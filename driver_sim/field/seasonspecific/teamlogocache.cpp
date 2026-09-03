#include "teamlogocache.h"

#include <SDL3/SDL.h>
#include <blackboard_app/gui.h>
#include <blackboard_app/logger.h>
#include <cpr/cpr.h>
#include <fstream>
#include <stop_token>

#include <bgfx/bgfx.h>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <bx/error.h>
#include <bx/file.h>
#include <bx/pixelformat.h>

#include <blackboard_app/platform/imgui_impl_sdl_bgfx.h>

#include "../../manifest.h"
#include <teamplaceholder.png.h>

using namespace blackboard::logger;

static bx::DefaultAllocator allocator;

TeamLogoCache::TeamLogoCache()
{
    logoCacheDirectory =
        std::filesystem::path(SDL_GetPrefPath(nullptr, "DriverSim")) / "team_logos";
    if (!std::filesystem::exists(logoCacheDirectory))
    {
        if (!std::filesystem::create_directories(logoCacheDirectory))
        {
            logger->error("Failed to create logo cache directory: {}", logoCacheDirectory.string());
        }
    }

    logger->info("Loading team placeholder");
    blackboard::gui::load_image(static_cast<const void *>(teamplaceholder_png_bytes),
                                sizeof(teamplaceholder_png_bytes), placeholderTexture,
                                BGFX_SAMPLER_POINT);
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

    placeholderTexture.destroy();
}

