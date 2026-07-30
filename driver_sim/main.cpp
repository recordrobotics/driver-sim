#define IMGUI_DEFINE_MATH_OPERATORS

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <argparse/argparse.hpp>

#include <blackboard_app/app.h>
#include <blackboard_app/gui.h>
#include <blackboard_app/resources.h>
#include <blackboard_app/window.h>
#include <blackboard_app/logger.h>

#include <bgfx/bgfx.h>
#include <imgui/imgui.h>

#include <algorithm>
#include <numeric>
#include <random>
#include <list>
#include <future>
#include <vector>
#include <string>

#include <fmt/ranges.h>

#include "ui/components.h"
#include "ui/theme.h"
#include "ui/transition.h"

#include "field/fieldrenderer.h"

#include "assets.h"

#include "fetch/storedasset.h"
#include "fetch/remotestoredasset.h"
#include "fetch/packagedstoredasset.h"

#include <code.zip.h>

#include "settings/settingsstore.h"
#include "process/processrunner.h"
#include "javalogmanager.h"

#include "discord.h"

using blackboard::gui::ImTexture;
using blackboard::gui::load_image;
using blackboard::gui::string_hex_to_rgba_float;
using namespace ui;
using namespace blackboard;

#define LOAD_FONT(font, set_as_default) \
  blackboard::gui::load_font(#font, (void *)font##_bytes, sizeof(font##_bytes), 12.0f, dpi, set_as_default);

blackboard::app::App *app_ptr;

ImTexture logo = {};

enum Page
{
  PAGE_LOADING,
  PAGE_SELECT,
  PAGE_3D_FIELD
};

ui::Transition pageTransition{PAGE_LOADING};

std::unique_ptr<RemoteStoredAsset, std::default_delete<RemoteStoredAsset>> javaAsset;
std::unique_ptr<RemoteStoredAsset, std::default_delete<RemoteStoredAsset>> dashboardAsset;
std::unique_ptr<RemoteStoredAsset, std::default_delete<RemoteStoredAsset>> fieldAsset;
std::unique_ptr<RemoteStoredAsset, std::default_delete<RemoteStoredAsset>> robotAsset;
std::unique_ptr<RemoteStoredAsset, std::default_delete<RemoteStoredAsset>> jniAsset;
std::unique_ptr<PackagedStoredAsset, std::default_delete<PackagedStoredAsset>> robotCodeAsset;
std::unique_ptr<ProcessRunner, std::default_delete<ProcessRunner>> elasticProcess;
std::unique_ptr<ProcessRunner, std::default_delete<ProcessRunner>> javaProcess;

std::shared_ptr<Discord> discord;

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

  const auto dpi{app_ptr->main_window->effective_display_resolution()};

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

  load_image((void *)logo_png_bytes, sizeof(logo_png_bytes), logo);

  field::init(*app_ptr->main_window);

  std::string prefPath = SDL_GetPrefPath(NULL, "DriverSim");
  javaAsset = std::make_unique<RemoteStoredAsset>("jdk", "8c7cfff78a55c56ebaf470ed6a89c6466b47d8274bdabdda997d7507c20325c5", prefPath, "https://api.adoptium.net/v3/binary/version/jdk-17.0.16%2B8/windows/x64/jdk/hotspot/normal/eclipse?project=jdk");
  dashboardAsset = std::make_unique<RemoteStoredAsset>("elastic", "6581e66eb237f9d615afb94077d89a03e2cdd7ce2d57f11c8cc5153821493ad7", prefPath, "https://github.com/Gold872/elastic_dashboard/releases/download/v2026.1.2/Elastic-Windows_portable.zip");
  fieldAsset = std::make_unique<RemoteStoredAsset>("field", "0f2abde864422367dd1bc3254da23b36a3d82eb727d5dac0a0f2231bdc397e31", prefPath, "https://github.com/Mechanical-Advantage/AdvantageScopeAssets/releases/download/archive-v1/Field3d_2026FRCFieldV1.zip");
  robotAsset = std::make_unique<RemoteStoredAsset>("robot", "b9d455ae13870531b35a6f87021d62feb606df146238b419c057af1c9a4d1462", prefPath, "https://hamster1.ddns.net/robot-b9d455ae13870531b35a6f87021d62feb606df146238b419c057af1c9a4d1462.zip");
  jniAsset = std::make_unique<RemoteStoredAsset>("jni", "0589a33fdf74cd58ef625dc2767956b260177de488ef89d8b17d60e250ee88c5", prefPath, "https://hamster1.ddns.net/jni-0589a33fdf74cd58ef625dc2767956b260177de488ef89d8b17d60e250ee88c5.zip");
  robotCodeAsset = std::make_unique<PackagedStoredAsset>("code", "7998021ca2a0f0d8867173cd7fcf8f4b15fb36d011d98df55b00bebb76732878", prefPath, std::span<const uint8_t>(code_zip_bytes, sizeof(code_zip_bytes)));

  // make sure we keep logs and settings
  robotCodeAsset->keepPaths = {"logs", "ctre_sim", "networktables.json"};

  fieldAsset->verifyOrDownload();
  robotAsset->verifyOrDownload();

  if (settings::launchElastic)
  {
    dashboardAsset->verifyOrDownload();
  }

  if (settings::launchRobotCode)
  {
    javaAsset->verifyOrDownload();
    jniAsset->verifyOrDownload();
    robotCodeAsset->verifyOrDownload();
  }

  javaLogEnforceFuture = std::async(std::launch::async, java_log_manager::enforceFolderLimits);

  discord = std::make_shared<Discord>();
}

