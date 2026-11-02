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

   struct mwv207_rasterizer_state *rast_state;
   struct mwv207_depth_stencil_alpha_state *zsa;
   struct mwv207_stencil_ref stencil_ref;
   struct mwv207_blend_state *blend_state;
   struct pipe_blend_color blend_color;

   struct pipe_shader_buffer atomic_bo[16];
   uint32_t atomic_mask;
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
   uint32_t fp_is_dual;      /* 双源混合指纹 */

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
