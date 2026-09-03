#define IMGUI_DEFINE_MATH_OPERATORS

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <argparse/argparse.hpp>

#include <blackboard_app/app.h>
#include <blackboard_app/gui.h>
#include <blackboard_app/logger.h>
#include <blackboard_app/platform/imgui_impl_sdl_bgfx.h>
#include <blackboard_app/resources.h>
#include <blackboard_app/window.h>

#include <bgfx/bgfx.h>
#include <imgui/imgui.h>

#include <algorithm>
#include <future>
#include <list>
#include <numeric>
#include <random>
#include <string>
#include <vector>

#include <fmt/ranges.h>

#include "ui/components.h"
#include "ui/theme.h"
#include "ui/transition.h"

#include "field/fieldrenderer.h"

#include "assets.h"

#include "fetch/packagedstoredasset.h"
#include "fetch/remotestoredasset.h"
#include "fetch/storedasset.h"

#include <code.zip.h>

#include "javalogmanager.h"
#include "process/processrunner.h"
#include "settings/settingsstore.h"
#include "settings/settingsui.h"

#include "discord.h"

#include "manifest.h"
#include "version.h"

using blackboard::gui::ImTexture;
using blackboard::gui::load_image;
using blackboard::gui::string_hex_to_rgba_float;
using namespace ui;
using namespace blackboard;

