#define IMGUI_DEFINE_MATH_OPERATORS

#include <blackboard_app/app.h>
#include <blackboard_app/gui.h>
#include <blackboard_app/resources.h>
#include <blackboard_app/window.h>

#include <bgfx/bgfx.h>
#include <imgui/imgui.h>

#include <algorithm>
#include <random>
#include <list>

#include "ui/components.h"
#include "ui/theme.h"
#include "ui/transition.h"

#include "field/fieldrenderer.h"

#include "assets.h"

using blackboard::gui::ImTexture;
using blackboard::gui::load_image;
using blackboard::gui::string_hex_to_rgba_float;
using namespace ui;

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

ui::Transition pageTransition{PAGE_3D_FIELD};

float javaProgress = 0.0f;
float elasticProgress = 0.0f;
float jniProgress = 0.0f;
float unpackProgress = 0.0f;
float javaSpeed = 0.0f;
float elasticSpeed = 0.0f;
float jniSpeed = 0.0f;
float unpackSpeed = 0.0f;
float javaRetargetTime = 0.0f;
float elasticRetargetTime = 0.0f;
float jniRetargetTime = 0.0f;
float unpackRetargetTime = 0.0f;

bool driverStationSelected = true;
bool simGuiSelected = false;

void init()
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

  load_image((void *)logo_png_bytes, sizeof(logo_png_bytes), logo);

  field::init(*app_ptr->main_window);
}

#pragma region Fake Progress Bar

void resetLoadingProgress()
{
  javaProgress = elasticProgress = jniProgress = unpackProgress = 0.0f;
  javaSpeed = elasticSpeed = jniSpeed = unpackSpeed = 0.0f;
  javaRetargetTime = elasticRetargetTime = jniRetargetTime = unpackRetargetTime = 0.0f;
}

float randomFloat(float minValue, float maxValue)
{
  thread_local std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<float> dist(minValue, maxValue);
  return dist(rng);
}

void retargetProgress(float &speed, float &retargetTime)
{
  speed = randomFloat(0.04f, 0.22f);
  retargetTime = randomFloat(0.15f, 0.90f);
}

void updateProgressValue(float &value, float &speed, float &retargetTime, float deltaTime)
{
  if (value >= 1.0f)
  {
    value = 1.0f;
    return;
  }

  retargetTime -= deltaTime;
  if (retargetTime <= 0.0f)
  {
    retargetProgress(speed, retargetTime);
  }

  value = std::min(1.0f, value + speed * deltaTime);
}

void updateLoadingProgress()
{
  const float deltaTime = ImGui::GetIO().DeltaTime;

  updateProgressValue(javaProgress, javaSpeed, javaRetargetTime, deltaTime);
  updateProgressValue(elasticProgress, elasticSpeed, elasticRetargetTime, deltaTime);
  updateProgressValue(jniProgress, jniSpeed, jniRetargetTime, deltaTime);
  updateProgressValue(unpackProgress, unpackSpeed, unpackRetargetTime, deltaTime);

  if (javaProgress >= 1.0f && elasticProgress >= 1.0f && jniProgress >= 1.0f && unpackProgress >= 1.0f)
  {
    pageTransition.transition(PAGE_SELECT);
  }
}

#pragma endregion

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
  const float logoTopY = (winSize.y - logoSize.y) * 0.5f;

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
  ImVec2 winSize = ImGui::GetWindowSize();

  ImGui::SetCursorPosY(winSize.y - 30 * globalScale);
  ImGui::PushFont(nullptr, 10.0f);
  ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#858585ff"));
  DrawCenteredText("Made by Record Robotics");
  ImGui::PopStyleColor();
  ImGui::PopFont();
}

void drawPageLoading()
{
  auto &style{ImGui::GetStyle()};
  float globalScale = style.FontScaleMain * style.FontScaleDpi;

  updateLoadingProgress();

  ImGui::Dummy(ImVec2(0, 22 * globalScale));

  DrawProgress("Downloading Java 17...", javaProgress);
  DrawProgress("Downloading Elastic Dashboard...", elasticProgress);
  DrawProgress("Downloading JNI Libraries...", jniProgress);
  DrawProgress("Unpacking robot code...", unpackProgress);
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

  SplitToggleButtonGroup({
      {"Driver Station", &driverStationSelected},
      {"Sim GUI", &simGuiSelected},
  });

  ImGui::Dummy(ImVec2(0, 17 * globalScale));

  if (UnderlineTextButton("Don't show again"))
  {
    resetLoadingProgress();
    pageTransition.transition(PAGE_LOADING);
  }

  ImGui::Dummy(ImVec2(0, 20 * globalScale));

  ImGui::SetCursorPosX((winSize.x - 28 * 2 * globalScale) / 2);
  if (CircularButton("go", 28 * globalScale))
  {
    resetLoadingProgress();
    pageTransition.transition(PAGE_3D_FIELD);
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

void app_update()
{
  if (pageTransition.getCurrentPage() == PAGE_3D_FIELD)
  {
    field::render(*app_ptr->main_window);
  }
  drawUI();
  ImGui::ShowMetricsWindow();
}

void app_cleanup()
{
  logo.destroy();
  field::cleanup();
}

int main(int argc, char *argv[])
{
  blackboard::app::App app("Driver Sim",
                           blackboard::renderer::Api::AUTO); // autodetect renderer api
  app_ptr = &app;
  app.on_update = app_update;
  app.on_init = init;
  app.run();

  app_cleanup();

  return 0;
}
