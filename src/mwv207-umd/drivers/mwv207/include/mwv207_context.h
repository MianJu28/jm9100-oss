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
#ifndef MWV207_CONTEXT_H
#define MWV207_CONTEXT_H

#include "util/slab.h"
#include "pipe/p_context.h"
#include "pipe/p_state.h"

#include "mwv207_resource.h"
#include "mwv207_surface.h"
#include "mwv207_state.h"

struct blitter_context;
struct primconvert_context;
struct list_head;

struct mwv207_blend_state;
struct mwv207_depth_stencil_alpha_state;
struct mwv207_rasterizer_state;
struct mwv207_resource;
struct mwv207_elements_state;
struct mwv207_sampler;
struct mwv207_query_info;
struct mwv207_sampler_view;
struct mwv207_stream_output;
struct mwv207_global_bindings_state;

enum mwv207_dirty {
   MWV207_DIRTY_BLEND           = (1 << 0),
   MWV207_DIRTY_SAMPLERS        = (1 << 1),
   MWV207_DIRTY_RASTERIZER      = (1 << 2),
   MWV207_DIRTY_ZSA             = (1 << 3),
   MWV207_DIRTY_VERTEX_ELEMENTS = (1 << 4),
   MWV207_DIRTY_BLEND_COLOR     = (1 << 6),
   MWV207_DIRTY_STENCIL_REF     = (1 << 7),
   MWV207_DIRTY_SAMPLE_MASK     = (1 << 8),
   MWV207_DIRTY_VIEWPORT        = (1 << 9),
   MWV207_DIRTY_FRAMEBUFFER     = (1 << 10),
   MWV207_DIRTY_SCISSOR         = (1 << 11),
   MWV207_DIRTY_SAMPLER_VIEWS   = (1 << 12),
   MWV207_DIRTY_CONSTBUF        = (1 << 13),
   MWV207_DIRTY_VERTEX_BUFFERS  = (1 << 14),
   MWV207_DIRTY_INDEX_BUFFER    = (1 << 15),
   MWV207_DIRTY_SHADER          = (1 << 16),
   MWV207_DIRTY_TS              = (1 << 17),
   MWV207_DIRTY_TEXTURE_CACHES  = (1 << 18),
   MWV207_DIRTY_DERIVE_TS       = (1 << 19),
   MWV207_DIRTY_STREAM_OUT      = (1 << 20),

   MWV207_DIRTY_ALL             = 0xFFFFFFFF,
};

struct mwv207_dual_source_blend_info {
   bool is_dual;
   uint32_t sampler_index;
   int32_t y_invert;
   struct pipe_rt_blend_state  blend_state;
};

struct mwv207_pipe_info {
   bool tex_128bit;
   bool tg4_lower;
   uint32_t tex_record[PIPE_SHADER_TYPES][PIPE_MAX_SHADER_SAMPLER_VIEWS];
   bool pixel_128bit;
   uint32_t pixel_record[PIPE_MAX_COLOR_BUFS];
   uint32_t pixel_num;
   bool z32_shadow;
   bool texture_dirty;
   bool linear_outmode;
   bool frag_rgba_swap;
   uint32_t swizzle_record[PIPE_MAX_COLOR_BUFS][4];
   uint32_t cbufs_map[PIPE_MAX_COLOR_BUFS];
   bool has_render_arrary;
   uint32_t vertices_per_patch;
   uint32_t alloc_page[PIPE_SHADER_TYPES];
   uint32_t max_thread[PIPE_SHADER_TYPES];
   uint32_t patchs[2];
   uint32_t grid[3];
   bool subsamplez_dirty;
   uint8_t rast_samples;
   struct mwv207_dual_source_blend_info dsbi;
};

struct mwv207_context {
   struct pipe_context base;

   struct mwv207_winsys *ws;

   struct mwv207_cmd *cmd;
   struct mwv207_cmd *dma;

   uint32_t num_vertex_buffers;
   struct mwv207_elements_state *element_state;
   struct pipe_vertex_buffer vertex_buffer[PIPE_MAX_ATTRIBS];
   struct pipe_constant_buffer cb[PIPE_SHADER_TYPES][PIPE_MAX_CONSTANT_BUFFERS];

   struct pipe_viewport_state viewport;
   struct pipe_scissor_state scissor;

   struct pipe_framebuffer_state fb_state;

   /* 双源混合在硬件上没有 SRC1 因子, 只能把混合降级进片元着色器, 并且每个
    * dual draw 都要把渲染目标(按 scissor 裁剪)拷一份给着色器 texelFetch 用。
    * 拷贝目标纹理与采样视图按 (宽, 高, 格式) 缓存复用, 避免每 draw 新建/销毁。 */
   struct pipe_resource *dual_copy_tex;
   struct pipe_sampler_view *dual_copy_view;
   unsigned dual_copy_w, dual_copy_h;
   enum pipe_format dual_copy_fmt;

   struct mwv207_rasterizer_state *rast_state;
   struct mwv207_depth_stencil_alpha_state *zsa;
   struct mwv207_stencil_ref stencil_ref;
   struct mwv207_blend_state *blend_state;
   struct pipe_blend_color blend_color;

   struct pipe_shader_buffer atomic_bo[16];
   uint32_t atomic_mask;