#define LOAD_FONT(font, set_as_default)                                                            \
    blackboard::gui::load_font(#font, static_cast<const void *>(font##_bytes),                     \
                               sizeof(font##_bytes), 12.0f, dpi, set_as_default);

blackboard::app::App *app_ptr;

ImTexture logo = {};

enum Page
{
    PAGE_LOADING,
    PAGE_SELECT,
    PAGE_3D_FIELD
};

ui::Transition pageTransition{PAGE_LOADING};

std::unique_ptr<StoredAsset> javaAsset;
std::unique_ptr<StoredAsset> dashboardAsset;
std::unique_ptr<StoredAsset> fieldAsset;
std::unique_ptr<StoredAsset> robotAsset;
std::unique_ptr<StoredAsset> jniAsset;
std::unique_ptr<StoredAsset> robotCodeAsset;
std::unique_ptr<ProcessRunner> elasticProcess;
std::unique_ptr<ProcessRunner> javaProcess;

std::shared_ptr<Discord> discord;

std::shared_ptr<FieldRenderer> fieldRenderer;

std::future<void> javaLogEnforceFuture;

blackboard::renderer::Api getRendererApiFromString(const std::string &api)
{
    if (api == "metal")
    {
        return blackboard::renderer::Api::METAL;
    }
    else if (api == "d3d11")
    {
        return blackboard::renderer::Api::D3D11;
    }
    else if (api == "d3d12")
    {
        return blackboard::renderer::Api::D3D12;
    }
    else if (api == "vulkan")
    {
        return blackboard::renderer::Api::VULKAN;
    }
    else if (api == "opengl")
    {
        return blackboard::renderer::Api::OPENGL;
    }
    else
    {
        return blackboard::renderer::Api::AUTO;
    }
}

std::string rendererApiToString(blackboard::renderer::Api api)
{
    switch (api)
    {
    case blackboard::renderer::Api::METAL:
        return "Metal";
    case blackboard::renderer::Api::D3D11:
        return "Direct3D 11";
    case blackboard::renderer::Api::D3D12:
        return "Direct3D 12";
    case blackboard::renderer::Api::VULKAN:
        return "Vulkan";
    case blackboard::renderer::Api::OPENGL:
        return "OpenGL";
    case blackboard::renderer::Api::WEBGL:
        return "WebGL";
    default:
        return "Unknown";
    }
}

void initApp()
{
    set_theme();

    const auto dpi{app_ptr->get_main_window()->effective_display_resolution()};

    LOAD_FONT(Inter_Thin_otf, false);
    LOAD_FONT(Inter_ThinItalic_otf, false);
    LOAD_FONT(Inter_ExtraLight_otf, false);
    LOAD_FONT(Inter_ExtraLightItalic_otf, false);
    LOAD_FONT(Inter_Light_otf, false);
    LOAD_FONT(Inter_LightItalic_otf, false);
    LOAD_FONT(Inter_Regular_otf, true);
    LOAD_FONT(Inter_Italic_otf, false);
    LOAD_FONT(Inter_Medium_otf, false);
    LOAD_FONT(Inter_MediumItalic_otf, false);
    LOAD_FONT(Inter_SemiBold_otf, false);
    LOAD_FONT(Inter_SemiBoldItalic_otf, false);
    LOAD_FONT(Inter_Bold_otf, false);
    LOAD_FONT(Inter_BoldItalic_otf, false);
    LOAD_FONT(Inter_ExtraBold_otf, false);
    LOAD_FONT(Inter_ExtraBoldItalic_otf, false);
    LOAD_FONT(Inter_Black_otf, false);
    LOAD_FONT(Inter_BlackItalic_otf, false);

    LOAD_FONT(Roboto_Bold_ttf, false);

    load_image(static_cast<const void *>(logo_png_bytes), sizeof(logo_png_bytes), logo);

    fieldRenderer = std::make_shared<FieldRenderer>(*app_ptr->get_main_window());

    settings::init(logo);

    Manifest &manifest = Manifest::getCurrent();

    std::string prefPath = SDL_GetPrefPath(nullptr, "DriverSim");
    javaAsset = std::make_unique<RemoteStoredAsset>("jdk", manifest.getJdkHash(), prefPath,
                                                    manifest.getJdkDownloadUrl());
    dashboardAsset = std::make_unique<RemoteStoredAsset>(
        "elastic", manifest.getElasticHash(), prefPath, manifest.getElasticDownloadUrl());
    fieldAsset = std::make_unique<RemoteStoredAsset>("field", manifest.getFieldHash(), prefPath,
                                                     manifest.getFieldDownloadUrl());
    robotAsset = std::make_unique<RemoteStoredAsset>("robot", manifest.getRobotAssetHash(),
                                                     prefPath, manifest.getRobotAssetDownloadUrl());
    jniAsset = std::make_unique<RemoteStoredAsset>("jni", manifest.getJniHash(), prefPath,
                                                   manifest.getJniDownloadUrl());
    if (manifest.getRobotCodeDownloadUrl().empty())
    {
        robotCodeAsset = std::make_unique<PackagedStoredAsset>(
            "code", manifest.getRobotCodeHash(), prefPath,
            std::span<const uint8_t>(code_zip_bytes, sizeof(code_zip_bytes)));
    }
    else
    {
        robotCodeAsset = std::make_unique<RemoteStoredAsset>(
            "code", manifest.getRobotCodeHash(), prefPath, manifest.getRobotCodeDownloadUrl());
    }

    // make sure we keep logs and settings
    robotCodeAsset->keepPaths = {"logs", "ctre_sim", "networktables.json"};

    fieldAsset->verifyOrDownload();
    robotAsset->verifyOrDownload();

    if (settings::current.launchElastic)
    {
        dashboardAsset->verifyOrDownload();
    }

    if (settings::current.launchRobotCode)
    {
        javaAsset->verifyOrDownload();
        jniAsset->verifyOrDownload();
        robotCodeAsset->verifyOrDownload();
    }

    javaLogEnforceFuture = std::async(std::launch::async, java_log_manager::enforceFolderLimits);

    discord = std::make_shared<Discord>();
}

bool hasInitializedFieldView = false;

std::vector<std::string> getJavaCommandLine()
{
    std::string prefPath = SDL_GetPrefPath(nullptr, "DriverSim");

    std::vector<std::string> javaCommandLine = {
        prefPath + "jdk/jdk-" + Manifest::getCurrent().getJdkVersion() + "/bin/java.exe",
        "-Djava.library.path=" + prefPath + "jni/release", "-jar",
        prefPath + "code/libs/" + Manifest::getCurrent().getRobotCodeJarName()};
    javaCommandLine.insert(
        javaCommandLine.end() - 2, settings::current.jvmArguments.begin(),
        settings::current.jvmArguments.end()); // insert JVM arguments before the -jar argument
    javaCommandLine.insert(
        javaCommandLine.end(), settings::current.codeArguments.begin(),
        settings::current.codeArguments.end()); // insert code arguments at the end

    return javaCommandLine;
}

void initFieldView()
{
    if (hasInitializedFieldView)
        return;
    hasInitializedFieldView = true;

    if (!fieldRenderer)
    {
        fieldRenderer = std::make_shared<FieldRenderer>(*app_ptr->get_main_window());
    }

    std::string prefPath = SDL_GetPrefPath(nullptr, "DriverSim");

    elasticProcess = std::make_unique<ProcessRunner>(ProcessRunner::Config{.commandLine =
                                                                               {prefPath +
                                                                                "elastic/"
                                                                                "elastic_dashboard."
                                                                                "exe"},
                                                                           .working_directory =
                                                                               prefPath + "elastic",
                                                                           .environment = {},
                                                                           .kill_parent_on_child_exit =
                                                                               true,
                                                                           .auto_restart = false,
                                                                           .use_existing_process = true /* elastic is single-instance (new instance exits immediately killing driver sim) */},
                                                     logger::logger);

    javaProcess = std::make_unique<ProcessRunner>(
        ProcessRunner::Config{
            .commandLine = getJavaCommandLine(),
            .working_directory = prefPath + "code",
            .environment = {{"HALSIM_EXTENSIONS",
                             std::accumulate(
                                 settings::current.enabledExtensions.begin(),
                                 settings::current.enabledExtensions.end(), std::string(),
                                 [prefPath](const std::string &acc, const std::string &ext)
                                 { return acc + prefPath + "jni/release/" + ext + ".dll;"; })}},
            .kill_parent_on_child_exit = false,
            .auto_restart = true,
            .use_existing_process = false},
        logger::logger);

    if (settings::current.launchElastic)
    {
        elasticProcess->start();
    }

    if (settings::current.launchRobotCode)
    {
        javaProcess->start();
    }

    fieldRenderer->startNTClient();

    fieldRenderer->setRestartSimulationCallback(
        []()
        {
            if (settings::current.launchRobotCode)
            {
                // update to include any changed settings
                javaProcess->updateCommandLine(getJavaCommandLine());
                javaProcess->restart();
            }
        });
}

void cleanupFieldView()
{
    if (!hasInitializedFieldView)
        return;
    hasInitializedFieldView = false;

    fieldRenderer.reset();

    if (settings::current.launchElastic)
    {
        elasticProcess.reset();
    }

    if (settings::current.launchRobotCode)
    {
        javaProcess.reset();
    }
}

void drawBackground()
{
    const ImVec2 windowPos = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const ImVec2 windowMax(windowPos.x + windowSize.x, windowPos.y + windowSize.y);

    ImGui::GetWindowDrawList()->AddRectFilled(
        windowPos, windowMax,
        ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_WindowBg]));
}

