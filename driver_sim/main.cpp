#define IMGUI_DEFINE_MATH_OPERATORS

#include <blackboard_app/app.h>
#include <blackboard_app/gui.h>
#include <blackboard_app/platform/imgui_impl_sdl_bgfx.h>
#include <blackboard_app/resources.h>
#include <blackboard_app/window.h>

#include <bgfx/bgfx.h>
#include <bimg/decode.h>
#include <bx/allocator.h>
#include <imgui/imgui.h>

#include <Inter-Thin.otf.h>
#include <Inter-ThinItalic.otf.h>
#include <Inter-ExtraLight.otf.h>
#include <Inter-ExtraLightItalic.otf.h>
#include <Inter-Light.otf.h>
#include <Inter-LightItalic.otf.h>
#include <Inter-Regular.otf.h>
#include <Inter-Italic.otf.h>
#include <Inter-Medium.otf.h>
#include <Inter-MediumItalic.otf.h>
#include <Inter-SemiBold.otf.h>
#include <Inter-SemiBoldItalic.otf.h>
#include <Inter-Bold.otf.h>
#include <Inter-BoldItalic.otf.h>
#include <Inter-ExtraBold.otf.h>
#include <Inter-ExtraBoldItalic.otf.h>
#include <Inter-Black.otf.h>
#include <Inter-BlackItalic.otf.h>

#include <logo.png.h>

#include <algorithm>
#include <random>

using blackboard::gui::string_hex_to_rgba_float;

