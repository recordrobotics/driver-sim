
#pragma once

#include <glsl/vs_pbr.sc.bin.h>
#include <essl/vs_pbr.sc.bin.h>
#include <spirv/vs_pbr.sc.bin.h>
#include <wgsl/vs_pbr.sc.bin.h>

#include <glsl/fs_pbr.sc.bin.h>
#include <essl/fs_pbr.sc.bin.h>
#include <spirv/fs_pbr.sc.bin.h>
#include <wgsl/fs_pbr.sc.bin.h>

#include <glsl/fs_pbr_oit.sc.bin.h>
#include <essl/fs_pbr_oit.sc.bin.h>
#include <spirv/fs_pbr_oit.sc.bin.h>
#include <wgsl/fs_pbr_oit.sc.bin.h>

#include <glsl/fs_pbr_oit_depth_post_pass.sc.bin.h>
#include <essl/fs_pbr_oit_depth_post_pass.sc.bin.h>
#include <spirv/fs_pbr_oit_depth_post_pass.sc.bin.h>
#include <wgsl/fs_pbr_oit_depth_post_pass.sc.bin.h>

#include <glsl/vs_pass.sc.bin.h>
#include <essl/vs_pass.sc.bin.h>
#include <spirv/vs_pass.sc.bin.h>
#include <wgsl/vs_pass.sc.bin.h>

#include <glsl/fs_oit_comp.sc.bin.h>
#include <essl/fs_oit_comp.sc.bin.h>
#include <spirv/fs_oit_comp.sc.bin.h>
#include <wgsl/fs_oit_comp.sc.bin.h>

#include <glsl/fs_blit.sc.bin.h>
#include <essl/fs_blit.sc.bin.h>
#include <spirv/fs_blit.sc.bin.h>
#include <wgsl/fs_blit.sc.bin.h>

#include <glsl/fs_taa_resolve.sc.bin.h>
#include <essl/fs_taa_resolve.sc.bin.h>
#include <spirv/fs_taa_resolve.sc.bin.h>
#include <wgsl/fs_taa_resolve.sc.bin.h>

#include <glsl/fs_background_velocity.sc.bin.h>
#include <essl/fs_background_velocity.sc.bin.h>
#include <spirv/fs_background_velocity.sc.bin.h>
#include <wgsl/fs_background_velocity.sc.bin.h>

#if BGFX_PLATFORM_SUPPORTS_DXBC
#include <dxbc/vs_pbr.sc.bin.h>
#include <dxbc/fs_pbr.sc.bin.h>
#include <dxbc/fs_pbr_oit.sc.bin.h>
#include <dxbc/fs_pbr_oit_depth_post_pass.sc.bin.h>

#include <dxbc/vs_pass.sc.bin.h>
#include <dxbc/fs_oit_comp.sc.bin.h>
#include <dxbc/fs_blit.sc.bin.h>
#include <dxbc/fs_taa_resolve.sc.bin.h>
#include <dxbc/fs_background_velocity.sc.bin.h>
#endif

#if BGFX_PLATFORM_SUPPORTS_DXIL
#include <dxil/vs_pbr.sc.bin.h>
#include <dxil/fs_pbr.sc.bin.h>
#include <dxil/fs_pbr_oit.sc.bin.h>
#include <dxil/fs_pbr_oit_depth_post_pass.sc.bin.h>

#include <dxil/vs_pass.sc.bin.h>
#include <dxil/fs_oit_comp.sc.bin.h>
#include <dxil/fs_blit.sc.bin.h>
#include <dxil/fs_taa_resolve.sc.bin.h>
#include <dxil/fs_background_velocity.sc.bin.h>
#endif

#if BGFX_PLATFORM_SUPPORTS_METAL
#include <metal/vs_pbr.sc.bin.h>
#include <metal/fs_pbr.sc.bin.h>
#include <metal/fs_pbr_oit.sc.bin.h>
#include <metal/fs_pbr_oit_depth_post_pass.sc.bin.h>

#include <metal/vs_pass.sc.bin.h>
#include <metal/fs_oit_comp.sc.bin.h>
#include <metal/fs_blit.sc.bin.h>
#include <metal/fs_taa_resolve.sc.bin.h>
#include <metal/fs_background_velocity.sc.bin.h>
#endif