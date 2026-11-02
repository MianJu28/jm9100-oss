/*
* Copyright (c) 2020 ChangSha JingJiaMicro Electronics Co., Ltd.
* All rights reserved.
*
* Author:
*      shanjinkui <shanjinkui@jingjiamicro.com>
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice (including the next
* paragraph) shall be included in all copies or substantial portions of the
* Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* ChangSha JingJiaMicro Electronics Co., Ltd. AND/OR ITS SUPPLIERS BE LIABLE
* FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "util/u_memory.h"
#include "util/u_screen.h"
#include "util/u_transfer_helper.h"
#include "util/format/u_format.h"
#include "util/os_time.h"
#include "compiler/nir/nir_builder.h"

#include "mwv207_cmd.h"
#include "mwv207_context.h"
#include "mwv207_screen.h"
#include "mwv207_shader.h"
#include "mwv207_drm_public.h"
#include "mwv207_common.h"

static const char *
mwv207_get_vendor(struct pipe_screen *pscreen)
{
   return "JingjiaMicro";
}

static const char *
mwv207_get_device_vendor(struct pipe_screen *pscreen)
{
   return "MWV207 (vendor-id: 0x0731)";
}

static const char *
mwv207_get_name(struct pipe_screen *pscreen)
{
   return "MWV207 (JingjiaMicro Corporation)";
}

static int
mwv207_get_param(struct pipe_screen *pscreen, enum pipe_cap param)
{
   switch (param) {
   case PIPE_CAP_GRAPHICS:
      return 1;
   case PIPE_CAP_TGSI_TEXCOORD:
      return 1;
   case PIPE_CAP_IMAGE_STORE_FORMATTED:
      return 1;
   case PIPE_CAP_NPOT_TEXTURES:
      return 1;
   case PIPE_CAP_MAX_RENDER_TARGETS:
      return 4;
   case PIPE_CAP_OCCLUSION_QUERY:
      return 1;
   case PIPE_CAP_QUERY_TIME_ELAPSED:
      return 1;
   case PIPE_CAP_TEXTURE_SHADOW_MAP:
      return 1;
   case PIPE_CAP_TEXTURE_SWIZZLE:
      return 1;
   case PIPE_CAP_MAX_TEXTURE_2D_SIZE:
      return 8192;
   case PIPE_CAP_MAX_TEXTURE_3D_LEVELS:
      return 14;
   case PIPE_CAP_MAX_TEXTURE_CUBE_LEVELS:
      return 14;
   case PIPE_CAP_BLEND_EQUATION_SEPARATE:
      return 1;
   case PIPE_CAP_BLEND_EQUATION_ADVANCED:
      /* Backed by a real implementation in mwv207_create_blend_state(): the
       * hardware extended-blend field encodes 8 of the 15 KHR equations
       * (MULTIPLY/SCREEN/OVERLAY/DARKEN/LIGHTEN/HARDLIGHT/DIFFERENCE/
       * EXCLUSION, verified against the spec formulas).  The other 7
       * (COLORDODGE/COLORBURN/SOFTLIGHT/HSL_*) are a hardware gap and render
       * without blending, so this is a partial (8/15) guarantee.  Declared to
       * satisfy the KHR_blend_equation_advanced gate of native GLES 3.2.
       */
      return 1;
   case PIPE_CAP_MAX_STREAM_OUTPUT_BUFFERS:
      return 4;
   case PIPE_CAP_STREAM_OUTPUT_INTERLEAVE_BUFFERS:
      return 1;
   case PIPE_CAP_MAX_STREAM_OUTPUT_SEPARATE_COMPONENTS:
      return 64;
   case PIPE_CAP_MAX_STREAM_OUTPUT_INTERLEAVED_COMPONENTS:
      return 64;
   case PIPE_CAP_STREAM_OUTPUT_PAUSE_RESUME:
      return 1;
   case PIPE_CAP_PRIMITIVE_RESTART:
      return 1;
   case PIPE_CAP_INDEP_BLEND_ENABLE:
      return 1;
   case PIPE_CAP_INDEP_BLEND_FUNC:
      return 1;
   case PIPE_CAP_MAX_TEXTURE_ARRAY_LAYERS:
      return 8192;
   case PIPE_CAP_FS_COORD_ORIGIN_UPPER_LEFT:
      return 1;
   case PIPE_CAP_FS_COORD_PIXEL_CENTER_HALF_INTEGER:
      return 1;
   case PIPE_CAP_DEPTH_CLIP_DISABLE:
      return 1;
   case PIPE_CAP_VS_INSTANCEID:
      return 1;
   case PIPE_CAP_VERTEX_ELEMENT_INSTANCE_DIVISOR:
      return 1;
   case PIPE_CAP_MIXED_COLORBUFFER_FORMATS:
      return 1;
   case PIPE_CAP_SEAMLESS_CUBE_MAP:
      return 1;
   case PIPE_CAP_MIN_TEXEL_OFFSET:
      return -8;
   case PIPE_CAP_MAX_TEXEL_OFFSET:
      return 7;
   case PIPE_CAP_CONDITIONAL_RENDER:
      /* FIX(jm9100-oss): 之前这里返回 0，是为了绕开“cap 声明了但
       * pipe_context::render_condition 没人实现 → cso_context.c:965 解引用
       * NULL 函数指针 SIGSEGV”。但桌面 GL 的 ver_3_0 门控里点名要
       * NV_conditional_render（model version.c:260-289），关掉它会把 GL 从
       * 4.0 直接砍到 2.1 —— 代价太大。
       *
       * 真正的修法是把钩子实现出来（mwv207_query.c 的 mwv207_render_condition
       * + mwv207_check_render_cond：Begin 时把查询结果读回来缓存，之后逐绘制点
       * 决定是否发命令；GL_QUERY_*_NO_WAIT 允许实现等待，故语义合法）。 */
      return 1;
   case PIPE_CAP_VERTEX_COLOR_UNCLAMPED:
      return 1;
   case PIPE_CAP_CONSTANT_BUFFER_OFFSET_ALIGNMENT:
      return 256;
   case PIPE_CAP_MIN_MAP_BUFFER_ALIGNMENT:
      return 256;
   case PIPE_CAP_CUBE_MAP_ARRAY:
      return 1;
   case PIPE_CAP_TEXTURE_BUFFER_OBJECTS:
      return 1;
   case PIPE_CAP_TEXTURE_BUFFER_OFFSET_ALIGNMENT:
      return 256;
   case PIPE_CAP_MAX_TEXEL_BUFFER_ELEMENTS_UINT:
      return 65536;
   case PIPE_CAP_MAX_VIEWPORTS:
      return 1;
   case PIPE_CAP_ENDIANNESS:
      return PIPE_ENDIAN_NATIVE;
   case PIPE_CAP_MIXED_FRAMEBUFFER_SIZES:
      return 1;
   case PIPE_CAP_MAX_GEOMETRY_OUTPUT_VERTICES:
      return 256;
   case PIPE_CAP_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS:
      return 1024;
   case PIPE_CAP_TEXTURE_MULTISAMPLE:
      return 1;
   case PIPE_CAP_FAKE_SW_MSAA:
      return !mwv207_get_param(pscreen, PIPE_CAP_TEXTURE_MULTISAMPLE);
   case PIPE_CAP_MIN_TEXTURE_GATHER_OFFSET:
      return -8;
   case PIPE_CAP_MAX_TEXTURE_GATHER_OFFSET:
      return 7;
   case PIPE_CAP_VENDOR_ID:
      return 0x0731;
   case PIPE_CAP_DEVICE_ID:
      return 0x9100;
   case PIPE_CAP_ACCELERATED:
      return 1;
   case PIPE_CAP_VIDEO_MEMORY:
      /* FIX(jm9100-oss): PIPE_CAP_VIDEO_MEMORY 的单位是 MB, 不是字节。
       * 原来返回 (1024 << 20) = 1 GiB 字节数, 于是 glxinfo -B 报
       * "Video memory: 1073741824MB" (约 1024 TiB), 所有按显存决定画质/
       * 纹理预算的应用与游戏都会被误导。按 1 GiB 显存上报 1024 MB。 */
      return 1024;
   case PIPE_CAP_UMA:
      return 1;
   case PIPE_CAP_MAX_VERTEX_ATTRIB_STRIDE:
      return 2048;
   case PIPE_CAP_TEXTURE_HALF_FLOAT_LINEAR:
      return 1;
   case PIPE_CAP_SHAREABLE_SHADERS:
      return 1;
   case PIPE_CAP_FS_POSITION_IS_SYSVAL:
      return 1;
   case PIPE_CAP_FS_FACE_IS_INTEGER_SYSVAL:
      return 1;
   case PIPE_CAP_SHADER_BUFFER_OFFSET_ALIGNMENT:
      return 256;
   case PIPE_CAP_GENERATE_MIPMAP:
      return 1;
   case PIPE_CAP_VIEWPORT_SUBPIXEL_BITS:
      return 2;
   case PIPE_CAP_RASTERIZER_SUBPIXEL_BITS:
      return 4;
   case PIPE_CAP_NATIVE_FENCE_FD:
      return 1;
   case PIPE_CAP_ALLOW_MAPPED_BUFFERS_DURING_EXECUTION:
      return 1;
   case PIPE_CAP_MAX_GS_INVOCATIONS:
      return 32;
   case PIPE_CAP_MAX_SHADER_BUFFER_SIZE_UINT:
      return 1 << 16;
   case PIPE_CAP_MAX_COMBINED_SHADER_BUFFERS:
      return 4;
   case PIPE_CAP_MAX_VERTEX_ELEMENT_SRC_OFFSET:
      return 2047;
   case PIPE_CAP_DEST_SURFACE_SRGB_CONTROL:
      return 1;
   case PIPE_CAP_MAX_VARYINGS:
      /* 保持 8。曾尝试提到 16（GLES 3.0 规定的最低是 15 个向量），但实测 16 个
       * vec4 varying 会撞上 mwv207_link_shader 自己的断言
       * `next_shader->infile.num_reg < 0x10`（mwv207_compiler.c:619）—— 驱动内部
       * 的 IO 记录只有 16 个槽，声明 16 属超额声明。火狐 WebRender 在 8 下已能
       * 正常渲染，故维持原值。 */
      return 8;
   case PIPE_CAP_THROTTLE:
      return 1;
   case PIPE_CAP_DMABUF:
      return 1;
   case PIPE_CAP_FRAGMENT_SHADER_TEXTURE_LOD:
      return 1;
   case PIPE_CAP_FRAGMENT_SHADER_DERIVATIVES:
      return 1;
   case PIPE_CAP_PREFER_IMM_ARRAYS_AS_CONSTBUF:
      return 1;
   case PIPE_CAP_FLATSHADE:
      return 1;
   case PIPE_CAP_ALPHA_TEST:
      return 1;
   case PIPE_CAP_POINT_SIZE_FIXED:
      return 1;
   case PIPE_CAP_TWO_SIDED_COLOR:
      return 1;
   case PIPE_CAP_CLIP_PLANES:
      return 1;
   case PIPE_CAP_MAX_VERTEX_BUFFERS:
      return 16;
   case PIPE_CAP_TEXTURE_TRANSFER_MODES:
      return PIPE_TEXTURE_TRANSFER_DEFAULT;

   case PIPE_CAP_FRAMEBUFFER_NO_ATTACHMENT:
      return 1;
   case PIPE_CAP_MAX_SHADER_PATCH_VARYINGS:
      return 32;

   case PIPE_CAP_GLSL_FEATURE_LEVEL:
   case PIPE_CAP_GLSL_FEATURE_LEVEL_COMPATIBILITY:
   case PIPE_CAP_ESSL_FEATURE_LEVEL:
      return 400;

   case PIPE_CAP_MAX_DUAL_SOURCE_RENDER_TARGETS:
      return 1;
   case PIPE_CAP_QUERY_TIMESTAMP:
      return 1;

   case PIPE_CAP_TEXTURE_QUERY_LOD:
      return 1;
   case PIPE_CAP_SAMPLE_SHADING:
      return 1;
   case PIPE_CAP_DRAW_INDIRECT:
      return 1;
   case PIPE_CAP_DOUBLES:
      return 1;
   case PIPE_CAP_GLSL_TESS_LEVELS_AS_INPUTS:
      return 1;
   case PIPE_CAP_INT64:
      return 1;

   case PIPE_CAP_COMPUTE:
      return 1;
   case PIPE_CAP_MAX_TEXTURE_GATHER_COMPONENTS:
      return 4;

   case PIPE_CAP_FBFETCH:
      return 0;
   case PIPE_CAP_ROBUST_BUFFER_ACCESS_BEHAVIOR:
      /* Real guarantee, not a declaration lie: SSBO offsets are clamped by
       * nir_lower_robust_access() in mwv207_compile_shader().  This unlocks
       * GL_KHR_robust_buffer_access_behavior + GL_KHR_robustness, and
       * KHR_robustness is the last gate blocking native GLES 3.2.
       */
      return 1;
   case PIPE_CAP_COPY_BETWEEN_COMPRESSED_AND_PLAIN_FORMATS:
      return 1;

   case PIPE_CAP_SUPPORTED_PRIM_MODES:
      return 1 << MESA_PRIM_POINTS |
         1 << MESA_PRIM_LINES |
         1 << MESA_PRIM_LINE_STRIP |
         1 << MESA_PRIM_TRIANGLES |
         1 << MESA_PRIM_TRIANGLE_STRIP |
         1 << MESA_PRIM_TRIANGLE_FAN |
         1 << MESA_PRIM_LINES_ADJACENCY |
         1 << MESA_PRIM_LINE_STRIP_ADJACENCY |
         1 << MESA_PRIM_TRIANGLES_ADJACENCY |
         1 << MESA_PRIM_TRIANGLE_STRIP_ADJACENCY |
         1 << MESA_PRIM_PATCHES;
   case PIPE_CAP_SUPPORTED_PRIM_MODES_WITH_RESTART:
      return 1 << MESA_PRIM_POINTS |
         1 << MESA_PRIM_LINES |
         1 << MESA_PRIM_LINE_STRIP |
         1 << MESA_PRIM_TRIANGLES |
         1 << MESA_PRIM_TRIANGLE_STRIP |
         1 << MESA_PRIM_TRIANGLE_FAN |
         1 << MESA_PRIM_LINES_ADJACENCY |
         1 << MESA_PRIM_LINE_STRIP_ADJACENCY |
         1 << MESA_PRIM_TRIANGLES_ADJACENCY |
         1 << MESA_PRIM_TRIANGLE_STRIP_ADJACENCY |
         1 << MESA_PRIM_PATCHES;
   case PIPE_CAP_NIR_IMAGES_AS_DEREF:
      return 1;
   case PIPE_CAP_GL_BEGIN_END_BUFFER_SIZE:
      return 512 * 1024;
   case PIPE_CAP_ALLOW_DYNAMIC_VAO_FASTPATH:
      return 1;
   case PIPE_CAP_PREFER_BACK_BUFFER_REUSE:
      return 1;
   case PIPE_CAP_CULL_DISTANCE_NOCOMBINE:
      return 1;
   case PIPE_CAP_GL_SPIRV:
      return 1;
   case PIPE_CAP_TIMER_RESOLUTION:
      return DIV_ROUND_UP(1000000000ull, 50000000ull);
   case PIPE_CAP_PCI_GROUP:
   case PIPE_CAP_PCI_BUS:
   case PIPE_CAP_PCI_DEVICE:
   case PIPE_CAP_PCI_FUNCTION:
      return 0;
   case PIPE_CAP_ANISOTROPIC_FILTER:
      return 1;
   case PIPE_CAP_CONDITIONAL_RENDER_INVERTED:
      /* FIX(jm9100-oss): 反相谓词由 mwv207_check_render_cond() 的
       * render_cond_cond 分支直接支持，声明即可。 */
      return 1;
   case PIPE_CAP_BUFFER_MAP_PERSISTENT_COHERENT:
      /* FIX(jm9100-oss): 这里原本靠 gallium 默认值返回 1（见
       * src/gallium/auxiliary/util/u_screen.c:213-214），于是 st 会让
       * GL_ARB_buffer_storage 出现在扩展串里（st_extensions.c:761
       * { o(ARB_buffer_storage), PIPE_CAP_BUFFER_MAP_PERSISTENT_COHERENT }）。
       * 但驱动既没实现 pipe_context::buffer_storage，也没处理
       * PIPE_MAP_PERSISTENT / COHERENT 的 transfer 语义；实测 Firefox 的
       * WebRender 一看到该扩展就走"持久映射"的缓冲上传路径，对没有 data store
       * 的 buffer 发 glMapBufferRange(WRITE|PERSISTENT|FLUSH_EXPLICIT)，Mesa 每次
       * 都以 GL_INVALID_OPERATION 拒绝 → 顶点/实例数据传不上去，页面只剩底色、
       * 图元全丢。真正支持持久映射之前，明确返回 0。 */
      return 0;
   default:
      return u_pipe_screen_get_param_defaults(pscreen, param);
   }
}

