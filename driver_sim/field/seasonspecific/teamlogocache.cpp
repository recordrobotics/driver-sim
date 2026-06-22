#include "teamlogocache.h"

#include <blackboard_app/logger.h>
#include <blackboard_app/gui.h>
#include <cpr/cpr.h>
#include <SDL3/SDL.h>
#include <stop_token>
#include <fstream>

#include <bgfx/bgfx.h>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/file.h>
#include <bx/error.h>
#include <bx/pixelformat.h>

#include <blackboard_app/platform/imgui_impl_sdl_bgfx.h>

#include <teamplaceholder.png.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

using namespace blackboard::logger;

static bx::DefaultAllocator allocator;

TeamLogoCache::TeamLogoCache()
{
    logoCacheDirectory = std::filesystem::path(SDL_GetPrefPath(NULL, "DriverSim")) / "team_logos";
    if (!std::filesystem::exists(logoCacheDirectory))
    {
        if (!std::filesystem::create_directories(logoCacheDirectory))
        {
            logger->error("Failed to create logo cache directory: {}", logoCacheDirectory.string());
        }
    }

    logger->info("Loading team placeholder");
    blackboard::gui::load_image((void *)teamplaceholder_png_bytes, sizeof(teamplaceholder_png_bytes), placeholderTexture, BGFX_SAMPLER_POINT);
}

TeamLogoCache::~TeamLogoCache()
{
    for (auto &[teamNumber, thread] : workerThreads)
    {
        if (thread.joinable())
        {
            thread.request_stop();
            thread.join();
        }
    }

    for (auto &[teamNumber, texture] : logoCache)
    {
        texture.destroy();
    }
}

bimg::ImageContainer *readImage(const char *path)
{
    bx::FileReader reader;
    bx::Error err;

    if (bx::open(&reader, path, &err))
    {
        uint32_t size = (uint32_t)bx::getSize(&reader);

        char *data = (char *)bx::alloc(&allocator, size);
        bx::read(&reader, data, size, &err);

        bimg::ImageContainer *image = bimg::imageParse(
            &allocator,
            data,
            size,
            bimg::TextureFormat::BGRA8);

        if (image == nullptr)
        {
            logger->error("Could not load image");
        }

        bx::close(&reader);
        bx::free(&allocator, data);
        return image;
    }
    else
    {
        logger->error("Could not open image file: {0}, error: {1}", path, err.getMessage().getCPtr());
        return nullptr;
    }
}

bool processImage(int teamNumber, bimg::ImageContainer *image, std::unordered_map<int, blackboard::gui::ImTexture> &logoCache)
{
    logger->info("Creating texture for team {}", teamNumber);

    bgfx::TextureHandle textureHandle = bgfx::createTexture2D(
        static_cast<uint16_t>(image->m_width),
        static_cast<uint16_t>(image->m_height),
        false,
        1,
        bgfx::TextureFormat::BGRA8,
        BGFX_SAMPLER_POINT,
        bgfx::copy(image->m_data, image->m_size));

    bimg::imageFree(image);

    if (!bgfx::isValid(textureHandle))
    {
        logger->error("Could not create texture");
        return false;
    }

    ImTextureID texture = blackboard::renderer::toId(
        textureHandle,
        IMGUI_FLAGS_ALPHA_BLEND,
        0);

    logoCache[teamNumber] = blackboard::gui::ImTexture{.handle = textureHandle, .id = texture};
    return true;
}