bool isPng(const void *data, size_t size)
{
    static constexpr uint8_t pngHeader[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

    return size >= sizeof(pngHeader) && std::memcmp(data, pngHeader, sizeof(pngHeader)) == 0;
}

bimg::ImageContainer *readImage(const char *path)
{
    bx::FileReader reader;
    bx::Error err;

    if (bx::open(&reader, path, &err))
    {
        auto size = static_cast<int32_t>(bx::getSize(&reader));

        if (size == 0)
        {
            logger->error("Image file is empty: {}", path);
            bx::close(&reader);
            return nullptr;
        }

        auto *data = static_cast<char *>(bx::alloc(&allocator, size));
        bx::read(&reader, data, size, &err);

        if (!isPng(data, size))
        {
            logger->error("Image file is not a valid PNG: {}", path);
            bx::close(&reader);
            bx::free(&allocator, data);
            return nullptr;
        }

        bimg::ImageContainer *image =
            bimg::imageParse(&allocator, data, size, bimg::TextureFormat::BGRA8);

        if (image == nullptr)
        {
            logger->error("Could not load image");
        }

        bx::close(&reader);
        bx::free(&allocator, data);
        return image;
    }

    logger->error("Could not open image file: {0}, error: {1}", path, err.getMessage().getCPtr());
    return nullptr;
}

bool processImage(int teamNumber, bimg::ImageContainer *image,
                  std::unordered_map<int, blackboard::gui::ImTexture> &logoCache)
{
    logger->info("Creating texture for team {}", teamNumber);

    bgfx::TextureHandle textureHandle = bgfx::createTexture2D(
        static_cast<uint16_t>(image->m_width), static_cast<uint16_t>(image->m_height), false, 1,
        bgfx::TextureFormat::BGRA8, BGFX_SAMPLER_POINT, bgfx::copy(image->m_data, image->m_size));

    bimg::imageFree(image);

    if (!bgfx::isValid(textureHandle))
    {
        logger->error("Could not create texture");
        return false;
    }

    ImTextureID texture = blackboard::renderer::toId(textureHandle, IMGUI_FLAGS_ALPHA_BLEND, 0);

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
    if (logoStates[teamNumber] == LogoState::NotLoaded)
    {
        logoStates[teamNumber] = LogoState::Loading;

        std::filesystem::path localPath = logoCacheDirectory / Manifest::getCurrent().getTbaYear() /
                                          (std::to_string(teamNumber) + ".png");
        std::string remoteUrl = "https://www.thebluealliance.com/avatar/" +
                                Manifest::getCurrent().getTbaYear() + "/frc" +
                                std::to_string(teamNumber) + ".png";

        // Load the logo asynchronously
        workerThreads[teamNumber] = std::jthread(
            [this, teamNumber, localPath, remoteUrl](std::stop_token stop_token)
            {
                bx::DefaultAllocator s_allocator;

                if (std::filesystem::exists(localPath))
                {
                    logger->info("Loading team {} logo from cache.", teamNumber);
                    bimg::ImageContainer *image = readImage(localPath.string().c_str());
                    if (image)
                    {
                        std::scoped_lock lock(processQueueMutex);
                        processQueue.emplace(teamNumber, image);
                        return;
                    }

                    logger->error("Failed to load cached logo for team {}.", teamNumber);
                    try
                    {
                        std::filesystem::remove(localPath);
                    }
                    catch (const std::filesystem::filesystem_error &e)
                    {
                        logger->error("Failed to delete corrupted image file {}: {}",
                                      localPath.string(), e.what());
                    }
                    logoStates[teamNumber] = LogoState::Error;
                }

                logger->info("Starting download of team logo {} from URL: {}", teamNumber,
                             remoteUrl);

                if (!std::filesystem::exists(localPath.parent_path()))
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

                cpr::Response response = cpr::Download(
                    ofs, cpr::Url{remoteUrl},
                    cpr::ProgressCallback(
                        [this, &stop_token,
                         remoteUrl](cpr::cpr_off_t downloadTotal, cpr::cpr_off_t downloadNow,
                                    cpr::cpr_off_t /*uploadTotal*/, cpr::cpr_off_t /*uploadNow*/,
                                    intptr_t /*userdata*/) -> bool
                        {
                            if (stop_token.stop_requested())
                            {
                                logger->info(
                                    "Download cancelled by engine during progress callback: {}",
                                    remoteUrl);
                                return false;
                            }

                            if (downloadTotal > 0)
                            {
                                logger->trace("Download progress: {}% ({} / {})",
                                              static_cast<int>((downloadNow * 100) / downloadTotal),
                                              downloadNow, downloadTotal);
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

                if (response.status_code == 200)
                {
                    logger->info("Download complete for URL: {}", remoteUrl);
                    bimg::ImageContainer *image = readImage(localPath.string().c_str());
                    if (image)
                    {
                        std::scoped_lock lock(processQueueMutex);
                        processQueue.emplace(teamNumber, image);
                    }
                    else
                    {
                        logger->error("Failed to read downloaded image for team {}.", teamNumber);
                        try
                        {
                            std::filesystem::remove(localPath);
                        }
                        catch (const std::filesystem::filesystem_error &e)
                        {
                            logger->error("Failed to delete corrupted image file {}: {}",
                                          localPath.string(), e.what());
                        }
                        logoStates[teamNumber] = LogoState::Error;
                    }
                }
                else
                {
                    logger->error("Failed to download team logo from {}: HTTP {} - {}", remoteUrl,
                                  response.status_code, response.error.message);
                    try
                    {
                        std::filesystem::remove(localPath); // Delete potential files
                    }
                    catch (const std::filesystem::filesystem_error &e)
                    {
                        logger->error("Failed to delete corrupted image file {}: {}",
                                      localPath.string(), e.what());
                    }

                    if (response.error.code != cpr::ErrorCode::OK)
                    {
                        logger->error("Network Error: {} ({})", response.error.message, remoteUrl);
                    }
                    else
                    {
                        logger->error("HTTP Error {} from: {}",
                                      std::to_string(response.status_code), remoteUrl);
                    }
                    try
                    {
                        std::filesystem::remove(localPath); // Delete potential files
                    }
                    catch (const std::filesystem::filesystem_error &e)
                    {
                        logger->error("Failed to delete corrupted image file {}: {}",
                                      localPath.string(), e.what());
                    }

                    logoStates[teamNumber] = LogoState::Error;
                }
            });
    }

    return placeholderTexture;
}

void TeamLogoCache::update()
{
    std::scoped_lock lock(processQueueMutex);
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