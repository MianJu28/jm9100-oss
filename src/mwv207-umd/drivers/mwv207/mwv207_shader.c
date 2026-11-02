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
#include "nir/tgsi_to_nir.h"
#include "nir/nir_serialize.h"
#include "tgsi/tgsi_dump.h"
#include "tgsi/tgsi_from_mesa.h"

#include "mwv207_cmd.h"
#include "mwv207_context.h"
#include "mwv207_shader.h"
#include "mwv207_screen.h"
#include "mwv207_compiler.h"
#include "mwv207_common.h"

static const uint32_t mwv207_gs_type[] = {
   0x1,
   0x2,
   0x7,
   0x3,
   0x4,
   0x5,
   0x6,
   0x8,
   0x8,
   0x8,
   0x9,
   0xA,
   0xB,
   0xC,
   0xD,
};

static int
mwv207_get_gs_vertices(struct mwv207_context *ctx)
{

   uint32_t gs_vertices = 3;
   struct mwv207_shader_state *state;

   if (ctx->shader_states[PIPE_SHADER_TESS_CTRL]) {
      state = ctx->shader_states[PIPE_SHADER_TESS_EVAL];

      if (state->nir->info.tess.point_mode)
         gs_vertices = 1;
      else if (state->nir->info.tess._primitive_mode == TESS_PRIMITIVE_ISOLINES)
         gs_vertices = 2;
      else
         gs_vertices = 3;
   } else if (ctx->shader_states[PIPE_SHADER_GEOMETRY]) {
      state = ctx->shader_states[PIPE_SHADER_GEOMETRY];

      switch (state->nir->info.gs.output_primitive) {
      case MESA_PRIM_POINTS:
         gs_vertices = 1;
         break;

      case MESA_PRIM_LINES:
      case MESA_PRIM_LINE_LOOP:
      case MESA_PRIM_LINE_STRIP:
         gs_vertices = 2;
         break;

      case MESA_PRIM_TRIANGLES:
      case MESA_PRIM_TRIANGLE_STRIP:
      case MESA_PRIM_TRIANGLE_FAN:
         gs_vertices = 3;
         break;

      case MESA_PRIM_LINES_ADJACENCY:
      case MESA_PRIM_LINE_STRIP_ADJACENCY:
         gs_vertices = 4;
         break;

      case MESA_PRIM_TRIANGLES_ADJACENCY:
      case MESA_PRIM_TRIANGLE_STRIP_ADJACENCY:
         gs_vertices = 6;
         break;

      case MESA_PRIM_PATCHES:
         gs_vertices = ctx->info.vertices_per_patch;
         break;

      default:
         gs_vertices = 3;
         break;
      }
   }

   return gs_vertices;
}

static void
mwv207_config_usc(struct mwv207_context *ctx)
{
   uint32_t gs_thread = 64;

   if (ctx->shader_states[PIPE_SHADER_GEOMETRY]) {
      ctx->info.alloc_page[PIPE_SHADER_VERTEX]   = 17;
      ctx->info.alloc_page[PIPE_SHADER_GEOMETRY] = 12;
   }

   if (ctx->shader_states[PIPE_SHADER_TESS_CTRL]) {
      if (ctx->shader_states[PIPE_SHADER_GEOMETRY]) {
         ctx->info.alloc_page[PIPE_SHADER_VERTEX]    = 5;
         ctx->info.alloc_page[PIPE_SHADER_TESS_CTRL] = 5;
         ctx->info.alloc_page[PIPE_SHADER_TESS_EVAL] = 9;
         ctx->info.alloc_page[PIPE_SHADER_GEOMETRY]  = 8;
      } else {
         ctx->info.alloc_page[PIPE_SHADER_VERTEX]    = 8;
         ctx->info.alloc_page[PIPE_SHADER_TESS_CTRL] = 8;
         ctx->info.alloc_page[PIPE_SHADER_TESS_EVAL] = 12;
      }
   }

   if (ctx->shader_states[PIPE_SHADER_GEOMETRY]) {
      struct mwv207_shader_state *gs = ctx->shader_states[PIPE_SHADER_GEOMETRY];
      uint32_t output_num = gs->nir->num_outputs + (gs->current->vs_pos_out_reg == -1 ? 1 : 0);
      uint32_t gs_out_size = output_num * 16 * gs->nir->info.gs.vertices_out;

      if (gs_out_size == 0)
         gs_out_size = 16;

      gs_thread = (ctx->info.alloc_page[PIPE_SHADER_GEOMETRY] * 1024 - 640) / gs_out_size;
      gs_thread = MIN2(gs_thread, 64);

      ctx->info.max_thread[PIPE_SHADER_GEOMETRY] = gs_thread;
      ctx->info.alloc_page[PIPE_SHADER_GEOMETRY] += 1;
   }

   if (ctx->shader_states[PIPE_SHADER_TESS_EVAL]) {
      struct mwv207_shader_state *tes = ctx->shader_states[PIPE_SHADER_TESS_EVAL];
      uint32_t tes_thread = 0;
      uint32_t tes_out_size = (tes->current->outfile.num_reg + 1) * 16;
      uint32_t max_thread = (ctx->info.alloc_page[PIPE_SHADER_TESS_EVAL] * 1024 / tes_out_size);

      uint32_t next_stream_vertices = 3;

      if (ctx->shader_states[PIPE_SHADER_GEOMETRY])
         next_stream_vertices = gs_thread * mwv207_get_gs_vertices(ctx);

      max_thread = max_thread - 3 - (next_stream_vertices - 1);
      tes_thread = max_thread / 5 * 4;

      if (tes_thread > 64)
         tes_thread = max_thread / 6 * 4;

      ctx->info.max_thread[PIPE_SHADER_TESS_EVAL] = MIN2(tes_thread, 64);
      ctx->info.alloc_page[PIPE_SHADER_TESS_EVAL] += 1;
   }

   if (ctx->shader_states[PIPE_SHADER_TESS_CTRL]) {
      struct mwv207_shader_state *tcs = ctx->shader_states[PIPE_SHADER_TESS_CTRL];
      uint32_t tcs_vertices_out = tcs->nir->info.tess.tcs_vertices_out;
      uint32_t patch_offset = tcs->current->output_num * tcs_vertices_out;
      uint32_t patch_size = (patch_offset + tcs->current->patch_num) * 16;
      int max_tcs_patch = MIN2(512 / ctx->info.vertices_per_patch, 64 / tcs->nir->info.tess.tcs_vertices_out);
      int patchs = ctx->info.alloc_page[PIPE_SHADER_TESS_CTRL] * 1024 / patch_size;
      int max_tes_patch = patchs - max_tcs_patch + 1;

      if (max_tes_patch <= 0) {
         max_tes_patch = 1;
         max_tcs_patch = patchs;
      }

      ctx->info.patchs[0x0] = MIN2(max_tcs_patch, 64);
      ctx->info.patchs[0x1] = MIN2(max_tes_patch, 64);
      ctx->info.alloc_page[PIPE_SHADER_TESS_CTRL] += 1;
   }

   if (ctx->shader_states[PIPE_SHADER_VERTEX]) {
      struct mwv207_shader_state *vs = ctx->shader_states[PIPE_SHADER_VERTEX];
      uint32_t vs_thread = 0;
      uint32_t vs_out_size = (vs->current->outfile.num_reg + 1) * 16;
      uint32_t max_thread = (ctx->info.alloc_page[PIPE_SHADER_VERTEX] * 1024 / vs_out_size);

      uint32_t next_stream_vertices = 3;

      if (ctx->shader_states[PIPE_SHADER_TESS_CTRL])
         next_stream_vertices = ctx->info.patchs[0x0] * ctx->info.vertices_per_patch;
      else if (ctx->shader_states[PIPE_SHADER_GEOMETRY])
         next_stream_vertices = gs_thread * mwv207_get_gs_vertices(ctx);
      else {

         ctx->info.alloc_page[PIPE_SHADER_VERTEX] = 31;
         ctx->info.max_thread[PIPE_SHADER_VERTEX] = 512;
         return;
      }

      max_thread = max_thread - 3 - (next_stream_vertices - 1);
      vs_thread = max_thread / 5 * 4;

      if (vs_thread > 64)
         vs_thread = max_thread / 3 * 2;

      ctx->info.max_thread[PIPE_SHADER_VERTEX] = vs_thread;
      ctx->info.alloc_page[PIPE_SHADER_VERTEX] += 1;
   }
}