blackboard::gui::ImTexture TeamLogoCache::getTeamLogo(int teamNumber)
{
    if (!logoStates.contains(teamNumber))
    {
        logoStates[teamNumber] = LogoState::NotLoaded;
    }

    if (logoStates[teamNumber] == LogoState::Loaded)
    {
        return logoCache[teamNumber];
    }
    else if (logoStates[teamNumber] == LogoState::NotLoaded)
    {
        logoStates[teamNumber] = LogoState::Loading;

        std::filesystem::path localPath = logoCacheDirectory / STR(GAME_YEAR) / (std::to_string(teamNumber) + ".png");
        std::string remoteUrl = "https://www.thebluealliance.com/avatar/" STR(GAME_YEAR) "/frc" + std::to_string(teamNumber) + ".png";

        // Load the logo asynchronously
        workerThreads[teamNumber] = std::jthread([this, teamNumber, localPath, remoteUrl](std::stop_token stop_token)
                                                 {
            bx::DefaultAllocator s_allocator;

            if(std::filesystem::exists(localPath))
            {
                logger->info("Loading team {} logo from cache.", teamNumber);
                bimg::ImageContainer *image = readImage(localPath.string().c_str());
                if (image)
                {
                    std::lock_guard<std::mutex> lock(processQueueMutex);
                    processQueue.push({teamNumber, image});
                    return;
                } else {
                    logger->error("Failed to load cached logo for team {}.", teamNumber);
                    std::filesystem::remove(localPath);
                    logoStates[teamNumber] = LogoState::Error;
                }
            }
            
            logger->info("Starting download of team logo {} from URL: {}", teamNumber, remoteUrl);

            if(!std::filesystem::exists(localPath.parent_path()))
            {
                std::filesystem::create_directories(localPath.parent_path());
            }

            std::ofstream ofs(localPath, std::ios::binary | std::ios::trunc);
            if (!ofs)
            {
                logger->error("Could not open local file for writing: {}", localPath.string());
                logoStates[teamNumber] = LogoState::Error;
                return;
            }

            cpr::Response r = cpr::Download(
                ofs,
                cpr::Url{remoteUrl},
                cpr::ProgressCallback([this, &stop_token, remoteUrl](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow,
                                                    cpr::cpr_off_t /*uploadTotal*/, cpr::cpr_off_t /*uploadNow*/,
                                                    intptr_t /*userdata*/) -> bool
                                    {
                    if (stop_token.stop_requested())
                    {
                        logger->info("Download cancelled by engine during progress callback: {}", remoteUrl);
                        return false;
                    }

                    if (downloadTotal > 0)
                    {
                        logger->trace("Download progress: {}% ({} / {})", static_cast<int>((downloadNow * 100) / downloadTotal), downloadNow, downloadTotal);
                    }

                    return true; // Continue downloading
                                    }));

            ofs.close();

            if (stop_token.stop_requested())
            {
                logger->info("Download cancelled by engine: {}", remoteUrl);
                std::filesystem::remove(localPath); // Delete the partial file
                logoStates[teamNumber] = LogoState::Error;
                return;
            }

            if (r.status_code == 200)
            {
                logger->info("Download complete for URL: {}", remoteUrl);
                bimg::ImageContainer *image = readImage(localPath.string().c_str());
                if (image)
                {
                    std::lock_guard<std::mutex> lock(processQueueMutex);
                    processQueue.push({teamNumber, image});
                } else {
                    logger->error("Failed to read downloaded image for team {}.", teamNumber);
                    std::filesystem::remove(localPath);
                    logoStates[teamNumber] = LogoState::Error;
                }
            }
            else
            {
                logger->error("Failed to download team logo from {}: HTTP {} - {}", remoteUrl, r.status_code, r.error.message);
                std::filesystem::remove(localPath); // Delete potential files

                if (r.error.code != cpr::ErrorCode::OK)
                {
                    logger->error("Network Error: {} ({})", r.error.message, remoteUrl);
                }
                else
                {
                    logger->error("HTTP Error {} from: {}", std::to_string(r.status_code), remoteUrl);
                }

                logoStates[teamNumber] = LogoState::Error;
            } });
    }

    return placeholderTexture;
}

void TeamLogoCache::update()
{
    std::lock_guard<std::mutex> lock(processQueueMutex);
    while (!processQueue.empty())
    {
        auto [teamNumber, image] = processQueue.front();
        processQueue.pop();

        if (image == nullptr)
        {
            logger->info("Using placeholder for team {}", teamNumber);
            logoCache[teamNumber] = placeholderTexture;
            logoStates[teamNumber] = LogoState::Loaded;
            continue;
        }

        if (processImage(teamNumber, image, logoCache))
        {
            logoStates[teamNumber] = LogoState::Loaded;
            logger->info("Team {} logo loaded successfully.", teamNumber);
        }
        else
        {
            logoStates[teamNumber] = LogoState::Error;
            logger->error("Failed to process logo for team {}.", teamNumber);
        }
    }
}