
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

#include <glsl/fs_blit.sc.bin.h>
#include <essl/fs_blit.sc.bin.h>
#include <spirv/fs_blit.sc.bin.h>
#include <wgsl/fs_blit.sc.bin.h>

#include <glsl/cs_oit_comp.sc.bin.h>
#include <essl/cs_oit_comp.sc.bin.h>
#include <spirv/cs_oit_comp.sc.bin.h>
#include <wgsl/cs_oit_comp.sc.bin.h>

#include <glsl/cs_taa_resolve.sc.bin.h>
#include <essl/cs_taa_resolve.sc.bin.h>
#include <spirv/cs_taa_resolve.sc.bin.h>
#include <wgsl/cs_taa_resolve.sc.bin.h>

#include <glsl/cs_camera_velocity.sc.bin.h>
#include <essl/cs_camera_velocity.sc.bin.h>
#include <spirv/cs_camera_velocity.sc.bin.h>
#include <wgsl/cs_camera_velocity.sc.bin.h>

#if BGFX_PLATFORM_SUPPORTS_DXBC
#include <dxbc/vs_pbr.sc.bin.h>
#include <dxbc/fs_pbr.sc.bin.h>
#include <dxbc/fs_pbr_oit.sc.bin.h>
#include <dxbc/fs_pbr_oit_depth_post_pass.sc.bin.h>

#include <dxbc/vs_pass.sc.bin.h>
#include <dxbc/fs_blit.sc.bin.h>

#include <dxbc/cs_oit_comp.sc.bin.h>
#include <dxbc/cs_taa_resolve.sc.bin.h>
#include <dxbc/cs_camera_velocity.sc.bin.h>
#endif

#if BGFX_PLATFORM_SUPPORTS_DXIL
#include <dxil/vs_pbr.sc.bin.h>
#include <dxil/fs_pbr.sc.bin.h>
#include <dxil/fs_pbr_oit.sc.bin.h>
#include <dxil/fs_pbr_oit_depth_post_pass.sc.bin.h>

#include <dxil/vs_pass.sc.bin.h>
#include <dxil/fs_blit.sc.bin.h>

#include <dxil/cs_oit_comp.sc.bin.h>
#include <dxil/cs_taa_resolve.sc.bin.h>
#include <dxil/cs_camera_velocity.sc.bin.h>
#endif

#if BGFX_PLATFORM_SUPPORTS_METAL
#include <metal/vs_pbr.sc.bin.h>
#include <metal/fs_pbr.sc.bin.h>
#include <metal/fs_pbr_oit.sc.bin.h>
#include <metal/fs_pbr_oit_depth_post_pass.sc.bin.h>

#include <metal/vs_pass.sc.bin.h>
#include <metal/fs_blit.sc.bin.h>

#include <metal/cs_oit_comp.sc.bin.h>
#include <metal/cs_taa_resolve.sc.bin.h>
#include <metal/cs_camera_velocity.sc.bin.h>
#endif