bool hasInitializedFieldView = false;

void initFieldView()
{
  if (hasInitializedFieldView)
    return;
  hasInitializedFieldView = true;

  std::string prefPath = SDL_GetPrefPath(NULL, "DriverSim");

  elasticProcess = std::make_unique<ProcessRunner>(ProcessRunner::Config{
                                                       .commandLine = {prefPath + "elastic/elastic_dashboard.exe"},
                                                       .working_directory = prefPath + "elastic",
                                                       .environment = {},
                                                       .kill_parent_on_child_exit = true,
                                                       .auto_restart = false,
                                                       .use_existing_process = true /* elastic is single-instance (new instance exits immediately killing driver sim) */},
                                                   logger::logger);

  std::vector<std::string> javaCommandLine = {
      prefPath + "jdk/jdk-17.0.16+8/bin/java.exe",
      "-Djava.library.path=" + prefPath + "jni/release",
      "-jar", prefPath + "code/libs/2026-robot.jar"};
  javaCommandLine.insert(javaCommandLine.end() - 2, settings::jvmArguments.begin(), settings::jvmArguments.end()); // insert JVM arguments before the -jar argument
  javaCommandLine.insert(javaCommandLine.end(), settings::codeArguments.begin(), settings::codeArguments.end());   // insert code arguments at the end

  javaProcess = std::make_unique<ProcessRunner>(ProcessRunner::Config{.commandLine = javaCommandLine,
                                                                      .working_directory = prefPath + "code",
                                                                      .environment = {{"HALSIM_EXTENSIONS", std::accumulate(settings::enabledExtensions.begin(), settings::enabledExtensions.end(), std::string(), [prefPath](const std::string &acc, const std::string &ext)
                                                                                                                            { return acc + prefPath + "jni/release/" + ext + ".dll;"; })}},
                                                                      .kill_parent_on_child_exit = false,
                                                                      .auto_restart = true,
                                                                      .use_existing_process = false},
                                                logger::logger);

  if (settings::launchElastic)
  {
    elasticProcess->start();
  }

  if (settings::launchRobotCode)
  {
    javaProcess->start();
  }

  field::startNTClient();

  field::setRestartSimulationCallback([]()
                                      {
                                        if (settings::launchRobotCode)
                                        {
                                          javaProcess->restart();
                                        } });
}

void drawBackground()
{
  const ImVec2 windowPos = ImGui::GetWindowPos();
  const ImVec2 windowSize = ImGui::GetWindowSize();
  const ImVec2 windowMax(windowPos.x + windowSize.x, windowPos.y + windowSize.y);

  ImGui::GetWindowDrawList()->AddRectFilled(
      windowPos,
      windowMax,
      ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_WindowBg]));
}

void drawHeader()
{
  auto &style{ImGui::GetStyle()};
  float globalScale = style.FontScaleMain * style.FontScaleDpi;
  ImVec2 winSize = ImGui::GetWindowSize();

  const ImVec2 logoSize(70.0f * globalScale, 70.0f * globalScale);
  const float contentHeight = pageTransition.getCurrentPage() == PAGE_LOADING ? 450.0f * globalScale : 270.0f * globalScale;
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
}

inline std::string getProgressName(const std::string &assetName, AssetState state, std::string error)
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
  DrawProgress(getProgressName(assetName, asset.getState(), asset.getError()), asset.getProgress() / 100.0f, asset.getState() == AssetState::Error);
}

