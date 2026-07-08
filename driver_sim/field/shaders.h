
#pragma once

#include <glsl/vs_pbr.sc.bin.h>
#include <essl/vs_pbr.sc.bin.h>
#include <spirv/vs_pbr.sc.bin.h>

#include <glsl/vs_pbr_instanced.sc.bin.h>
#include <essl/vs_pbr_instanced.sc.bin.h>
#include <spirv/vs_pbr_instanced.sc.bin.h>

#include <glsl/vs_pbr_led.sc.bin.h>
#include <essl/vs_pbr_led.sc.bin.h>
#include <spirv/vs_pbr_led.sc.bin.h>

#include <glsl/vs_pbr_apriltag.sc.bin.h>
#include <essl/vs_pbr_apriltag.sc.bin.h>
#include <spirv/vs_pbr_apriltag.sc.bin.h>

#include <glsl/fs_pbr.sc.bin.h>
#include <essl/fs_pbr.sc.bin.h>
#include <spirv/fs_pbr.sc.bin.h>

#include <glsl/fs_pbr_led.sc.bin.h>
#include <essl/fs_pbr_led.sc.bin.h>
#include <spirv/fs_pbr_led.sc.bin.h>

#include <glsl/fs_pbr_textured.sc.bin.h>
#include <essl/fs_pbr_textured.sc.bin.h>
#include <spirv/fs_pbr_textured.sc.bin.h>

#include <glsl/fs_pbr_apriltag.sc.bin.h>
#include <essl/fs_pbr_apriltag.sc.bin.h>
#include <spirv/fs_pbr_apriltag.sc.bin.h>

#include <glsl/fs_pbr_oit.sc.bin.h>
#include <essl/fs_pbr_oit.sc.bin.h>
#include <spirv/fs_pbr_oit.sc.bin.h>

#include <glsl/fs_pbr_oit_depth_post_pass.sc.bin.h>
#include <essl/fs_pbr_oit_depth_post_pass.sc.bin.h>
#include <spirv/fs_pbr_oit_depth_post_pass.sc.bin.h>

#include <glsl/vs_pass.sc.bin.h>
#include <essl/vs_pass.sc.bin.h>
#include <spirv/vs_pass.sc.bin.h>

#include <glsl/fs_present.sc.bin.h>
#include <essl/fs_present.sc.bin.h>
#include <spirv/fs_present.sc.bin.h>

#include <glsl/cs_blit.sc.bin.h>
#include <essl/cs_blit.sc.bin.h>
#include <spirv/cs_blit.sc.bin.h>

#include <glsl/cs_debug_normals.sc.bin.h>
#include <essl/cs_debug_normals.sc.bin.h>
#include <spirv/cs_debug_normals.sc.bin.h>

#include <glsl/cs_tonemap.sc.bin.h>
#include <essl/cs_tonemap.sc.bin.h>
#include <spirv/cs_tonemap.sc.bin.h>

#include <glsl/cs_exposure.sc.bin.h>
#include <essl/cs_exposure.sc.bin.h>
#include <spirv/cs_exposure.sc.bin.h>

#include <glsl/cs_oit_comp.sc.bin.h>
#include <essl/cs_oit_comp.sc.bin.h>
#include <spirv/cs_oit_comp.sc.bin.h>

#include <glsl/cs_taa_resolve.sc.bin.h>
#include <essl/cs_taa_resolve.sc.bin.h>
#include <spirv/cs_taa_resolve.sc.bin.h>

#include <glsl/cs_mb_velocity.sc.bin.h>
#include <essl/cs_mb_velocity.sc.bin.h>
#include <spirv/cs_mb_velocity.sc.bin.h>

#include <glsl/cs_mb_guertin_tile_max_x.sc.bin.h>
#include <essl/cs_mb_guertin_tile_max_x.sc.bin.h>
#include <spirv/cs_mb_guertin_tile_max_x.sc.bin.h>

#include <glsl/cs_mb_guertin_tile_max_y.sc.bin.h>
#include <essl/cs_mb_guertin_tile_max_y.sc.bin.h>
#include <spirv/cs_mb_guertin_tile_max_y.sc.bin.h>

#include <glsl/cs_mb_guertin_neighbor_max.sc.bin.h>
#include <essl/cs_mb_guertin_neighbor_max.sc.bin.h>
#include <spirv/cs_mb_guertin_neighbor_max.sc.bin.h>