void drawHeader()
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;
    ImVec2 winSize = ImGui::GetWindowSize();

    const ImVec2 logoSize(70.0f * globalScale, 70.0f * globalScale);
    const float contentHeight = pageTransition.getCurrentPage() == PAGE_LOADING
                                    ? 450.0f * globalScale
                                    : 270.0f * globalScale;
    const float logoTopY = std::max(0.0f, (winSize.y - logoSize.y - contentHeight) * 0.5f);

    ImGui::SetCursorPosY(logoTopY);

    // Logo
    ImGui::SetCursorPosX((winSize.x - logoSize.x) / 2);
    if (logo.id)
    {
        ImGui::Image(logo.id, logoSize);
    }
    else
    {
        ImGui::Dummy(logoSize);
    }

    ImGui::Dummy(ImVec2(0, 6 * globalScale));

    ImGui::PushFont(nullptr, 35.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#8F8686ff"));
    DrawCenteredText("Driver Sim");
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void drawFooter()
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;
    float height = std::max(ImGui::GetCursorPosY() + 40 * globalScale, ImGui::GetWindowSize().y);

    ImGui::SetCursorPosY(height - 30 * globalScale);
    ImGui::PushFont(nullptr, 10.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#858585ff"));
    DrawCenteredText("Made by Record Robotics");
    ImGui::PopStyleColor();
    ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 10 * globalScale));
}