void drawPageLoading()
{
  auto &style{ImGui::GetStyle()};
  float globalScale = style.FontScaleMain * style.FontScaleDpi;

  if (fieldAsset->getState() == AssetState::Complete)
  {
    field::startLoadFieldModel();
  }

  if (robotAsset->getState() == AssetState::Complete)
  {
    field::startLoadRobotModel();
  }

  if (
      fieldAsset->getState() == AssetState::Complete &&
      robotAsset->getState() == AssetState::Complete &&
      (!settings::launchElastic || dashboardAsset->getState() == AssetState::Complete) &&
      (!settings::launchRobotCode || (javaAsset->getState() == AssetState::Complete &&
                                      jniAsset->getState() == AssetState::Complete &&
                                      robotCodeAsset->getState() == AssetState::Complete)))
  {
    pageTransition.transition(settings::showSelectPage ? PAGE_SELECT : PAGE_3D_FIELD,
                              // instant transition if all assets were quick loaded
                              fieldAsset->isQuickLoaded() &&
                                  robotAsset->isQuickLoaded() &&
                                  (!settings::launchElastic || dashboardAsset->isQuickLoaded()) &&
                                  (!settings::launchRobotCode || (javaAsset->isQuickLoaded() &&
                                                                  jniAsset->isQuickLoaded() &&
                                                                  robotCodeAsset->isQuickLoaded())));
    if (!settings::showSelectPage)
    {
      initFieldView();
    }
  }

  ImGui::Dummy(ImVec2(0, 22 * globalScale));

  if (settings::launchRobotCode)
  {
    drawAssetProgress("Java 17", *javaAsset);
  }
  if (settings::launchElastic)
  {
    drawAssetProgress("Elastic Dashboard", *dashboardAsset);
  }
  drawAssetProgress("Field Model", *fieldAsset);
  drawAssetProgress("Robot Model", *robotAsset);
  if (settings::launchRobotCode)
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

  bool driverStationSelected = settings::enabledExtensions.contains("halsim_ds_socket");
  bool simGuiSelected = settings::enabledExtensions.contains("halsim_gui");

  SplitToggleButtonGroup({
      {"Driver Station", &driverStationSelected},
      {"Sim GUI", &simGuiSelected},
  });

  if (driverStationSelected)
  {
    settings::enabledExtensions.insert("halsim_ds_socket");
  }
  else
  {
    settings::enabledExtensions.erase("halsim_ds_socket");
  }

  if (simGuiSelected)
  {
    settings::enabledExtensions.insert("halsim_gui");
  }
  else
  {
    settings::enabledExtensions.erase("halsim_gui");
  }

  ImGui::Dummy(ImVec2(0, 17 * globalScale));

  if (UnderlineTextButton("Don't show again"))
  {
    settings::showSelectPage = false;
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
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground};

  const ImGuiViewport *viewport{ImGui::GetMainViewport()};
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  ImGui::Begin("DriverSim", nullptr, pageTransition.getCurrentPage() == PAGE_3D_FIELD ? (window_flags | ImGuiWindowFlags_NoInputs) : window_flags);

  pageTransition.update();

  if (pageTransition.getCurrentPage() != PAGE_3D_FIELD)
  {
    drawBackground();
    drawHeader();
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
  ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 10, viewport->WorkPos.y + 10), ImGuiCond_Always, ImVec2(1.0f, 0.0f));

  const static ImGuiWindowFlags window_flags{
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize};

  ImGui::Begin("FPS", nullptr, window_flags);
  ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#ffffffc4"));
  ImGui::Text("%.1f FPS", io.Framerate);
  ImGui::Text(rendererApiToString(app_ptr->get_renderer_api()).c_str());
  ImGui::PopStyleColor();
  ImGui::End();
}

void app_update()
{
  discord->update();

  if (pageTransition.getCurrentPage() == PAGE_3D_FIELD)
  {
    field::render(*app_ptr->main_window, discord);
  }

  drawUI();
  drawFPS();
}

void app_cleanup()
{
  javaLogEnforceFuture = std::async(std::launch::async, java_log_manager::enforceFolderLimits);
  logo.destroy();
  field::cleanup();
  settings::saveSettings();
  javaLogEnforceFuture.wait();
}

#ifdef _WIN32
int WINAPI WinMain(
    HINSTANCE,
    HINSTANCE,
    LPSTR,
    int)
#else
int main(int argc, char *argv[])
#endif
{
  std::string api;

  argparse::ArgumentParser program("driver_sim", "1.0", argparse::default_arguments::none, false);
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

  settings::loadSettings();

  if (api == "auto")
  {
    api = settings::renderApi;
  }
  else if (api != settings::renderApi)
  {
    logger::logger->info("Updating render API from {} to {}", settings::renderApi, api);
    settings::renderApi = api;
    settings::saveSettings();
  }

  blackboard::app::App app("Driver Sim",
                           getRendererApiFromString(api));
  app_ptr = &app;
  app.on_update = app_update;
  app.on_init = initApp;
  app.run();

  app_cleanup();

  return 0;
}