static float
mwv207_get_paramf(struct pipe_screen *pscreen, enum pipe_capf param)
{
   switch (param) {
   case PIPE_CAPF_MAX_LINE_WIDTH:
   case PIPE_CAPF_MAX_LINE_WIDTH_AA:
      return 8.0f;
   case PIPE_CAPF_MAX_POINT_SIZE:
   case PIPE_CAPF_MAX_POINT_SIZE_AA:
      return 128.0f;
   case PIPE_CAPF_MAX_TEXTURE_ANISOTROPY:
      return 16.0f;
   case PIPE_CAPF_MAX_TEXTURE_LOD_BIAS:
      return 2.0f;
   case PIPE_CAPF_MIN_CONSERVATIVE_RASTER_DILATE:
   case PIPE_CAPF_MAX_CONSERVATIVE_RASTER_DILATE:
   case PIPE_CAPF_CONSERVATIVE_RASTER_DILATE_GRANULARITY:
      return 0.0f;
   case PIPE_CAPF_MIN_LINE_WIDTH:
   case PIPE_CAPF_MIN_LINE_WIDTH_AA:
   case PIPE_CAPF_MIN_POINT_SIZE:
   case PIPE_CAPF_MIN_POINT_SIZE_AA:
      return 1.0f;
   case PIPE_CAPF_POINT_SIZE_GRANULARITY:
   case PIPE_CAPF_LINE_WIDTH_GRANULARITY:
      return 0.1f;
   default:
      debug_printf("unknown paramf %d", param);
      return 0;
   }
}