inline std::string getProgressName(const std::string &assetName, AssetState state,
                                   std::string error)
{
    switch (state)
    {
    case AssetState::Verifying:
        return "Verifying " + assetName + "...";
    case AssetState::Downloading:
        return "Downloading " + assetName + "...";
    case AssetState::Writing:
        return "Writing " + assetName + "...";
    case AssetState::Extracting:
        return "Extracting " + assetName + "...";
    case AssetState::Cleanup:
        return "Finalizing " + assetName + "...";
    case AssetState::Complete:
        return assetName + " is ready!";
    case AssetState::Error:
        return "Error (" + error + ") " + assetName;
    default:
        return "Unknown " + assetName;
    }
}

inline void drawAssetProgress(const std::string &assetName, StoredAsset &asset)
{
    DrawProgress(getProgressName(assetName, asset.getState(), asset.getError()),
                 asset.getProgress() / 100.0f, asset.getState() == AssetState::Error);
}

void drawPageLoading()
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;

    if (fieldAsset->getState() == AssetState::Complete &&
        robotAsset->getState() == AssetState::Complete &&
        (!settings::current.launchElastic || dashboardAsset->getState() == AssetState::Complete) &&
        (!settings::current.launchRobotCode ||
         (javaAsset->getState() == AssetState::Complete &&
          jniAsset->getState() == AssetState::Complete &&
          robotCodeAsset->getState() == AssetState::Complete)))
    {
        pageTransition.transition(
            settings::current.showMainMenu ? PAGE_SELECT : PAGE_3D_FIELD,
            // instant transition if all assets were quick loaded
            fieldAsset->isQuickLoaded() && robotAsset->isQuickLoaded() &&
                (!settings::current.launchElastic || dashboardAsset->isQuickLoaded()) &&
                (!settings::current.launchRobotCode ||
                 (javaAsset->isQuickLoaded() && jniAsset->isQuickLoaded() &&
                  robotCodeAsset->isQuickLoaded())));
        if (!settings::current.showMainMenu)
        {
            initFieldView();
        }
    }

    ImGui::Dummy(ImVec2(0, 22 * globalScale));

    if (settings::current.launchRobotCode)
    {
        std::string javaMajorVersion = Manifest::getCurrent().getJdkVersion().substr(
            0, Manifest::getCurrent().getJdkVersion().find('.'));
        drawAssetProgress("Java " + javaMajorVersion, *javaAsset);
    }
    if (settings::current.launchElastic)
    {
        drawAssetProgress("Elastic Dashboard", *dashboardAsset);
    }
    drawAssetProgress("Field Model", *fieldAsset);
    drawAssetProgress("Robot Model", *robotAsset);
    if (settings::current.launchRobotCode)
    {
        drawAssetProgress("JNI Libraries", *jniAsset);
        drawAssetProgress("Robot Code", *robotCodeAsset);
    }
}