void
mwv207_config_vs(struct mwv207_context *ctx)
{
   uint32_t  *cmd_buf;
   uint32_t input[8] = {0};
   struct mwv207_shader_state *state = ctx->shader_states[PIPE_SHADER_VERTEX];
   struct mwv207_shader *vs = state->current;
   struct mwv207_shader *fs = ctx->shader_states[PIPE_SHADER_FRAGMENT]->current;
   uint32_t time_out = align(vs->infile.num_reg * 4 + 4, 16) / 16;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   for (int i = 0; i < vs->infile.num_reg; i++)
      mwv207_config_arrays(input, 8, i, vs->infile.reg[i].reg);

   if (vs->infile.num_reg == 0) {
      MWV207_EMIT(cmd_buf, 0x202,
         (ALIGN_BIT((uint32_t) (1), 0x0) & (0x3F)) |
         (ALIGN_BIT((uint32_t) (1), 0x8) & (0x1F00)) |
         (ALIGN_BIT((uint32_t) (1), 0x1F) & (0x80000000)));

      MWV207_EMIT(cmd_buf, 0x1F1,
         (ALIGN_BIT(0x3, 0x0) & (0x3)) |
         (ALIGN_BIT((uint32_t) (0), 0x8) & (0x7F00)) |
         (ALIGN_BIT((uint32_t) (1), 0x10) & (0x7F0000)));
   } else if (vs->vs_id_in_reg != -1) {
      mwv207_config_arrays(input, 8, vs->infile.num_reg, vs->vs_id_in_reg);
      MWV207_EMIT(cmd_buf, 0x202,
         (ALIGN_BIT((uint32_t) (vs->infile.num_reg), 0x0) & (0x3F)) |
         (ALIGN_BIT((uint32_t) (time_out), 0x8) & (0x1F00)) |
         (ALIGN_BIT((uint32_t) (1), 0x1F) & (0x80000000)));

      MWV207_EMIT(cmd_buf, 0x1F1,
         (ALIGN_BIT(0x3, 0x0) & (0x3)) |
         (ALIGN_BIT((uint32_t) (vs->infile.num_reg * 4), 0x8) & (0x7F00)) |
         (ALIGN_BIT((uint32_t) (vs->infile.num_reg * 4 + 1), 0x10) & (0x7F0000)));
   } else {
      MWV207_EMIT(cmd_buf, 0x202,
         (ALIGN_BIT((uint32_t) (vs->infile.num_reg), 0x0) & (0x3F)) |
         (ALIGN_BIT((uint32_t) (time_out), 0x8) & (0x1F00)));

      MWV207_EMIT(cmd_buf, 0x1F1, (ALIGN_BIT(0x0, 0x0) & (0x3)));
   }

   MWV207_EMITS(cmd_buf, 0x230, 8, input);

   int vertex_num = 0;

   if (vs->vs_pos_out_reg >= 0)
      vertex_num += 1;

   if (vs->vs_pointsize_out_reg != -1 && ctx->rast_state->base.point_size_per_vertex) {
      vertex_num += 1;

      if (fs->usePointCoord)
         vertex_num += 1;
   }

   mwv207_config_usc(ctx);

   uint32_t max_thread = ctx->info.max_thread[PIPE_SHADER_VERTEX];
   uint32_t alloc_pages = ctx->info.alloc_page[PIPE_SHADER_VERTEX];
   uint32_t res_window_size = max_thread / 4;

   if (max_thread > 64) {
      res_window_size = max_thread / 2;
      max_thread = 64;
   }

   MWV207_EMIT(cmd_buf, 0x21C,
       (ALIGN_BIT((uint32_t) (vs->outfile.num_reg + vertex_num), 0x0) & (0x3F)) |
       (ALIGN_BIT((uint32_t) (max_thread * (vs->outfile.num_reg + vertex_num)), 0x8) & (0x7FF00)));

   MWV207_EMIT(cmd_buf, 0x228,
      (ALIGN_BIT((uint32_t) (alloc_pages), 0x0) & (0x3F)) |
      (ALIGN_BIT((uint32_t) (max_thread), 0xC) & (0xFF000)) |
      (ALIGN_BIT((uint32_t) (res_window_size), 0x14) & (0x1FF00000)));

   MWV207_EMIT(cmd_buf, 0x5582, alloc_pages);

   MWV207_EMIT(cmd_buf, 0x203, vs->num_temps);

   MWV207_EMIT(cmd_buf, 0x22A, 0x10);
   MWV207_EMIT(cmd_buf, 0x219, (0x0 / 4));

   MWV207_EMIT(cmd_buf, 0x21A, (ALIGN_BIT(0x1, 0x0) & (0x1)));

   MWV207_EMIT(cmd_buf, 0x21D, 0);
   MWV207_EMIT(cmd_buf, 0x22F, (vs->code_size / 4));
   mwv207_add_relocs(ctx, &cmd_buf, 0x21B,
      state->bo, state->current->bo_offset, 0x1);
   MWV207_EMIT(cmd_buf, 0x5580, 0x00000002);
   MWV207_EMIT(cmd_buf, 0x5581, (vs->code_size / 4 - 1));
   MWV207_EMIT(cmd_buf, 0x20C, vs->vs_load_balancing);

   struct mwv207_shader_link_info link = {0};

   mwv207_link_shader(ctx, vs, &link);
   uint32_t output[8] = {0};
   unsigned index_offset = 1;
   if (vs->vs_pos_out_reg < 0)
      index_offset = 0;
   else
      mwv207_config_arrays(output, 8, 0, vs->vs_pos_out_reg);

   for (int i = 0; i < link.num_varyings_fixed; i++)
      mwv207_config_arrays(output, 8, i + index_offset, link.varyings[i].reg);

   if (vs->vs_pointsize_out_reg >= 0 && ctx->rast_state->base.point_size_per_vertex)
      mwv207_config_arrays(output, 8, link.num_varyings_fixed + index_offset, vs->vs_pointsize_out_reg);

   MWV207_EMITS(cmd_buf, 0x238, 8, output);

   uint32_t pcoord_varying_comp_ofs = 0x7f;

   if (link.pcoord_varying_comp_ofs != -1)
      pcoord_varying_comp_ofs = link.pcoord_varying_comp_ofs;

   uint32_t render_arrary_ofs = 0x7f;

   uint32_t output_add = index_offset;

   if (ctx->info.has_render_arrary && ctx->shader_states[PIPE_SHADER_GEOMETRY] &&
       ctx->shader_states[PIPE_SHADER_GEOMETRY]->current->gs_use_layer) {
      render_arrary_ofs = 0;
      if (vs->infile.num_reg == 0)
         output_add += 1;
   }

   uint32_t point_size_ops = 0;

   if (vs->vs_pointsize_out_reg != -1 && ctx->rast_state->base.point_size_per_vertex) {
      point_size_ops = 4 * (link.num_varyings + 1);
      output_add += 1;
   }

   MWV207_EMIT(cmd_buf, 0x201, link.num_varyings_fixed + output_add);
   MWV207_EMIT(cmd_buf, 0xE22, 0x7f000000 |
       (ALIGN_BIT((uint32_t) (render_arrary_ofs), 0x10) & (0x7F0000)) |
       (ALIGN_BIT((uint32_t) (pcoord_varying_comp_ofs), 0x8) & (0x7F00)) |
       (ALIGN_BIT((uint32_t) (point_size_ops), 0x0) & (0x7F)));

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

void
mwv207_config_tcs(struct mwv207_context *ctx)
{
   uint32_t  *cmd_buf;
   struct mwv207_shader_state *state = ctx->shader_states[PIPE_SHADER_TESS_CTRL];

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   if (state) {
      struct mwv207_shader *tcs = state->current;
      struct mwv207_shader_link_info link = {0};
      uint32_t tcs_vertices_out = state->nir->info.tess.tcs_vertices_out;
      uint32_t remap = 0x0;
      uint32_t control_point_size = tcs->output_num;
      uint32_t patch_offset = control_point_size * tcs_vertices_out;
      uint32_t patch_size = patch_offset + tcs->patch_num;
      uint32_t max_patch = ctx->info.patchs[0x0];

      if (tcs->info->nir->info.patch_outputs_read)
         remap = 0x1;

      MWV207_EMIT(cmd_buf, 0x5285,
         (ALIGN_BIT((uint32_t) (tcs_vertices_out), 0x0) & (0x3F)) |
         (ALIGN_BIT((uint32_t) (remap), 0x8) & (0x300)) |
         (ALIGN_BIT((uint32_t) (tcs->num_temps), 0xC) & (0x7F000)) |
         (ALIGN_BIT((uint32_t) (link.num_varyings_fixed), 0x14) & (0x3F00000)));

      uint32_t output[8] = {0};

      for (int i = 0; i < link.num_varyings_fixed; i++)
         mwv207_config_arrays(output, 8, i + 1, link.varyings[i].reg);

      MWV207_EMITS(cmd_buf, 0x5288, 8, output);

      MWV207_EMIT(cmd_buf, 0x5287,
         (ALIGN_BIT((uint32_t) (control_point_size), 0x0) & (0x3F)) |
         (ALIGN_BIT((uint32_t) (patch_offset), 0x8) & (0x7FF00)) |
         (ALIGN_BIT((uint32_t) (patch_size), 0x14) & (0x7FF00000)) |
         (ALIGN_BIT((uint32_t) (0x1), 0x1F) & (0x80000000)));

      MWV207_EMIT(cmd_buf, 0x5290, max_patch * patch_size);

      MWV207_EMIT(cmd_buf, 0x5286,
         (ALIGN_BIT((uint32_t) (max_patch), 0xC) & (0xFF000)) |
         (ALIGN_BIT((uint32_t) (ctx->info.alloc_page[PIPE_SHADER_TESS_CTRL]), 0x0) & (0x3F)) |
         (ALIGN_BIT((uint32_t) (ctx->info.alloc_page[PIPE_SHADER_TESS_CTRL]), 0x14) & (0x3F00000)));

      MWV207_EMIT(cmd_buf, 0x5293, 0x20);

      MWV207_EMIT(cmd_buf, 0x21A, (ALIGN_BIT(0x1, 0x0) & (0x1)));
      MWV207_EMIT(cmd_buf, 0x5280, 0);
      MWV207_EMIT(cmd_buf, 0x5281, (tcs->code_size / 4));
      mwv207_add_relocs(ctx, &cmd_buf, 0x5282,
         state->bo, state->current->bo_offset, 0x1);
      MWV207_EMIT(cmd_buf, 0x5284, (tcs->code_size / 4 - 1));
   } else {
      MWV207_EMIT(cmd_buf, 0x5285, 0);
      MWV207_EMIT(cmd_buf, 0x5286, 0);
   }

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

void
mwv207_config_tes(struct mwv207_context *ctx)
{
   uint32_t  *cmd_buf;
   struct mwv207_shader_state *state = ctx->shader_states[PIPE_SHADER_TESS_EVAL];

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   if (state) {
      struct mwv207_shader *tes = state->current;
      struct mwv207_shader_link_info link = {0};
      uint32_t mode = 0;
      uint32_t spacing = 0;
      uint32_t topolog = 0;
      uint32_t out_nums = 0;
      uint32_t out_offset = 1;

      mwv207_link_shader(ctx, tes, &link);

      out_nums = link.num_varyings_fixed;

      switch (state->nir->info.tess._primitive_mode) {
      case TESS_PRIMITIVE_TRIANGLES:
         mode = 0x0;
         break;

      case TESS_PRIMITIVE_QUADS:
         mode = 0x1;
         break;

      case TESS_PRIMITIVE_ISOLINES:
         mode = 0x2;
         break;

      default:
         unreachable("unknow tess primitive_mode");
         break;
      }

      switch (state->nir->info.tess.spacing) {
      case TESS_SPACING_UNSPECIFIED:
         spacing = 0x1;
         break;

      case TESS_SPACING_EQUAL:
         spacing = 0x0;
         break;

      case TESS_SPACING_FRACTIONAL_ODD:
         spacing = 0x2;
         break;

      case TESS_SPACING_FRACTIONAL_EVEN:
         spacing = 0x3;
         break;

      default:
         unreachable("unknow tess spacing");
         break;
      }

      if (state->nir->info.tess.point_mode)
         topolog = 0x0;
      else if (mode == 0x2)
         topolog = 0x1;
      else if (state->nir->info.tess.ccw)
         topolog = 0x3;
      else
         topolog = 0x2;

      MWV207_EMIT(cmd_buf, 0x52C0,
         (ALIGN_BIT((uint32_t) (mode), 0x0) & (0x3)) |
         (ALIGN_BIT((uint32_t) (spacing), 0x4) & (0x30)) |
         (ALIGN_BIT((uint32_t) (topolog), 0x8) & (0x300)) |
         (ALIGN_BIT((uint32_t) (64), 0xC) & (0x7F000)) |
         (ALIGN_BIT((uint32_t) (32), 0x14) & (0x7F00000)) |
         (ALIGN_BIT((uint32_t) (false), 0x1C) & (0x10000000)));

      uint32_t output[8] = {0};
      if (tes->vs_pos_out_reg < 0 && ctx->shader_states[PIPE_SHADER_VERTEX]->current->vs_pos_out_reg < 0)
         out_offset = 0;
      else
         mwv207_config_arrays(output, 8, 0, tes->vs_pos_out_reg < 0 ? 0 : tes->vs_pos_out_reg);

      out_nums += out_offset;

      for (int i = 0; i < link.num_varyings_fixed; i++)
         mwv207_config_arrays(output, 8, i + out_offset, link.varyings[i].reg);

      MWV207_EMIT(cmd_buf, 0x52C6,
         (ALIGN_BIT(0x1, 0x0) & (0x1)) |
         (ALIGN_BIT((uint32_t) (out_nums), 0x4) & (0x3F0)) |
         (ALIGN_BIT((uint32_t) (tes->num_temps), 0xC) & (0x7F000)));

      MWV207_EMITS(cmd_buf, 0x52D0, 8, output);

      MWV207_EMIT(cmd_buf, 0x52C8,
         (ALIGN_BIT((uint32_t) (out_nums), 0x0) & (0x3F)) |
         (ALIGN_BIT((uint32_t) (out_nums * 64), 0x8) & (0x7FF00)));

      MWV207_EMIT(cmd_buf, 0x52C7,
         (ALIGN_BIT((uint32_t) (ctx->info.alloc_page[PIPE_SHADER_TESS_EVAL]), 0x0) & (0x3F)) |
         (ALIGN_BIT((uint32_t) (ctx->info.max_thread[PIPE_SHADER_TESS_EVAL]), 0xC) & (0xFF000)) |
         (ALIGN_BIT((uint32_t) (ctx->info.max_thread[PIPE_SHADER_TESS_EVAL] / 4), 0x14) & (0x1FF00000)));

      MWV207_EMIT(cmd_buf, 0x52CD,
         (ALIGN_BIT((uint32_t) (ctx->info.patchs[0x1]), 0x0) & (0xFF)) |
         (ALIGN_BIT((uint32_t) (ctx->info.alloc_page[PIPE_SHADER_TESS_EVAL]), 0x8) & (0x3F00)));

      MWV207_EMIT(cmd_buf, 0x52CB, 0x28);

      MWV207_EMIT(cmd_buf, 0x21A, (ALIGN_BIT(0x1, 0x0) & (0x1)));
      MWV207_EMIT(cmd_buf, 0x52C1, 0);
      MWV207_EMIT(cmd_buf, 0x52C2, (tes->code_size / 4));
      mwv207_add_relocs(ctx, &cmd_buf, 0x52C3,
         state->bo, state->current->bo_offset, 0x1);
      MWV207_EMIT(cmd_buf, 0x52C5, (tes->code_size / 4 - 1));
   } else {
      MWV207_EMIT(cmd_buf, 0x52C6, 0);
      MWV207_EMIT(cmd_buf, 0x52C7, 0);
      MWV207_EMIT(cmd_buf, 0x52CD, 0);
   }

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

void
mwv207_config_gs(struct mwv207_context *ctx)
{
   uint32_t  *cmd_buf;
   struct mwv207_shader_state *state = ctx->shader_states[PIPE_SHADER_GEOMETRY];

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   if (state) {
      struct mwv207_shader *gs = state->current;
      struct mwv207_shader_link_info link = {0};
      uint32_t vertex_count = state->nir->info.gs.vertices_out;
      uint32_t outputs_num  = state->nir->num_outputs;
      if (gs->vs_pos_out_reg == -1)
         outputs_num += 1;
      uint32_t max_thread   = ctx->info.max_thread[PIPE_SHADER_GEOMETRY];
      uint32_t thread_size  = outputs_num * vertex_count;
      uint32_t group_size   = align((thread_size * max_thread + max_thread / 2 + 8), 16);
      uint32_t input_start  = 0x0;

      mwv207_link_shader(ctx, gs, &link);

      if (gs->gs_use_primitive_id || gs->gs_use_invocation_id || gs->gs_use_restart)
         input_start = 0x1;

      MWV207_EMIT(cmd_buf, 0x440,
         (ALIGN_BIT(0x1, 0x0) & (0x1)) |
         (ALIGN_BIT((uint32_t) (0), 0x4) & (0x3F0)) |
         (ALIGN_BIT((uint32_t) (gs->num_temps), 0xC) & (0x7F000)) |
         (ALIGN_BIT((uint32_t) (input_start), 0x14) & (0x300000)) |
         (ALIGN_BIT((uint32_t) (gs->gs_use_restart), 0x18) & (0x1000000)) |
         (ALIGN_BIT((uint32_t) (state->nir->info.gs.active_stream_mask != 1), 0x19) & (0x2000000)) |
         (ALIGN_BIT((uint32_t) (gs->gs_use_primitive_id), 0x1A) & (0x4000000)) |
         (ALIGN_BIT((uint32_t) (gs->gs_use_invocation_id), 0x1B) & (0x8000000)) |
         (ALIGN_BIT((uint32_t) (0x0), 0x1C) & (0x30000000)));

      MWV207_EMIT(cmd_buf, 0x441,
         mwv207_gs_type[state->nir->info.gs.output_primitive]);

      MWV207_EMIT(cmd_buf, 0x442,
         (ALIGN_BIT((uint32_t) (state->nir->info.gs.invocations), 0x0) & (0x7F)) |
         (ALIGN_BIT((uint32_t) (state->nir->info.gs.vertices_out), 0x10) & (0x1FF0000)));

      MWV207_EMIT(cmd_buf, 0x448, 0);

      MWV207_EMIT(cmd_buf, 0x451,
         (ALIGN_BIT((uint32_t) (outputs_num << 2), 0x0) & (0xFF)) |
         (ALIGN_BIT((uint32_t) (thread_size), 0x8) & (0x1FF00)) |
         (ALIGN_BIT((uint32_t) (thread_size * max_thread), 0x14) & (0xFFF00000)));

      MWV207_EMIT(cmd_buf, 0x452, group_size);

      MWV207_EMIT(cmd_buf, 0x450,
         (ALIGN_BIT((uint32_t) (max_thread), 0x8) & (0xFF00)) |
         (ALIGN_BIT((uint32_t) (ctx->info.alloc_page[PIPE_SHADER_GEOMETRY]), 0x0) & (0x3F)) |
         (ALIGN_BIT((uint32_t) (ctx->info.alloc_page[PIPE_SHADER_GEOMETRY]), 0x15) & (0x7E00000)));

      MWV207_EMIT(cmd_buf, 0x457, max_thread);

      MWV207_EMIT(cmd_buf, 0x455, 0x30);

      MWV207_EMIT(cmd_buf, 0x21A, (ALIGN_BIT(0x1, 0x0) & (0x1)));
      MWV207_EMIT(cmd_buf, 0x443, 0);
      MWV207_EMIT(cmd_buf, 0x444, (gs->code_size / 4));
      mwv207_add_relocs(ctx, &cmd_buf, 0x445,
         state->bo, state->current->bo_offset, 0x1);
      MWV207_EMIT(cmd_buf, 0x447, (gs->code_size / 4 - 1));
   } else {
      MWV207_EMIT(cmd_buf, 0x440, 0);
      MWV207_EMIT(cmd_buf, 0x450, 0);
      MWV207_EMIT(cmd_buf, 0x457, 0x000000ff);
   }

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

void
mwv207_config_fs(struct mwv207_context *ctx)
{
   uint32_t  *cmd_buf;
   struct mwv207_shader_state *state = ctx->shader_states[PIPE_SHADER_FRAGMENT];
   struct mwv207_shader *fs = state->current;
   uint32_t output_reg[2] = {0};
   uint32_t msaa_extra_temp = 0;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   for (int i = 0; i < ctx->fb_state.nr_cbufs; i++) {
      if (ctx->fb_state.cbufs[i]) {
         uint32_t render_num = ctx->info.cbufs_map[i];
         mwv207_config_arrays(output_reg, 8, render_num, fs->ps_color_out_reg[i]);

         if (ctx->info.pixel_record[i]) {
            uint32_t ba_index = ctx->info.pixel_record[i];
            mwv207_config_arrays(output_reg, 8, ba_index, fs->ps_color_out_reg[ba_index]);
         }
      }
   }

   if (ctx->info.subsamplez_dirty)
      msaa_extra_temp = 1;

   MWV207_EMIT(cmd_buf, 0x401, output_reg[0]);
   MWV207_EMIT(cmd_buf, 0x40B,     output_reg[1]);

   MWV207_EMIT(cmd_buf, 0x28D,
      ((ALIGN_BIT((uint32_t) (fs->usePointCoord), 0x4) & (0x10)) | (~(0x10 | 0x20))));

   MWV207_EMIT(cmd_buf, 0x5580, 0x00000002);

   uint32_t fs_input = fs->num_temps ? fs->num_temps : 1;
   MWV207_EMIT(cmd_buf, 0x403, fs_input + msaa_extra_temp);

   MWV207_EMIT(cmd_buf, 0x416, 0x0);
   MWV207_EMIT(cmd_buf, 0x409, (0x400 / 4));

   MWV207_EMIT(cmd_buf, 0x21A, (ALIGN_BIT(0x1, 0x0) & (0x1)));
   MWV207_EMIT(cmd_buf, 0x21F, 0);
   MWV207_EMIT(cmd_buf, 0x424, (fs->code_size / 4));
   mwv207_add_relocs(ctx, &cmd_buf, 0x40A,
      state->bo, state->current->bo_offset, 0x1);
   MWV207_EMIT(cmd_buf, 0x425, (fs->code_size / 4 - 1));

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

enum pipe_error
mwv207_config_link(struct mwv207_context *ctx)
{
   struct mwv207_shader *fs = ctx->shader_states[PIPE_SHADER_FRAGMENT]->current;
   struct mwv207_shader_link_info link = fs->link;
   uint32_t msaa_extra_input = 0;

   uint32_t *cmd_buf;
   mwv207_begin_record(ctx->cmd, &cmd_buf);

   if (ctx->shader_states[PIPE_SHADER_VERTEX]->current->vs_pointsize_out_reg != -1 &&
      ctx->rast_state->base.point_size_per_vertex) {
      MWV207_EMIT(cmd_buf, 0x2AA, link.num_varyings_fixed + 2);
   } else {
      MWV207_EMIT(cmd_buf, 0x2AA, link.num_varyings_fixed + 1);
   }

   MWV207_EMIT(cmd_buf, 0x28C, (ALIGN_BIT((uint32_t) (link.num_varyings), 0x8) & (0xF00)));

   if (ctx->info.subsamplez_dirty)
      msaa_extra_input = 1;

   MWV207_EMIT(cmd_buf, 0x402,
      (ALIGN_BIT((uint32_t) (link.num_varyings + 1 + msaa_extra_input), 0x0) & (0x3F)) |
      (ALIGN_BIT((uint32_t) (31), 0x8) & (0x1F00)) |
      (ALIGN_BIT((uint32_t) (false), 0x18) & (0x1000000)));

   uint32_t varyings[4] = {0};
   uint32_t semantics[16] = {0};
   uint32_t component_count = 0;
   bool half_attribute = false;

   for (int i = 0; i < link.num_varyings; ++i) {
      mwv207_config_arrays(varyings, 4, i, link.varyings[i].num_components);

      for (int j = 0; j < link.varyings[i].num_components; ++j)
         mwv207_config_arrays(semantics, 4, component_count++, link.varyings[i].interpolation);

      if (link.varyings[i].num_components <= 2)
         half_attribute = true;
   }

   component_count = align(component_count, 2);

   MWV207_EMIT(cmd_buf, 0xE07, component_count);

   MWV207_EMITS(cmd_buf, 0x2A4,     4, varyings);
   MWV207_EMITS(cmd_buf, 0x420,   4, varyings);
   MWV207_EMITS(cmd_buf, 0xE30, 16, semantics);

   MWV207_EMIT(cmd_buf, 0x380,
      (ALIGN_BIT((uint32_t) (true), 0x0) & (0x1)) |
      (ALIGN_BIT((uint32_t) (half_attribute), 0x1) & (0x2)));

   MWV207_EMIT(cmd_buf, 0x410, 0x00000000);

   MWV207_EMIT(cmd_buf, 0x223, 0);

   if (ctx->shader_states[PIPE_SHADER_TESS_CTRL])
      MWV207_EMIT(cmd_buf, 0x5283, 0);

   if (ctx->shader_states[PIPE_SHADER_TESS_EVAL])
      MWV207_EMIT(cmd_buf, 0x52C4, 0);

   if (ctx->shader_states[PIPE_SHADER_GEOMETRY])
      MWV207_EMIT(cmd_buf, 0x446, 0);

   MWV207_EMIT(cmd_buf, 0x412, 0);

   mwv207_end_record(ctx->cmd, &cmd_buf);

   return PIPE_OK;
}

static const struct nir_shader_compiler_options mwv207_nir_options_VSGSFS = {
   .lower_all_io_to_temps = true,
   .lower_flrp32 = true,
   .lower_flrp16 = true,
   .lower_flrp64 = true,
   .lower_fpow = true,
   .lower_fsat = true,
   .lower_hadd = true,
   .lower_pack_half_2x16 = true,
   .lower_pack_32_2x16_split = true,
   .lower_pack_snorm_4x8 = true,
   .lower_pack_snorm_2x16 = true,
   .lower_pack_unorm_4x8 = true,
   .lower_pack_unorm_2x16 = true,
   .lower_unpack_half_2x16 = true,
   .lower_unpack_32_2x16_split = true,
   .lower_unpack_snorm_4x8 = true,
   .lower_unpack_snorm_2x16 = true,
   .lower_unpack_unorm_4x8 = true,
   .lower_unpack_unorm_2x16 = true,
   .lower_pack_32_2x16_split = true,
   .lower_unpack_32_2x16_split = true,
   .lower_ftrunc = true,
   .lower_fdph = true,
   .lower_fmod = true,
   .lower_fdiv = true,
   .fuse_ffma16 = true,
   .fuse_ffma32 = true,
   .fuse_ffma64 = true,
   .lower_uadd_carry = true,
   .lower_usub_borrow = true,
   .lower_ldexp = true,
   .lower_bitops = false,
   .lower_insert_byte = true,
   .lower_insert_word = true,
   .lower_extract_byte = true,
   .lower_extract_word = true,
   .lower_vector_cmp = true,
   .lower_uniforms_to_ubo = true,
   .force_indirect_unrolling = nir_var_all,
   .vectorize_io = true,
   .has_uclz = true,
   .lower_bit_count = true,
   .compact_arrays = true,
   .lower_cs_local_index_to_id = true,
   .has_cs_global_id = true,
   .lower_int64_options = (nir_lower_int64_options)~0,
   .lower_doubles_options = (nir_lower_doubles_options)~0,
};

static const struct nir_shader_compiler_options mwv207_nir_options_TCSTES = {
   .lower_flrp32 = true,
   .lower_flrp16 = true,
   .lower_flrp64 = true,
   .lower_fpow = true,
   .lower_fsat = true,
   .lower_pack_half_2x16 = true,
   .lower_pack_32_2x16_split = true,
   .lower_pack_snorm_4x8 = true,
   .lower_pack_snorm_2x16 = true,
   .lower_pack_unorm_4x8 = true,
   .lower_pack_unorm_2x16 = true,
   .lower_unpack_half_2x16 = true,
   .lower_unpack_32_2x16_split = true,
   .lower_unpack_snorm_4x8 = true,
   .lower_unpack_snorm_2x16 = true,
   .lower_unpack_unorm_4x8 = true,
   .lower_unpack_unorm_2x16 = true,
   .lower_ftrunc = true,
   .lower_fdph = true,
   .lower_fmod = true,
   .lower_fdiv = true,
   .fuse_ffma16 = true,
   .fuse_ffma32 = true,
   .fuse_ffma64 = true,
   .lower_uadd_carry = true,
   .lower_usub_borrow = true,
   .lower_ldexp = true,
   .lower_mul_high = true,
   .lower_bitops = false,
   .lower_insert_byte = true,
   .lower_insert_word = true,
   .lower_extract_byte = true,
   .lower_extract_word = true,
   .lower_vector_cmp = true,
   .lower_uniforms_to_ubo = true,
   .force_indirect_unrolling = nir_var_all,
   .vectorize_io = true,
   .lower_bit_count = true,
   .lower_cs_local_index_to_id = true,
   .lower_int64_options = (nir_lower_int64_options)~0,
   .lower_doubles_options = (nir_lower_doubles_options)~0,
   .has_uclz = true,
   .compact_arrays = true,
};

const void *
mwv207_get_compiler_options(struct pipe_screen *screen,
                            enum pipe_shader_ir ir,
                            enum pipe_shader_type shader)
{
   assert(ir == PIPE_SHADER_IR_NIR);

   if (shader == PIPE_SHADER_TESS_CTRL || shader == PIPE_SHADER_TESS_EVAL)
      return &mwv207_nir_options_TCSTES;
   else
      return &mwv207_nir_options_VSGSFS;
}

static struct nir_shader *
mwv207_tgsi_to_nir(struct pipe_screen *pscreen, const struct tgsi_token *tokens)
{
   struct mwv207_screen *screen = mwv207_screen(pscreen);

   if (screen->ws->debug & 0x2) {
      fprintf(stderr, "TGSI shader:\n---8<---\n");
      tgsi_dump_to_file(tokens, 0, stderr);
      fprintf(stderr, "---8<---\n\n");
   }

   return tgsi_to_nir(tokens, pscreen, false);
}

static struct mwv207_shader *
mwv207_create_shader(struct mwv207_shader_state *state, struct mwv207_shader_key key)
{
   struct mwv207_shader *shader = CALLOC_STRUCT(mwv207_shader);
   assert(shader);

   shader->info = state;
   shader->stage =  pipe_shader_type_from_mesa(state->nir->info.stage);
   shader->code = NULL;
   shader->next = NULL;
   shader->key = key;
   shader->bo_offset = 0;

   shader->vs_id_in_reg = -1;
   shader->vs_pos_out_reg = -1;
   shader->vs_pointsize_out_reg = -1;

   memset(&shader->ps_color_out_reg, 0, sizeof(shader->ps_color_out_reg));
   memset(&shader->ubo_mask, 0, sizeof(shader->ubo_mask));
   shader->ps_depth_out_reg = -1;

   shader->inner_outer_count = 0;

   shader->useFragCoord = false;
   shader->usePointCoord = false;

   shader->gs_use_primitive_id = false;
   shader->gs_use_layer = false;
   shader->gs_use_restart = false;
   shader->gs_primitive_id_location = 0;

   shader->cs_shared_size = 0;
   shader->spill_offset = 0;

   return shader;
}

void static
mwv207_destroy_shader(struct mwv207_shader *shader)
{
   assert(shader);

   if (shader->spill_buffer.buffer)
      util_copy_shader_buffer(&shader->spill_buffer, NULL);

   FREE(shader->code);
   FREE(shader->uniforms.imm_data);
   FREE(shader->uniforms.imm_contents);
   FREE(shader);
}

bool
mwv207_set_current_shader(struct mwv207_context *ctx,
                          struct mwv207_shader_state *state)
{
   bool result = false;
   struct mwv207_shader *shader;
   struct mwv207_shader_key key = {0};
   key.frag_rgba_swap = ctx->info.frag_rgba_swap;
   key.front_ccw = 0;
   key.tex_128bit = ctx->info.tex_128bit;
   key.pixel_128bit = ctx->info.pixel_128bit;
   key.is_tg4_lower = ctx->info.tg4_lower;
   key.z32_shadow = ctx->info.z32_shadow;
   key.render_arrary = 0;
   key.is_dual = ctx->info.dsbi.is_dual;
   int guess_tcs_input_vertices = state->nir->info.tess.tcs_vertices_out;
   int guess_tcs_input_vertices_regs = (guess_tcs_input_vertices - 1) / 8;

   assert(state->nir->info.stage != MESA_SHADER_TESS_EVAL ||
         (guess_tcs_input_vertices_regs >= 0 && guess_tcs_input_vertices_regs <= 3));
   key.tcs_input_vertries_regs = guess_tcs_input_vertices_regs;

   if (state->current && state->current->stage == PIPE_SHADER_FRAGMENT) {
      if (ctx->rast_state) {
         key.front_ccw = ctx->rast_state->base.front_ccw;
         key.sprite_coord_enable = ctx->rast_state->sprite_coord_enable;
         key.sprite_coord_yinvert = ctx->rast_state->sprite_coord_mode;
      }

      if (ctx->shader_states[PIPE_SHADER_GEOMETRY]) {
         struct mwv207_shader *gs = ctx->shader_states[PIPE_SHADER_GEOMETRY]->current;

         if (gs->gs_use_layer)
            key.render_arrary = ctx->info.has_render_arrary;
      }
   }

   if (state->current && state->current->stage == PIPE_SHADER_GEOMETRY) {
      if (ctx->shader_states[PIPE_SHADER_TESS_EVAL])
         key.has_tess = true;
   }

   if (state->current && state->current->stage == PIPE_SHADER_TESS_CTRL) {
      int32_t real_input_vertices = ctx->info.vertices_per_patch;
      int32_t real_input_vertices_regs = (real_input_vertices - 1) / 8;

      assert(real_input_vertices_regs >= 0 && real_input_vertices_regs <= 3);
      key.tcs_input_vertries_regs = real_input_vertices_regs;
   }

   for (shader = state->shader_list; shader; shader = shader->next)
      if (mwv207_shader_key_equal(&key, &shader->key))
         break;

   if (shader) {
      if (state->current != shader) {
         state->current = shader;
         result = true;
      }

      if ((key.tex_128bit && (ctx->dirty & MWV207_DIRTY_SAMPLER_VIEWS)) ||
          (key.pixel_128bit && (ctx->dirty & MWV207_DIRTY_FRAMEBUFFER)) ||
          (key.z32_shadow && (ctx->dirty & MWV207_DIRTY_SAMPLERS)) ||
          (key.is_dual && (ctx->dirty & MWV207_DIRTY_BLEND))) {
         mwv207_compile_shader(ctx, state, false);
         result = true;
      }
   } else {

      state->current = mwv207_create_shader(state, key);
      state->current->next = state->shader_list;
      state->shader_list = state->current;
      mwv207_compile_shader(ctx, state, false);
      result = true;
   }

   if (result && unlikely(ctx->ws->debug & 0x1)) {
      nir_print_shader(state->nir, stdout);
      mwv207_dump_shader_nir(state->current);

   }

   return result;
}

static void *
mwv207_create_shader_state(struct pipe_context *pctx, const struct pipe_shader_state *pshader)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_shader_state *state = CALLOC_STRUCT(mwv207_shader_state);
   struct nir_shader *nir;

   if (pshader->type != PIPE_SHADER_IR_NIR)
      nir = mwv207_tgsi_to_nir(pctx->screen, pshader->tokens);
   else
      nir = (struct nir_shader *)pshader->ir.nir;

   state->nir = nir;
   state->bo = NULL;
   state->shader_list = NULL;
   state->base_stream_output = pshader->stream_output;

   mwv207_set_current_shader(ctx, state);

   return (void *)state;
}

static void
mwv207_bind_vs_state(struct pipe_context *pctx, void *state)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   ctx->shader_states[PIPE_SHADER_VERTEX] = state;

   ctx->dirty |= MWV207_DIRTY_SHADER;
}

static void
mwv207_bind_fs_state(struct pipe_context *pctx, void *state)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   ctx->shader_states[PIPE_SHADER_FRAGMENT] = state;

   ctx->dirty |= MWV207_DIRTY_SHADER;
}

static void
mwv207_bind_gs_state(struct pipe_context *pctx, void *state)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   ctx->shader_states[PIPE_SHADER_GEOMETRY] = state;

   ctx->dirty |= MWV207_DIRTY_SHADER;
}

static void
mwv207_bind_tcs_state(struct pipe_context *pctx, void *state)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   ctx->shader_states[PIPE_SHADER_TESS_CTRL] = state;

   ctx->dirty |= MWV207_DIRTY_SHADER;
}