   /* 上传暂存: transfer_unmap() 把 CPU 数据 blit 进 tiled 纹理后, staging
    * 资源不能立刻让 pb_cache 回收 —— GPU 可能还没读完它 (竞态会撕裂纹理,
    * 表现为 Firefox 字形/图标错乱)。先挂在这里, 环满时再等待最老的一个。 */
   struct pipe_resource *retired_staging[16];
   unsigned retired_staging_next;
   struct pipe_shader_buffer ssbo[PIPE_SHADER_TYPES][PIPE_MAX_SHADER_BUFFERS];
   uint32_t ssbo_mask[PIPE_SHADER_TYPES];
   struct pipe_image_view ssimage[PIPE_SHADER_TYPES][PIPE_MAX_SHADER_IMAGES];
   uint32_t num_images[PIPE_SHADER_TYPES];

   struct pipe_shader_buffer local_memory;

   uint32_t sample_mask;
   uint32_t num_samplers[PIPE_SHADER_TYPES];
   uint32_t num_sampler_views[PIPE_SHADER_TYPES];

   struct mwv207_sampler *samplers[PIPE_SHADER_TYPES][PIPE_MAX_SAMPLERS];
   struct pipe_sampler_view *sampler_views[PIPE_SHADER_TYPES][PIPE_MAX_SHADER_SAMPLER_VIEWS];

   struct mwv207_shader_state *shader_states[PIPE_SHADER_TYPES];

   float tess_outer_level[4];
   float tess_inner_level[2];

   uint32_t dirty;
   int      in_fence_fd;

   /* FIX(jm9100-oss): E2 —— 绘制序号必须跟着 context 走。
    * 原来 mwv207_config_draw() 里写的是函数内 static uint32_t draw_count，
    * 被所有 context 共享：多 context（应用多线程 + 驱动内部 blit/query
    * context）时序号会互相串，且每帧递增无上限。硬件侧 0xE27
    * (MWV207REG_DRAW_ID) 是 32 位自由计数器，语义上就是"这次绘制是第几次"，
    * 每个 context 各自计数即可。 */
   uint32_t draw_count;

   /* 影响着色器编译的状态"指纹"。mwv207_submit() 会把 ctx->dirty 重置成
    * MWV207_DIRTY_ALL（新 cmdbuf 必须重发全部状态），因此 dirty 不能用来
    * 判断"某个状态是否真的变了"——用它做判据会导致每次绘制都重编译着色器。
    * 这里给每种会改变着色器二进制的状态各记一个内容指纹（不是调用次数），
    * mwv207_set_current_shader() 拿指纹当缓存键的一部分，不同布局的着色器
    * 各自成为独立缓存条目，只有指纹变化时才需要新编译一份。
    * 指纹由 mwv207_update_shader() 每次绘制前重算。FIX(jm9100-oss) */
   uint32_t fp_tex_128bit[PIPE_SHADER_TYPES];   /* 各 stage 视图 128 位取样模式的指纹 */
   uint32_t fp_pixel_128bit; /* 颜色附件 128 位模式的指纹 */
   uint32_t fp_z32_shadow[PIPE_SHADER_TYPES];   /* 各 stage 采样器比较模式的指纹 */
   uint32_t fp_is_dual;      /* 双源混合开关指纹 */
   /* FIX(jm9100-oss): 双源混合在硬件上没有 SRC1 因子(见 mwv207_blend.c 的
    * alpha_func[] 表), 只能把混合降级进片元着色器, 而 lower_dual 会把
    * 因子/方程/blend color 直接烘焙进二进制。因此这些内容也必须进缓存键,
    * 否则先编译出来的那一份会被后面的不同混合状态复用 -> 颜色全错
    * (火狐 LCD 文字被彩色竖条覆盖的根因)。 */
   uint32_t fp_dual_blend;   /* 双源混合参数(因子/方程/blend color)的指纹 */
   /* 指纹只在相关状态（采样视图/采样器/颜色附件）真的变动时重算：
    * 每次绘制都重扫 PIPE_MAX_SHADER_SAMPLER_VIEWS × stage 约 200 次哈希，
    * 实测占每个 draw 的 0.76 us（占 config 阶段 70%）。FIX(jm9100-oss) */
   bool fp_dirty;

   struct mwv207_stream_output stream_out;

   struct mwv207_query_info *query_info;

   /* 条件渲染（GL_NV_conditional_render），见 mwv207_check_render_cond()。
    * FIX(jm9100-oss) */
   struct pipe_query *render_cond_query;
   enum pipe_render_cond_flag render_cond_mode;
   bool render_cond_cond;
   bool render_cond_cached;
   bool render_cond_pass;

   struct blitter_context *blitter;
   struct primconvert_context *primconvert;
   struct slab_child_pool transfer_pool;

   struct mwv207_pipe_info info;

   struct mwv207_global_bindings_state global_bindings;
};

static inline struct mwv207_context *
mwv207_context(struct pipe_context *context)
{
   return (struct mwv207_context *)context;
}

struct pipe_context *
mwv207_context_create(struct pipe_screen *pscreen, void *priv, unsigned flags);

void
mwv207_draw_vbo(struct pipe_context *pipe,
                const struct pipe_draw_info *info,
                unsigned drawid_offset,
                const struct pipe_draw_indirect_info *indirect,
                const struct pipe_draw_start_count_bias *draws,
                unsigned num_draws);

void
mwv207_context_compute_init(struct pipe_context *pctx);

#endif
