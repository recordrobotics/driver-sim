#include <blackboard_app/app.h>
#include <blackboard_app/gui.h>
#include <blackboard_app/resources.h>
#include <blackboard_app/window.h>

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

#define LOAD_FONT(font) \
  blackboard::gui::load_font(#font, (void *)font##_bytes, sizeof(font##_bytes), 12.0f, dpi);

blackboard::app::App *app_ptr;

void init()
{
  blackboard::gui::set_blackboard_theme();
  const auto dpi{app_ptr->main_window->effective_display_resolution()};

  LOAD_FONT(Inter_Thin_otf);
  LOAD_FONT(Inter_ThinItalic_otf);
  LOAD_FONT(Inter_ExtraLight_otf);
  LOAD_FONT(Inter_ExtraLightItalic_otf);
  LOAD_FONT(Inter_Light_otf);
  LOAD_FONT(Inter_LightItalic_otf);
  LOAD_FONT(Inter_Regular_otf);
  LOAD_FONT(Inter_Italic_otf);
  LOAD_FONT(Inter_Medium_otf);
  LOAD_FONT(Inter_MediumItalic_otf);
  LOAD_FONT(Inter_SemiBold_otf);
  LOAD_FONT(Inter_SemiBoldItalic_otf);
  LOAD_FONT(Inter_Bold_otf);
  LOAD_FONT(Inter_BoldItalic_otf);
  LOAD_FONT(Inter_ExtraBold_otf);
  LOAD_FONT(Inter_ExtraBoldItalic_otf);
  LOAD_FONT(Inter_Black_otf);
  LOAD_FONT(Inter_BlackItalic_otf);
}

void app_update()
{
  blackboard::gui::dockspace();
  ImGui::ShowDemoWindow();
}

int main(int argc, char *argv[])
{
  blackboard::app::App app("Driver Sim",
                           blackboard::renderer::Api::AUTO); // autodetect renderer api
  app_ptr = &app;
  app.on_update = app_update;
  app.on_init = init;
  app.run();

  return 0;
}