static void
mwv207_bind_tes_state(struct pipe_context *pctx, void *state)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   ctx->shader_states[PIPE_SHADER_TESS_EVAL] = state;

   ctx->dirty |= MWV207_DIRTY_SHADER;
}

static void
mwv207_delete_shader_state(struct pipe_context *pctx, void *pshader)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_shader_state *state = mwv207_shader_state(pshader);
   struct mwv207_shader *shader = state->shader_list;
   struct mwv207_shader *next_shader;

   if (state->nir)
      ralloc_free(state->nir);

   while (shader) {
      next_shader = shader->next;
      mwv207_destroy_shader(shader);
      shader = next_shader;
   }

   ctx->ws->bo_reference(ctx->ws, &state->bo, NULL);

   FREE(state);
}

static void *
mwv207_create_cs_state(struct pipe_context *pctx, const struct pipe_compute_state *pshader)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_shader_state *state = CALLOC_STRUCT(mwv207_shader_state);
   struct nir_shader *nir;

   if (pshader->ir_type == PIPE_SHADER_IR_TGSI)
      nir = mwv207_tgsi_to_nir(pctx->screen, (const struct tgsi_token *)pshader->prog);
   else if (pshader->ir_type == PIPE_SHADER_IR_NIR_SERIALIZED) {
      struct blob_reader reader;
      const struct pipe_binary_program_header *hdr = pshader->prog;

      blob_reader_init(&reader, hdr->blob, hdr->num_bytes);
      nir = nir_deserialize(NULL, &mwv207_nir_options_VSGSFS, &reader);
   }
   else
      nir = (struct nir_shader *)pshader->prog;

   state->nir = nir;
   state->bo = NULL;
   state->shader_list = NULL;

   mwv207_set_current_shader(ctx, state);

   return (void *)state;
}