#include <glsl/cs_mb_guertin_tile_variance.sc.bin.h>
#include <essl/cs_mb_guertin_tile_variance.sc.bin.h>
#include <spirv/cs_mb_guertin_tile_variance.sc.bin.h>

#include <glsl/cs_mb_guertin_experimental_blur.sc.bin.h>
#include <essl/cs_mb_guertin_experimental_blur.sc.bin.h>
#include <spirv/cs_mb_guertin_experimental_blur.sc.bin.h>

#include <glsl/cs_bloom_downscale.sc.bin.h>
#include <essl/cs_bloom_downscale.sc.bin.h>
#include <spirv/cs_bloom_downscale.sc.bin.h>

#include <glsl/cs_bloom_upscale.sc.bin.h>
#include <essl/cs_bloom_upscale.sc.bin.h>
#include <spirv/cs_bloom_upscale.sc.bin.h>

#include <glsl/cs_XeGTAO_PrefilterDepths16x16.sc.bin.h>
#include <essl/cs_XeGTAO_PrefilterDepths16x16.sc.bin.h>
#include <spirv/cs_XeGTAO_PrefilterDepths16x16.sc.bin.h>

#include <glsl/cs_XeGTAO_MainPass.sc.bin.h>
#include <essl/cs_XeGTAO_MainPass.sc.bin.h>
#include <spirv/cs_XeGTAO_MainPass.sc.bin.h>

#include <glsl/cs_XeGTAO_Denoise.sc.bin.h>
#include <essl/cs_XeGTAO_Denoise.sc.bin.h>
#include <spirv/cs_XeGTAO_Denoise.sc.bin.h>

#include <glsl/cs_XeGTAO_debugNormals.sc.bin.h>
#include <essl/cs_XeGTAO_debugNormals.sc.bin.h>
#include <spirv/cs_XeGTAO_debugNormals.sc.bin.h>

#include <glsl/cs_XeGTAO_debugVisibility.sc.bin.h>
#include <essl/cs_XeGTAO_debugVisibility.sc.bin.h>
#include <spirv/cs_XeGTAO_debugVisibility.sc.bin.h>

#if BGFX_PLATFORM_SUPPORTS_DXBC
#include <dxbc/vs_pbr.sc.bin.h>
#include <dxbc/vs_pbr_instanced.sc.bin.h>
#include <dxbc/vs_pbr_led.sc.bin.h>
#include <dxbc/vs_pbr_apriltag.sc.bin.h>
#include <dxbc/fs_pbr.sc.bin.h>
#include <dxbc/fs_pbr_led.sc.bin.h>
#include <dxbc/fs_pbr_textured.sc.bin.h>
#include <dxbc/fs_pbr_apriltag.sc.bin.h>
#include <dxbc/fs_pbr_oit.sc.bin.h>
#include <dxbc/fs_pbr_oit_depth_post_pass.sc.bin.h>

#include <dxbc/vs_pass.sc.bin.h>
#include <dxbc/fs_present.sc.bin.h>

#include <dxbc/cs_blit.sc.bin.h>
#include <dxbc/cs_debug_normals.sc.bin.h>
#include <dxbc/cs_tonemap.sc.bin.h>
#include <dxbc/cs_exposure.sc.bin.h>
#include <dxbc/cs_oit_comp.sc.bin.h>
#include <dxbc/cs_taa_resolve.sc.bin.h>
#include <dxbc/cs_mb_velocity.sc.bin.h>
#include <dxbc/cs_mb_guertin_tile_max_x.sc.bin.h>
#include <dxbc/cs_mb_guertin_tile_max_y.sc.bin.h>
#include <dxbc/cs_mb_guertin_neighbor_max.sc.bin.h>
#include <dxbc/cs_mb_guertin_tile_variance.sc.bin.h>
#include <dxbc/cs_mb_guertin_experimental_blur.sc.bin.h>

#include <dxbc/cs_bloom_downscale.sc.bin.h>
#include <dxbc/cs_bloom_upscale.sc.bin.h>

#include <dxbc/cs_XeGTAO_PrefilterDepths16x16.sc.bin.h>
#include <dxbc/cs_XeGTAO_MainPass.sc.bin.h>
#include <dxbc/cs_XeGTAO_Denoise.sc.bin.h>
#include <dxbc/cs_XeGTAO_debugNormals.sc.bin.h>
#include <dxbc/cs_XeGTAO_debugVisibility.sc.bin.h>
#endif