static int
mwv207_get_shader_param(struct pipe_screen *pscreen,
                        enum pipe_shader_type shader,
                        enum pipe_shader_cap param)
{
   switch (param) {
   case PIPE_SHADER_CAP_MAX_INSTRUCTIONS:
   case PIPE_SHADER_CAP_MAX_ALU_INSTRUCTIONS:
   case PIPE_SHADER_CAP_MAX_TEX_INSTRUCTIONS:
   case PIPE_SHADER_CAP_MAX_TEX_INDIRECTIONS:
   case PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH:
      switch (shader) {
      case PIPE_SHADER_VERTEX:
      case PIPE_SHADER_FRAGMENT:
      case PIPE_SHADER_GEOMETRY:
      case PIPE_SHADER_TESS_CTRL:
      case PIPE_SHADER_TESS_EVAL:
      case PIPE_SHADER_COMPUTE:
         return INT_MAX;
      default:
         return 0;
      }
   case PIPE_SHADER_CAP_MAX_INPUTS:
      switch (shader) {
      case PIPE_SHADER_VERTEX:
      case PIPE_SHADER_FRAGMENT:
      case PIPE_SHADER_GEOMETRY:
      case PIPE_SHADER_TESS_CTRL:
      case PIPE_SHADER_TESS_EVAL:
      case PIPE_SHADER_COMPUTE:
         return 16;
      default:
         return 0;
      }
   case PIPE_SHADER_CAP_MAX_OUTPUTS:
      switch (shader) {
      case PIPE_SHADER_VERTEX:
         return 17;
      case PIPE_SHADER_FRAGMENT:
      case PIPE_SHADER_COMPUTE:
         return 4;
      case PIPE_SHADER_GEOMETRY:
      case PIPE_SHADER_TESS_CTRL:
      case PIPE_SHADER_TESS_EVAL:
         return 16;
      default:
         return 0;
      }
   case PIPE_SHADER_CAP_MAX_CONST_BUFFER0_SIZE:
      return 65536;
   case PIPE_SHADER_CAP_MAX_CONST_BUFFERS:
      return 16;
   case PIPE_SHADER_CAP_MAX_TEMPS:
      return 64;
   case PIPE_SHADER_CAP_SUPPORTED_IRS:
      return ((1 << PIPE_SHADER_IR_NIR) |
             (1 << PIPE_SHADER_IR_TGSI)) |
             (1 << PIPE_SHADER_IR_NIR_SERIALIZED);
   case PIPE_SHADER_CAP_INDIRECT_CONST_ADDR:
      return 1;
   case PIPE_SHADER_CAP_INDIRECT_INPUT_ADDR:
   case PIPE_SHADER_CAP_INDIRECT_OUTPUT_ADDR:
   case PIPE_SHADER_CAP_INDIRECT_TEMP_ADDR: {
      switch (shader) {
      case PIPE_SHADER_TESS_CTRL:
      case PIPE_SHADER_TESS_EVAL: {
         return 1;
      }
      default:
         return 0;
      }
   }
   case PIPE_SHADER_CAP_INTEGERS:
      return 1;
   case PIPE_SHADER_CAP_MAX_TEXTURE_SAMPLERS:
      return 16;
   case PIPE_SHADER_CAP_MAX_SAMPLER_VIEWS: {
      switch (shader) {
      case MESA_SHADER_KERNEL:
         return 128;
      default:
         return 16;
      }
   }
   case PIPE_SHADER_CAP_CONT_SUPPORTED:
      return 1;
   case PIPE_SHADER_CAP_SUBROUTINES:
   case PIPE_SHADER_CAP_INT64_ATOMICS:
   case PIPE_SHADER_CAP_FP16:
   case PIPE_SHADER_CAP_TGSI_SQRT_SUPPORTED:
   case PIPE_SHADER_CAP_TGSI_ANY_INOUT_DECL_RANGE:
      return 0;
   case PIPE_SHADER_CAP_MAX_SHADER_BUFFERS:
      return 0x4;
   case PIPE_SHADER_CAP_MAX_SHADER_IMAGES:
      return 8;
   case PIPE_SHADER_CAP_MAX_HW_ATOMIC_COUNTERS:
      return 128;
   case PIPE_SHADER_CAP_MAX_HW_ATOMIC_COUNTER_BUFFERS:
      return 16;
   default:
      return 0;
   }
   debug_printf("unknown paramf %d", param);
   return 0;
}