void drawPageSelect()
{
    auto &style{ImGui::GetStyle()};
    float globalScale = style.FontScaleMain * style.FontScaleDpi;
    ImVec2 winSize = ImGui::GetWindowSize();

    ImGui::Dummy(ImVec2(0, 16 * globalScale));

    ImGui::PushFont(nullptr, 16.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#BBBBBBff"));
    DrawCenteredText("Choose extensions to run");
    ImGui::PopStyleColor();
    ImGui::PopFont();

    ImGui::Dummy(ImVec2(0, 17 * globalScale));

    bool driverStationSelected = settings::current.enabledExtensions.contains("halsim_ds_socket");
    bool simGuiSelected = settings::current.enabledExtensions.contains("halsim_gui");

    SplitToggleButtonGroup({
        {"Driver Station", &driverStationSelected},
        {"Sim GUI", &simGuiSelected},
    });

    if (driverStationSelected)
    {
        settings::current.enabledExtensions.insert("halsim_ds_socket");
    }
    else
    {
        settings::current.enabledExtensions.erase("halsim_ds_socket");
    }

    if (simGuiSelected)
    {
        settings::current.enabledExtensions.insert("halsim_gui");
    }
    else
    {
        settings::current.enabledExtensions.erase("halsim_gui");
    }

    ImGui::Dummy(ImVec2(0, 17 * globalScale));

    if (UnderlineTextButton("Don't show again"))
    {
        settings::current.showMainMenu = false;
        settings::saveSettings();
        pageTransition.transition(PAGE_3D_FIELD);
        initFieldView();
    }

    ImGui::Dummy(ImVec2(0, 20 * globalScale));

    ImGui::SetCursorPosX((winSize.x - 28 * 2 * globalScale) / 2);
    if (CircularButton("go", 28 * globalScale))
    {
        settings::saveSettings();
        pageTransition.transition(PAGE_3D_FIELD);
        initFieldView();
    }
}

void drawUI()
{
    const static ImGuiWindowFlags window_flags{
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground};

    const ImGuiViewport *viewport{ImGui::GetMainViewport()};
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("DriverSim", nullptr,
                 pageTransition.getCurrentPage() == PAGE_3D_FIELD
                     ? (window_flags | ImGuiWindowFlags_NoInputs)
                     : window_flags);

    pageTransition.update();

    if (pageTransition.getCurrentPage() != PAGE_3D_FIELD)
    {
        drawBackground();
        drawHeader();
    }

    if (fieldRenderer)
    {
        if (fieldAsset->getState() == AssetState::Complete)
        {
            fieldRenderer->startLoadFieldModel();
        }

        if (robotAsset->getState() == AssetState::Complete)
        {
            fieldRenderer->startLoadRobotModel();
        }
    }

    if (pageTransition.getCurrentPage() == PAGE_LOADING)
    {
        drawPageLoading();
    }
    else if (pageTransition.getCurrentPage() == PAGE_SELECT)
    {
        drawPageSelect();
    }

    if (pageTransition.getCurrentPage() != PAGE_3D_FIELD)
    {
        drawFooter();
    }

    pageTransition.draw();

    ImGui::PopStyleVar(3);

    ImGui::End();
}

void drawFPS()
{
    ImGuiIO &io = ImGui::GetIO();
    const ImGuiViewport *viewport{ImGui::GetMainViewport()};
    ImGui::SetNextWindowPos(
        ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 10, viewport->WorkPos.y + 10),
        ImGuiCond_Always, ImVec2(1.0f, 0.0f));

    const static ImGuiWindowFlags window_flags{
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize};

    ImGui::Begin("FPS", nullptr, window_flags);
    ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#ffffffc4"));
    ImGui::Text("%.1f FPS", io.Framerate);
    ImGui::Text(rendererApiToString(app_ptr->get_renderer_api()).c_str());
    ImGui::PopStyleColor();
    ImGui::End();
}