static void
mwv207_get_compute_state_info(struct pipe_context *ctx,
                              void *state,
                              struct pipe_compute_state_object_info *info)
{
   info->max_threads = 1024;
   info->preferred_simd_size = 32;
   info->simd_sizes = 8 | 16 | 32;
   info->private_memory = 0;
}

static uint32_t
mwv207_get_compute_state_subgroup_size(struct pipe_context *ctx,
                                       void *state,
                                       const uint32_t block[3])
{
   return 0;
}

static void
mwv207_bind_cs_state(struct pipe_context *pctx, void *state)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   ctx->shader_states[PIPE_SHADER_COMPUTE] = state;

   ctx->dirty |= MWV207_DIRTY_SHADER;
}

void
mwv207_context_shader_init(struct pipe_context *pctx)
{
   pctx->create_vs_state = mwv207_create_shader_state;
   pctx->bind_vs_state =   mwv207_bind_vs_state;
   pctx->delete_vs_state = mwv207_delete_shader_state;

   pctx->create_fs_state = mwv207_create_shader_state;
   pctx->bind_fs_state =   mwv207_bind_fs_state;
   pctx->delete_fs_state = mwv207_delete_shader_state;

   pctx->create_gs_state = mwv207_create_shader_state;
   pctx->bind_gs_state =   mwv207_bind_gs_state;
   pctx->delete_gs_state = mwv207_delete_shader_state;

   pctx->create_tcs_state = mwv207_create_shader_state;
   pctx->bind_tcs_state =   mwv207_bind_tcs_state;
   pctx->delete_tcs_state = mwv207_delete_shader_state;

   pctx->create_tes_state = mwv207_create_shader_state;
   pctx->bind_tes_state =   mwv207_bind_tes_state;
   pctx->delete_tes_state = mwv207_delete_shader_state;

   pctx->create_compute_state = mwv207_create_cs_state;
   pctx->bind_compute_state =   mwv207_bind_cs_state;
   pctx->delete_compute_state = mwv207_delete_shader_state;

   pctx->get_compute_state_info = mwv207_get_compute_state_info;
   pctx->get_compute_state_subgroup_size = mwv207_get_compute_state_subgroup_size;
}