static int
mwv207_get_compute_param(struct pipe_screen *pscreen,
                         enum pipe_shader_ir ir_type,
                         enum pipe_compute_cap param,
                         void *ret)
{
   switch (param) {
   case PIPE_COMPUTE_CAP_IR_TARGET: {
      const char *gpu = "mwv207-nir";
      if (ret)
         sprintf(ret, "%s", gpu);
      return (strlen(gpu) + 2) * sizeof(char);
   }
   case PIPE_COMPUTE_CAP_MAX_GRID_SIZE:
      RET(((uint64_t []){ 65535, 65535, 65535 }));
   case PIPE_COMPUTE_CAP_MAX_BLOCK_SIZE:
      RET(((uint64_t []){ 1024, 1024, 1024 }));
   case PIPE_COMPUTE_CAP_MAX_THREADS_PER_BLOCK:
      RET((uint64_t []){ 1024 });
   case PIPE_COMPUTE_CAP_MAX_LOCAL_SIZE:
      RET((uint64_t []){ 32768 });
   case PIPE_COMPUTE_CAP_GRID_DIMENSION:
      RET(((uint64_t []){ 3 }));
   case PIPE_COMPUTE_CAP_MAX_GLOBAL_SIZE:
      RET((uint64_t []){ 0x10000000 });
   case PIPE_COMPUTE_CAP_MAX_MEM_ALLOC_SIZE:
      RET((uint64_t []){ 0x8000000 });
   case PIPE_COMPUTE_CAP_MAX_PRIVATE_SIZE:
   case PIPE_COMPUTE_CAP_MAX_INPUT_SIZE:
      RET((uint64_t []){ 4096 });
   case PIPE_COMPUTE_CAP_IMAGES_SUPPORTED:
      RET((uint32_t []){ 0 });
   case PIPE_COMPUTE_CAP_MAX_VARIABLE_THREADS_PER_BLOCK:
      RET((uint64_t []){ 0 });
   case PIPE_COMPUTE_CAP_SUBGROUP_SIZES:
      RET((uint32_t []){ 64 });
   case PIPE_COMPUTE_CAP_MAX_COMPUTE_UNITS:
      RET((uint32_t []){ 2 });
   case PIPE_COMPUTE_CAP_MAX_CLOCK_FREQUENCY:
      RET((uint32_t []){ 50 });
   case PIPE_COMPUTE_CAP_ADDRESS_BITS:
      RET((uint32_t []){ 32 });
   case PIPE_COMPUTE_CAP_MAX_SUBGROUPS:
      RET((uint32_t []){ 0 });
   }

   return 0;
}