#define LOAD_FONT(font, set_as_default) \
  blackboard::gui::load_font(#font, (void *)font##_bytes, sizeof(font##_bytes), 12.0f, dpi, set_as_default);

blackboard::app::App *app_ptr;

bool loadLogoTexture();

void set_theme()
{
  ImGui::StyleColorsDark();

  static ImVec4 background{string_hex_to_rgba_float("#1E1E1Eff")};
  static auto selection{string_hex_to_rgba_float("#445a46ff")};
  static auto foreground{string_hex_to_rgba_float("#BBBBBBff")};
  static auto comment{string_hex_to_rgba_float("#38903Eff")};
  static auto cyan{string_hex_to_rgba_float("#8be9fdff")};
  static auto green{string_hex_to_rgba_float("#50fa7bff")};
  static auto orange{string_hex_to_rgba_float("#ffb86cff")};
  static auto pink{string_hex_to_rgba_float("#ff79c6ff")};
  static auto purple{string_hex_to_rgba_float("#bd93f9ff")};
  static auto red{string_hex_to_rgba_float("#ff5555ff")};
  static auto yellow{string_hex_to_rgba_float("#f1fa8cff")};

  const auto dark_alpha_selection{selection * ImVec4{1.0f, 1.0f, 1.0f, 0.5f}};
  const auto dark_alpha_green{green * ImVec4{1.0f, 1.0f, 1.0f, 0.3f}};
  const auto darker_background{background * ImVec4{0.15f, 0.15f, 0.15f, 1.0f}};
  const auto dark_alpha_red{red * ImVec4{1.0f, 1.0f, 1.0f, 0.10f}};

  auto &colors{ImGui::GetStyle().Colors};

  const auto IconColour{ImVec4{0.718, 0.62f, 0.86f, 1.00f}};
  colors[ImGuiCol_Text] = foreground;
  colors[ImGuiCol_TextSelectedBg] = comment;
  colors[ImGuiCol_TextDisabled] = string_hex_to_rgba_float("#666666ff");

  colors[ImGuiCol_WindowBg] = background;
  colors[ImGuiCol_ChildBg] = background;

  colors[ImGuiCol_PopupBg] = background;
  colors[ImGuiCol_Border] = dark_alpha_green;
  colors[ImGuiCol_BorderShadow] = ImVec4{0.00f, 0.00f, 0.00f, 0.00f};
  colors[ImGuiCol_FrameBg] = selection;
  colors[ImGuiCol_FrameBgHovered] = selection * ImVec4{1.1f, 1.1f, 1.1f, 1.0f};
  colors[ImGuiCol_FrameBgActive] = selection * ImVec4{1.2f, 1.2f, 1.2f, 1.0f};

  colors[ImGuiCol_TitleBg] = (selection + background) * ImVec4{0.5f, 0.5f, 0.5f, 1.0f};
  colors[ImGuiCol_TitleBgActive] = (selection + background) * ImVec4{0.5f, 0.5f, 0.5f, 1.0f};
  colors[ImGuiCol_TitleBgCollapsed] = (selection + background) * ImVec4{0.5f, 0.5f, 0.5f, 1.0f};
  colors[ImGuiCol_MenuBarBg] = selection;

  colors[ImGuiCol_ScrollbarBg] = ImVec4{0.02f, 0.02f, 0.02f, 0.39f};
  colors[ImGuiCol_ScrollbarGrab] = dark_alpha_selection;
  colors[ImGuiCol_ScrollbarGrabActive] = dark_alpha_selection * ImVec4{1.3f, 1.3f, 1.3f, 1.3f};
  colors[ImGuiCol_ScrollbarGrabHovered] = dark_alpha_selection * ImVec4{1.2f, 1.2f, 1.2f, 1.2f};

  colors[ImGuiCol_CheckMark] = comment;
  colors[ImGuiCol_SliderGrab] = comment;
  colors[ImGuiCol_SliderGrabActive] = comment * ImVec4{1.3f, 1.3f, 1.3f, 1.3f};
  colors[ImGuiCol_Button] = comment;
  colors[ImGuiCol_ButtonHovered] = comment * ImVec4{1.2f, 1.2f, 1.2f, 1.2f};
  colors[ImGuiCol_ButtonActive] = comment * ImVec4{1.3f, 1.3f, 1.3f, 1.3f};

  colors[ImGuiCol_Separator] = selection;
  colors[ImGuiCol_SeparatorHovered] = selection;
  colors[ImGuiCol_SeparatorActive] = selection;

  colors[ImGuiCol_ResizeGrip] = dark_alpha_green;
  colors[ImGuiCol_ResizeGripHovered] = dark_alpha_green * ImVec4{1.2f, 1.2f, 1.2f, 1.2f};
  colors[ImGuiCol_ResizeGripActive] = dark_alpha_green * ImVec4{1.3f, 1.3f, 1.3f, 1.3f};

  colors[ImGuiCol_PlotLines] = yellow;
  colors[ImGuiCol_PlotLinesHovered] = yellow * ImVec4{1.2f, 1.2f, 1.2f, 1.2f};
  colors[ImGuiCol_PlotHistogram] = yellow;
  colors[ImGuiCol_PlotHistogramHovered] = yellow * ImVec4{1.2f, 1.2f, 1.2f, 1.2f};

  colors[ImGuiCol_DragDropTarget] = red;

  colors[ImGuiCol_NavHighlight] = red;
  colors[ImGuiCol_NavWindowingHighlight] = comment;
  colors[ImGuiCol_NavWindowingDimBg] = red;
  colors[ImGuiCol_ModalWindowDimBg] = dark_alpha_red;

  colors[ImGuiCol_Header] = dark_alpha_selection;
  colors[ImGuiCol_HeaderHovered] = dark_alpha_selection * ImVec4{1.2f, 1.2f, 1.2f, 1.2f};
  colors[ImGuiCol_HeaderActive] = dark_alpha_selection * ImVec4{1.3f, 1.3f, 1.3f, 1.3f};

  colors[ImGuiCol_Tab] = comment;
  colors[ImGuiCol_TabHovered] = comment * ImVec4{1.2f, 1.2f, 1.2f, 1.2f};
  colors[ImGuiCol_TabActive] = comment * ImVec4{1.3f, 1.3f, 1.3f, 1.3f};
  colors[ImGuiCol_TabUnfocused] = comment * ImVec4{0.5f, 0.5f, 0.5f, 0.5f};
  colors[ImGuiCol_TabUnfocusedActive] = comment * ImVec4{0.5f, 0.5f, 0.5f, 0.5f};

  colors[ImGuiCol_DockingEmptyBg] = darker_background;
  colors[ImGuiCol_DockingPreview] = dark_alpha_green;

  colors[ImGuiCol_TableHeaderBg] = comment;
  colors[ImGuiCol_TableBorderLight] = dark_alpha_green;
  colors[ImGuiCol_TableBorderStrong] = dark_alpha_green;

  auto &style{ImGui::GetStyle()};
  style.FramePadding = {2.0f, 2.0f};
  style.CellPadding = {2.0f, 2.0f};
  style.TabBorderSize = 1.0f;
  style.TabRounding = 1.0f;
  style.ScrollbarRounding = 2.0f;
  style.GrabRounding = 2.0f;
  style.WindowRounding = 2.0f;
  style.ChildRounding = 2.0f;
  style.FrameRounding = 2.0f;
}

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

  loadLogoTexture();
}