#if BGFX_PLATFORM_SUPPORTS_DXIL
#include <dxil/vs_pbr.sc.bin.h>
#include <dxil/vs_pbr_instanced.sc.bin.h>
#include <dxil/vs_pbr_led.sc.bin.h>
#include <dxil/vs_pbr_apriltag.sc.bin.h>
#include <dxil/fs_pbr.sc.bin.h>
#include <dxil/fs_pbr_led.sc.bin.h>
#include <dxil/fs_pbr_textured.sc.bin.h>
#include <dxil/fs_pbr_apriltag.sc.bin.h>
#include <dxil/fs_pbr_oit.sc.bin.h>
#include <dxil/fs_pbr_oit_depth_post_pass.sc.bin.h>

#include <dxil/vs_pass.sc.bin.h>
#include <dxil/fs_present.sc.bin.h>

#include <dxil/cs_blit.sc.bin.h>
#include <dxil/cs_debug_normals.sc.bin.h>
#include <dxil/cs_tonemap.sc.bin.h>
#include <dxil/cs_exposure.sc.bin.h>
#include <dxil/cs_oit_comp.sc.bin.h>
#include <dxil/cs_taa_resolve.sc.bin.h>
#include <dxil/cs_mb_velocity.sc.bin.h>
#include <dxil/cs_mb_guertin_tile_max_x.sc.bin.h>
#include <dxil/cs_mb_guertin_tile_max_y.sc.bin.h>
#include <dxil/cs_mb_guertin_neighbor_max.sc.bin.h>
#include <dxil/cs_mb_guertin_tile_variance.sc.bin.h>
#include <dxil/cs_mb_guertin_experimental_blur.sc.bin.h>

#include <dxil/cs_bloom_downscale.sc.bin.h>
#include <dxil/cs_bloom_upscale.sc.bin.h>

#include <dxil/cs_XeGTAO_PrefilterDepths16x16.sc.bin.h>
#include <dxil/cs_XeGTAO_MainPass.sc.bin.h>
#include <dxil/cs_XeGTAO_Denoise.sc.bin.h>
#include <dxil/cs_XeGTAO_debugNormals.sc.bin.h>
#include <dxil/cs_XeGTAO_debugVisibility.sc.bin.h>
#endif

#if BGFX_PLATFORM_SUPPORTS_METAL
#include <metal/vs_pbr.sc.bin.h>
#include <metal/vs_pbr_instanced.sc.bin.h>
#include <metal/vs_pbr_led.sc.bin.h>
#include <metal/vs_pbr_apriltag.sc.bin.h>
#include <metal/fs_pbr.sc.bin.h>
#include <metal/fs_pbr_led.sc.bin.h>
#include <metal/fs_pbr_textured.sc.bin.h>
#include <metal/fs_pbr_apriltag.sc.bin.h>
#include <metal/fs_pbr_oit.sc.bin.h>
#include <metal/fs_pbr_oit_depth_post_pass.sc.bin.h>

#include <metal/vs_pass.sc.bin.h>
#include <metal/fs_present.sc.bin.h>

#include <metal/cs_blit.sc.bin.h>
#include <metal/cs_debug_normals.sc.bin.h>
#include <metal/cs_tonemap.sc.bin.h>
#include <metal/cs_exposure.sc.bin.h>
#include <metal/cs_oit_comp.sc.bin.h>
#include <metal/cs_taa_resolve.sc.bin.h>
#include <metal/cs_mb_velocity.sc.bin.h>
#include <metal/cs_mb_guertin_tile_max_x.sc.bin.h>
#include <metal/cs_mb_guertin_tile_max_y.sc.bin.h>
#include <metal/cs_mb_guertin_neighbor_max.sc.bin.h>
#include <metal/cs_mb_guertin_tile_variance.sc.bin.h>
#include <metal/cs_mb_guertin_experimental_blur.sc.bin.h>

#include <metal/cs_bloom_downscale.sc.bin.h>
#include <metal/cs_bloom_upscale.sc.bin.h>

#include <metal/cs_XeGTAO_PrefilterDepths16x16.sc.bin.h>
#include <metal/cs_XeGTAO_MainPass.sc.bin.h>
#include <metal/cs_XeGTAO_Denoise.sc.bin.h>
#include <metal/cs_XeGTAO_debugNormals.sc.bin.h>
#include <metal/cs_XeGTAO_debugVisibility.sc.bin.h>
#endif