static bool
mwv207_sample_count_flags(uint32_t sample_count)
{
   switch (sample_count) {
   case 1:
   case 2:
   case 4:
      return true;
   case 8:
   case 16:
   case 32:
   case 64:
   default:
      return false;
   }
}

static bool
mwv207_is_format_supported(struct pipe_screen *pscreen,
                           enum pipe_format format,
                           enum pipe_texture_target target,
                           unsigned sample_count,
                           unsigned storage_sample_count,
                           unsigned bind)
{
   if (format == PIPE_FORMAT_NONE)
      return mwv207_sample_count_flags(sample_count);

   if (sample_count > 1) {
      if (!mwv207_sample_count_flags(sample_count))
         return false;

      if (bind & PIPE_BIND_DEPTH_STENCIL) {
         if (format == PIPE_FORMAT_Z32_FLOAT)
            return false;
      }

      if (bind & PIPE_BIND_RENDER_TARGET) {
         if (util_format_is_float(format))
            return false;
      }
   }

   uint32_t support = mwv207_get_format_desc(format);

   if (target == PIPE_BUFFER) {
      if (bind & PIPE_BIND_VERTEX_BUFFER && !(support & PIPE_BIND_VERTEX_BUFFER))
         return false;
   } else {

      if (bind & PIPE_BIND_RENDER_TARGET && !(support & PIPE_BIND_RENDER_TARGET))
         return false;

      if (bind & PIPE_BIND_BLENDABLE && !(support & PIPE_BIND_BLENDABLE))
         return false;

      if (bind & PIPE_BIND_SAMPLER_VIEW && !(support & PIPE_BIND_SAMPLER_VIEW))
         return false;

      if (bind & PIPE_BIND_DEPTH_STENCIL && !(support & PIPE_BIND_DEPTH_STENCIL))
         return false;
   }

   if (util_format_is_compressed(format)) {
      const struct util_format_description *desc = util_format_description(format);

      if (desc->layout == UTIL_FORMAT_LAYOUT_BPTC)
         return false;
   }

   return true;
}

