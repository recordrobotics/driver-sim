#pragma once
#include <stdint.h>
#include <cstdint>

struct SDL_Window;

namespace blackboard::app
{
  struct Window;
}

namespace blackboard::renderer
{

  enum class Api : uint8_t
  {
    NONE = 0,
    METAL,
    D3D11,
    D3D12,
    VULKAN,
    OPENGL,
    WEBGL,
    AUTO
  };

  bool init(app::Window &window, Api &, const uint16_t width, const uint16_t height);

  uint32_t get_bgfx_debug_flags();
  void set_bgfx_debug_flags(uint32_t flags);

} // namespace blackboard::renderer