void showMetrics()
{
  const auto io{ImGui::GetIO()};

  if (!ImGui::Begin("Metrics"))
  {
    ImGui::End();
    return;
  }

  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);

  ImGui::End();
}

// ---------- State ----------
enum Page
{
  PAGE_LOADING,
  PAGE_SELECT
};

enum PageTransitionState
{
  PAGE_TRANSITION_NONE,
  PAGE_TRANSITION_FADE_TO_BG,
  PAGE_TRANSITION_FADE_FROM_BG
};

static Page currentPage = PAGE_LOADING;
static Page transitionTargetPage = PAGE_LOADING;
static PageTransitionState transitionState = PAGE_TRANSITION_NONE;
static float transitionAlpha = 0.0f;
static constexpr float TRANSITION_PHASE_DURATION = 0.30f;

// Progress values (0.0f → 1.0f)
static float javaProgress = 0.0f;
static float elasticProgress = 0.0f;
static float jniProgress = 0.0f;
static float unpackProgress = 0.0f;

static float javaSpeed = 0.0f;
static float elasticSpeed = 0.0f;
static float jniSpeed = 0.0f;
static float unpackSpeed = 0.0f;

static float javaRetargetTime = 0.0f;
static float elasticRetargetTime = 0.0f;
static float jniRetargetTime = 0.0f;
static float unpackRetargetTime = 0.0f;

void resetLoadingProgress()
{
  javaProgress = elasticProgress = jniProgress = unpackProgress = 0.0f;
  javaSpeed = elasticSpeed = jniSpeed = unpackSpeed = 0.0f;
  javaRetargetTime = elasticRetargetTime = jniRetargetTime = unpackRetargetTime = 0.0f;
}

void beginPageTransition(Page targetPage)
{
  if (transitionState != PAGE_TRANSITION_NONE)
  {
    return;
  }

  if (targetPage == currentPage)
  {
    return;
  }

  transitionTargetPage = targetPage;
  transitionState = PAGE_TRANSITION_FADE_TO_BG;
  transitionAlpha = 0.0f;
}

void updatePageTransition()
{
  if (transitionState == PAGE_TRANSITION_NONE)
  {
    return;
  }

  const float deltaTime = ImGui::GetIO().DeltaTime;
  const float alphaStep = TRANSITION_PHASE_DURATION > 0.0f ? (deltaTime / TRANSITION_PHASE_DURATION) : 1.0f;

  if (transitionState == PAGE_TRANSITION_FADE_TO_BG)
  {
    transitionAlpha = std::min(1.0f, transitionAlpha + alphaStep);
    if (transitionAlpha >= 1.0f)
    {
      currentPage = transitionTargetPage;
      transitionState = PAGE_TRANSITION_FADE_FROM_BG;
    }
  }
  else if (transitionState == PAGE_TRANSITION_FADE_FROM_BG)
  {
    transitionAlpha = std::max(0.0f, transitionAlpha - alphaStep);
    if (transitionAlpha <= 0.0f)
    {
      transitionState = PAGE_TRANSITION_NONE;
    }
  }
}