void app_after_events()
{
    blackboard::app::Window *main_window = app_ptr->get_main_window();
    if (main_window->fullscreen != settings::current.fullscreen)
    {
        main_window->fullscreen = settings::current.fullscreen;
        SDL_SetWindowFullscreen(main_window->window, main_window->fullscreen);

        int drawable_width{0};
        int drawable_height{0};
        SDL_GetWindowSizeInPixels(main_window->window, &drawable_width, &drawable_height);
        bgfx::reset(drawable_width, drawable_height, blackboard::renderer::get_bgfx_reset_flags());
    }

    if (main_window->vsync != settings::current.enableVSync)
    {
        main_window->vsync = settings::current.enableVSync;

        if (main_window->vsync)
        {
            blackboard::renderer::set_bgfx_reset_flags(
                blackboard::renderer::get_bgfx_reset_flags() | BGFX_RESET_VSYNC);
        }
        else
        {
            blackboard::renderer::set_bgfx_reset_flags(
                blackboard::renderer::get_bgfx_reset_flags() & ~BGFX_RESET_VSYNC);
        }

        int drawable_width{0};
        int drawable_height{0};
        SDL_GetWindowSizeInPixels(main_window->window, &drawable_width, &drawable_height);
        bgfx::reset(drawable_width, drawable_height, blackboard::renderer::get_bgfx_reset_flags());
    }
}

void app_update()
{
    discord->update();

    if (fieldRenderer && pageTransition.getCurrentPage() == PAGE_3D_FIELD)
    {
        fieldRenderer->render(*app_ptr->get_main_window(), discord);
        if (fieldRenderer->isExiting())
        {
            pageTransition.transition(PAGE_SELECT);
        }
    }
    else if (fieldRenderer && fieldRenderer->isExiting())
    {
        cleanupFieldView();
    }

    drawUI();
    drawFPS();
}

void app_cleanup()
{
    javaLogEnforceFuture = std::async(std::launch::async, java_log_manager::enforceFolderLimits);
    logo.destroy();
    cleanupFieldView();
    settings::cleanup();
    settings::saveSettings();
    javaLogEnforceFuture.wait();
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#else
int main(int argc, char *argv[])
#endif
{
    std::string api;

    argparse::ArgumentParser program("driver_sim", DRIVERSIM_VERSION,
                                     argparse::default_arguments::none, false);
    program.add_argument("--api")
        .default_value("auto")
        .choices("auto", "metal", "d3d11", "d3d12", "vulkan", "opengl")
        .store_into(api);

    logger::init();

    try
    {
#ifdef _WIN32
        auto unknown_args = program.parse_known_args(__argc, __argv);
#else
        auto unknown_args = program.parse_known_args(argc, argv);
#endif

        if (!unknown_args.empty())
        {
            logger::logger->warn("Unknown arguments: {}", fmt::join(unknown_args, " "));
        }
    }
    catch (const std::exception &err)
    {
        logger::logger->error("Error parsing arguments: {}", err.what());
        logger::logger->info(program.help().str());
    }

    logger::logger->info("Driver Sim version: {} {}", DRIVERSIM_VERSION, DRIVERSIM_COMMIT);

    settings::loadSettings();

    if (api == "auto")
    {
        api = settings::current.renderApi;
    }
    else if (api != settings::current.renderApi)
    {
        logger::logger->info("Updating render API from {} to {}", settings::current.renderApi, api);
        settings::current.renderApi = api;
        settings::saveSettings();
    }

    blackboard::renderer::Api renderer_api = getRendererApiFromString(api);

#ifdef _WIN32
    // On Windows use Vulkan instead of D3D11 when Auto is selected due to visual glitches.
    if (renderer_api == blackboard::renderer::Api::AUTO)
    {
        renderer_api = blackboard::renderer::Api::VULKAN;
    }
#endif

    blackboard::app::App app("Driver Sim", renderer_api, initApp, app_update, app_after_events,
                             blackboard::app::Window::DEFAULT_WIDTH,
                             blackboard::app::Window::DEFAULT_HEIGHT, settings::current.fullscreen,
                             settings::current.enableVSync);
    app_ptr = &app;
    app.run();

    app_cleanup();

    return 0;
}