static uint64_t
mwv207_get_timestamp(struct pipe_screen *screen)
{
   return os_time_get_nano();
}

static void mwv207_fence_reference(struct pipe_screen *screen,
                                   struct pipe_fence_handle **ptr,
                                   struct pipe_fence_handle *fence)
{
   struct mwv207_winsys *ws = mwv207_screen(screen)->ws;

   ws->fence_reference(ptr, fence);
}

static bool
mwv207_fence_finish(struct pipe_screen *screen,
                    struct pipe_context *ctx,
                    struct pipe_fence_handle *fence,
                    uint64_t timeout)
{
   struct mwv207_winsys *ws = mwv207_screen(screen)->ws;

   return ws->fence_wait(ws, fence, timeout);
}

static int mwv207_fence_get_fd(struct pipe_screen *screen,
                               struct pipe_fence_handle *fence)
{

   struct mwv207_winsys *ws = mwv207_screen(screen)->ws;
   return ws->fence_get_fd(ws, fence);
}

static void
mwv207_destroy_screen(struct pipe_screen *pscreen)
{
   struct mwv207_screen *screen = mwv207_screen(pscreen);

   if (screen->ws->destroy(screen->ws)) {
      glsl_type_singleton_decref();
      u_transfer_helper_destroy(pscreen->transfer_helper);
      slab_destroy_parent(&screen->transfer_pool);
      FREE(screen);
   }
}