void drawPageTransitionOverlay()
{
  if (transitionState == PAGE_TRANSITION_NONE && transitionAlpha <= 0.0f)
  {
    return;
  }

  ImVec4 overlayColor = string_hex_to_rgba_float("#1E1E1Eff");
  overlayColor.w = std::clamp(transitionAlpha, 0.0f, 1.0f);

  const ImVec2 windowPos = ImGui::GetWindowPos();
  const ImVec2 windowSize = ImGui::GetWindowSize();
  const ImVec2 windowMax(windowPos.x + windowSize.x, windowPos.y + windowSize.y);

  ImGui::GetWindowDrawList()->AddRectFilled(
      windowPos,
      windowMax,
      ImGui::ColorConvertFloat4ToU32(overlayColor));
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
    beginPageTransition(PAGE_SELECT);
  }
}

// Multiselect buttons
static bool driverStationSelected = true;
static bool simGuiSelected = false;

// Texture (load this externally)
static ImTextureID logoTexture = NULL;
static bgfx::TextureHandle logoTextureHandle = BGFX_INVALID_HANDLE;

static bx::DefaultAllocator s_allocator;

bool loadLogoTexture()
{
  bimg::ImageContainer *image = bimg::imageParse(
      &s_allocator,
      logo_png_bytes,
      static_cast<uint32_t>(sizeof(logo_png_bytes)),
      bimg::TextureFormat::BGRA8);

  if (image == nullptr)
  {
    return false;
  }

  logoTextureHandle = bgfx::createTexture2D(
      static_cast<uint16_t>(image->m_width),
      static_cast<uint16_t>(image->m_height),
      false,
      1,
      bgfx::TextureFormat::BGRA8,
      0,
      bgfx::copy(image->m_data, image->m_size));

  bimg::imageFree(image);

  if (!bgfx::isValid(logoTextureHandle))
  {
    return false;
  }

  logoTexture = blackboard::renderer::toId(
      logoTextureHandle,
      IMGUI_FLAGS_ALPHA_BLEND,
      0);

  return true;
}

// ---------- Helpers ----------
void DrawCenteredText(const char *text, float yOffset = 0.0f)
{
  ImVec2 winSize = ImGui::GetWindowSize();
  ImVec2 textSize = ImGui::CalcTextSize(text);

  ImGui::SetCursorPosX((winSize.x - textSize.x) * 0.5f);
  ImGui::SetCursorPosY(ImGui::GetCursorPosY() + yOffset);
  ImGui::TextUnformatted(text);
}

void DrawProgress(const char *label, float value)
{
  auto &style{ImGui::GetStyle()};
  float globalScale = style.FontScaleMain * style.FontScaleDpi;

  ImVec2 winSize = ImGui::GetWindowSize();
  float width = 350.0f * globalScale;

  ImGui::SetCursorPosX((winSize.x - width) * 0.5f);

  char buf[128];
  sprintf(buf, "%s (%.0f%%)", label, value * 100.0f);

  ImGui::PushFont(nullptr, 16.0f);
  ImGui::TextColored(string_hex_to_rgba_float("#A7A7A7ff"), "%s", buf);
  ImGui::PopFont();

  ImGui::Dummy(ImVec2(0, 8 * globalScale));

  ImGui::SetCursorPosX((winSize.x - width) * 0.5f);

  ImGui::PushStyleColor(ImGuiCol_FrameBg, string_hex_to_rgba_float("#2D2A2Aff"));
  ImGui::PushStyleColor(ImGuiCol_PlotHistogram, string_hex_to_rgba_float("#42A749ff"));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 999.0f * globalScale);
  ImGui::ProgressBar(value, ImVec2(width, 7 * globalScale), "");
  ImGui::PopStyleVar();
  ImGui::PopStyleColor(2);

  ImGui::Dummy(ImVec2(0, 18 * globalScale));
}

typedef struct ToggleButton
{
  const char *label;
  bool *state;
} ToggleButton;

void SplitToggleButtonGroup(std::list<ToggleButton> buttons)
{
  if (buttons.empty())
  {
    return;
  }

  auto &style{ImGui::GetStyle()};
  float globalScale = style.FontScaleMain * style.FontScaleDpi;
  ImVec2 winSize = ImGui::GetWindowSize();

  const ImVec2 framePadding(16.0f * globalScale, 7.0f * globalScale);
  const float frameRounding = 6.0f * globalScale;
  const float borderThickness = 1.0f * globalScale;

  ImVec4 off_normal = string_hex_to_rgba_float("#ffffff00");
  ImVec4 off_hovered = string_hex_to_rgba_float("#ffffff0a");
  ImVec4 off_active = string_hex_to_rgba_float("#ffffff28");
  ImVec4 on_normal = string_hex_to_rgba_float("#38903Eff");
  ImVec4 on_hovered = string_hex_to_rgba_float("#449c4aff");
  ImVec4 on_active = string_hex_to_rgba_float("#44b24bff");
  ImVec4 textColor = string_hex_to_rgba_float("#FFFFFFff");
  ImVec4 borderColor = string_hex_to_rgba_float("#3E3E3Eff");

  const ImU32 borderColorU32 = ImGui::ColorConvertFloat4ToU32(borderColor);
  const ImU32 textColorU32 = ImGui::ColorConvertFloat4ToU32(textColor);

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

  ImGui::PushFont(nullptr, 16.0f);

  float totalWidth = 0;
  for (auto &button : buttons)
  {
    totalWidth += ImGui::CalcTextSize(button.label).x + framePadding.x * 2.0f;
  }

  const float segmentHeight = ImGui::GetTextLineHeight() + framePadding.y * 2.0f;

  ImGui::SetCursorPosX((winSize.x - totalWidth) / 2);
  const ImVec2 groupMin = ImGui::GetCursorScreenPos();

  ImDrawList *drawList = ImGui::GetWindowDrawList();
  const size_t buttonCount = buttons.size();
  size_t index = 0;

  for (auto &button : buttons)
  {
    const float segmentWidth = ImGui::CalcTextSize(button.label).x + framePadding.x * 2.0f;

    ImGui::PushID(static_cast<int>(index));
    const ImVec2 segmentPos = ImGui::GetCursorScreenPos();
    const ImVec2 segmentSize(segmentWidth, segmentHeight);

    const bool pressed = ImGui::InvisibleButton("##segment", segmentSize);
    if (pressed)
    {
      *button.state = !(*button.state);
    }

    const bool hovered = ImGui::IsItemHovered();
    const bool active = ImGui::IsItemActive();

    if (hovered)
    {
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    const ImVec4 fillColor = *button.state
                                 ? (active ? on_active : (hovered ? on_hovered : on_normal))
                                 : (active ? off_active : (hovered ? off_hovered : off_normal));

    ImDrawFlags cornerFlags = ImDrawFlags_RoundCornersNone;
    if (buttonCount == 1)
    {
      cornerFlags = ImDrawFlags_RoundCornersAll;
    }
    else if (index == 0)
    {
      cornerFlags = ImDrawFlags_RoundCornersLeft;
    }
    else if (index + 1 == buttonCount)
    {
      cornerFlags = ImDrawFlags_RoundCornersRight;
    }

    drawList->AddRectFilled(
        segmentPos,
        ImVec2(segmentPos.x + segmentSize.x, segmentPos.y + segmentSize.y),
        ImGui::ColorConvertFloat4ToU32(fillColor),
        frameRounding,
        cornerFlags);

    const ImVec2 textSize = ImGui::CalcTextSize(button.label);
    const ImVec2 textPos(
        segmentPos.x + (segmentSize.x - textSize.x) * 0.5f,
        segmentPos.y + (segmentSize.y - textSize.y) * 0.5f);

    drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, textColorU32, button.label);

    ImGui::PopID();

    ++index;
    if (index < buttonCount)
    {
      ImGui::SameLine(0.0f, 0.0f);
    }
  }

  const ImVec2 groupMax(groupMin.x + totalWidth, groupMin.y + segmentHeight);
  drawList->AddRect(
      groupMin,
      groupMax,
      borderColorU32,
      frameRounding,
      ImDrawFlags_RoundCornersAll,
      borderThickness);

  float dividerX = groupMin.x;
  size_t dividerIndex = 0;
  for (auto &button : buttons)
  {
    dividerX += ImGui::CalcTextSize(button.label).x + framePadding.x * 2.0f;
    ++dividerIndex;
    if (dividerIndex < buttonCount)
    {
      drawList->AddLine(
          ImVec2(dividerX, groupMin.y),
          ImVec2(dividerX, groupMax.y),
          borderColorU32,
          borderThickness);
    }
  }

  ImGui::PopFont();

  ImGui::PopStyleVar();
}