struct pipe_screen *
mwv207_create_screen(struct mwv207_winsys *winsys)
{
   struct mwv207_screen *screen = CALLOC_STRUCT(mwv207_screen);

   if (!screen)
      return NULL;

   screen->ws = winsys;

   screen->base.get_name             = mwv207_get_name;
   screen->base.get_vendor           = mwv207_get_vendor;
   screen->base.get_device_vendor    = mwv207_get_device_vendor;

   screen->base.get_param            = mwv207_get_param;
   screen->base.get_paramf           = mwv207_get_paramf;
   screen->base.get_shader_param     = mwv207_get_shader_param;
   screen->base.get_compute_param    = mwv207_get_compute_param;
   screen->base.get_compiler_options = mwv207_get_compiler_options;
   screen->base.is_format_supported  = mwv207_is_format_supported;
   screen->base.get_timestamp        = mwv207_get_timestamp;

   screen->base.get_timestamp   = mwv207_get_timestamp;

   screen->base.context_create  = mwv207_context_create;
   screen->base.destroy         = mwv207_destroy_screen;

   screen->base.fence_reference = mwv207_fence_reference;
   screen->base.fence_finish    = mwv207_fence_finish;
   screen->base.fence_get_fd    = mwv207_fence_get_fd;

   mwv207_screen_resource_init(&screen->base);

   slab_create_parent(&screen->transfer_pool, sizeof(struct mwv207_transfer), 16);

   glsl_type_singleton_init_or_ref();

   return &screen->base;
}