bool UnderlineTextButton(const char *text)
{
  auto &style{ImGui::GetStyle()};
  float globalScale = style.FontScaleMain * style.FontScaleDpi;

  ImGui::PushFont(nullptr, 12.0f);
  ImVec2 winSize = ImGui::GetWindowSize();
  const ImVec2 textSize = ImGui::CalcTextSize(text);
  ImGui::SetCursorPosX((winSize.x - textSize.x) / 2);

  ImGui::PushID(text);
  bool pressed = ImGui::InvisibleButton("##underline_btn", textSize);
  ImGui::PopID();

  const bool hovered = ImGui::IsItemHovered();
  const bool active = ImGui::IsItemActive();

  if (hovered)
  {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }

  ImVec4 textColor = active ? string_hex_to_rgba_float("#5cbd62ff") : hovered ? string_hex_to_rgba_float("#47a54dff")
                                                                              : string_hex_to_rgba_float("#38903Eff");

  ImVec2 min = ImGui::GetItemRectMin();
  ImVec2 max = ImGui::GetItemRectMax();
  const ImU32 textColorU32 = ImGui::ColorConvertFloat4ToU32(textColor);

  ImGui::GetWindowDrawList()->AddText(min, textColorU32, text);

  float thickness = 1.5f * globalScale;
  float offset = 2.0f * globalScale;

  ImGui::GetWindowDrawList()->AddLine(
      ImVec2(min.x, max.y + offset),
      ImVec2(max.x, max.y + offset),
      textColorU32,
      thickness);

  ImGui::PopFont();

  return pressed;
}

bool CircularButton(const char *id, float radius)
{
  ImVec2 pos = ImGui::GetCursorScreenPos();
  ImDrawList *draw = ImGui::GetWindowDrawList();

  bool pressed = ImGui::InvisibleButton(id, ImVec2(radius * 2, radius * 2));

  bool hovered = ImGui::IsItemHovered();
  bool active = ImGui::IsItemActive();

  if (hovered)
  {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }

  ImVec4 color = active ? string_hex_to_rgba_float("#71d478ff") : hovered ? string_hex_to_rgba_float("#5dbb64ff")
                                                                          : string_hex_to_rgba_float("#42A749ff");

  ImU32 colorU32 = ImGui::ColorConvertFloat4ToU32(color);

  draw->AddCircleFilled(ImVec2(pos.x + radius, pos.y + radius), radius, colorU32);

  // Arrow rendered as 3 stroked segments with round endcaps.
  const ImVec2 center(pos.x + radius, pos.y + radius);
  const float arrowThickness = radius * 0.13f;
  const float capRadius = arrowThickness * 0.5f;

  const ImVec2 shaftStart(center.x - radius * 0.45f, center.y);
  const ImVec2 shaftEnd(center.x + radius * 0.45f, center.y);
  const ImVec2 arrowTip(center.x + radius * 0.45f, center.y);
  const ImVec2 headTop(center.x, arrowTip.y - radius * 0.45f);
  const ImVec2 headBottom(center.x, arrowTip.y + radius * 0.45f);

  auto drawRoundedSegment = [&](const ImVec2 &a, const ImVec2 &b)
  {
    draw->AddLine(a, b, IM_COL32_WHITE, arrowThickness);
    draw->AddCircleFilled(a, capRadius, IM_COL32_WHITE);
    draw->AddCircleFilled(b, capRadius, IM_COL32_WHITE);
  };

  drawRoundedSegment(shaftStart, shaftEnd);
  drawRoundedSegment(headTop, arrowTip);
  drawRoundedSegment(headBottom, arrowTip);

  return pressed;
}

// ---------- Main UI ----------
void RenderDriverSimUI()
{
  const static ImGuiWindowFlags window_flags{
      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
      ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus};

  const ImGuiViewport *viewport{ImGui::GetMainViewport()};
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::SetNextWindowViewport(viewport->ID);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

  ImGui::Begin("DriverSim", nullptr, window_flags);

  auto &style{ImGui::GetStyle()};
  float globalScale = style.FontScaleMain * style.FontScaleDpi;

  updatePageTransition();

  ImVec2 winSize = ImGui::GetWindowSize();
  const ImVec2 logoSize(70.0f * globalScale, 70.0f * globalScale);
  const float logoTopY = (winSize.y - logoSize.y) * 0.5f;

  ImGui::SetCursorPosY(logoTopY);

  // Logo
  if (logoTexture)
  {
    ImGui::SetCursorPosX((winSize.x - logoSize.x) / 2);
    ImGui::Image(logoTexture, logoSize);
  }
  else
  {
    ImGui::SetCursorPosX((winSize.x - logoSize.x) / 2);
    ImGui::Dummy(logoSize);
  }

  ImGui::Dummy(ImVec2(0, 6 * globalScale));

  ImGui::PushFont(nullptr, 35.0f);
  ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#8F8686ff"));
  DrawCenteredText("Driver Sim");
  ImGui::PopStyleColor();
  ImGui::PopFont();

  if (currentPage == PAGE_LOADING)
  {
    updateLoadingProgress();

    ImGui::Dummy(ImVec2(0, 22 * globalScale));

    DrawProgress("Downloading Java 17...", javaProgress);
    DrawProgress("Downloading Elastic Dashboard...", elasticProgress);
    DrawProgress("Downloading JNI Libraries...", jniProgress);
    DrawProgress("Unpacking robot code...", unpackProgress);
  }
  else
  {
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
      beginPageTransition(PAGE_LOADING);
    }

    ImGui::Dummy(ImVec2(0, 20 * globalScale));

    ImGui::SetCursorPosX((winSize.x - 28 * 2 * globalScale) / 2);
    if (CircularButton("go", 28 * globalScale))
    {
      resetLoadingProgress();
      beginPageTransition(PAGE_LOADING);
    }
  }

  // Footer
  ImGui::SetCursorPosY(winSize.y - 30 * globalScale);
  ImGui::PushFont(nullptr, 10.0f);
  ImGui::PushStyleColor(ImGuiCol_Text, string_hex_to_rgba_float("#858585ff"));
  DrawCenteredText("Made by Record Robotics");
  ImGui::PopStyleColor();
  ImGui::PopFont();

  drawPageTransitionOverlay();

  ImGui::PopStyleVar(3);

  ImGui::End();
}

void app_update()
{
  RenderDriverSimUI();
  showMetrics();
  ImGui::ShowMetricsWindow();
}

int main(int argc, char *argv[])
{
  blackboard::app::App app("Driver Sim",
                           blackboard::renderer::Api::AUTO); // autodetect renderer api
  app_ptr = &app;
  app.on_update = app_update;
  app.on_init = init;
  app.run();

  if (bgfx::isValid(logoTextureHandle))
  {
    bgfx::destroy(logoTextureHandle);
    logoTextureHandle = BGFX_INVALID_HANDLE;
    logoTexture = 0;
  }

  return 0;
}
