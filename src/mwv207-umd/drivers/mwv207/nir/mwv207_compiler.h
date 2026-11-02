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
#ifndef H_MWV207_COMPILER
#define H_MWV207_COMPILER

#include "mwv207_common.h"
#include "mwv207_shader.h"

struct mwv207_shader_key {
   union {
      struct {

         uint32_t frag_rgba_swap : 1;

         uint32_t front_ccw : 1;
         uint32_t tex_128bit : 1;
         uint32_t pixel_128bit : 1;
         uint32_t z32_shadow : 1;
         uint32_t render_arrary : 1;
         uint32_t has_tess : 1;

         uint32_t tcs_input_vertries_regs : 2;
         uint32_t is_tg4_lower : 1;
         uint32_t is_dual : 1;

         uint32_t sprite_coord_enable : MAX_TEXTURE_COORD_UNITS;
         uint32_t sprite_coord_yinvert : 1;
      };
      uint32_t global;
   };
};

static inline bool
mwv207_shader_key_equal(struct mwv207_shader_key *a, struct mwv207_shader_key *b)
{
   STATIC_ASSERT(sizeof(struct mwv207_shader_key) <= sizeof(a->global));

   return a->global == b->global;
}

struct mwv207_shader_inout {
   int reg;
   int slot;
   int driver_location;
   bool is_array;
   int arry_index;
   int num_components;
   int interpolation;
};

struct mwv207_shader_io_file {
   size_t num_reg;
   struct mwv207_shader_inout reg[0x10];
};

struct mwv207_shader_uniform_info {
   enum mwv207_immediate_contents *imm_contents;
   uint32_t *imm_data;
   uint32_t imm_count;
   bool no_unifrom;
};

struct mwv207_varying {
   uint32_t interpolation;
   uint8_t num_components;
   uint8_t reg;
};

struct mwv207_shader_link_info {

   uint32_t num_varyings;

   uint32_t num_varyings_fixed;
   struct mwv207_varying varyings[0x10];
   int pcoord_varying_comp_ofs;
};

struct mwv207_shader {
   enum pipe_shader_type stage;

   struct mwv207_shader_io_file infile;

   struct mwv207_shader_io_file outfile;

   struct mwv207_shader_link_info link;

   struct mwv207_shader_uniform_info uniforms;

   uint32_t *code;
   uint32_t code_size;
   uint32_t num_temps;

   int vs_id_in_reg;
   int vs_pos_out_reg;
   int vs_pointsize_out_reg;
   uint32_t vs_load_balancing;

   bool gs_use_primitive_id;
   int  gs_primitive_id_location;
   bool gs_use_invocation_id;
   bool gs_use_restart;
   bool gs_use_layer;

   int output_num;
   int patch_num;

   int inner_outer_count;

   int ps_color_out_reg[8];
   int ps_depth_out_reg;
   bool useFragCoord;
   bool usePointCoord;
   bool useClipDistance;

   unsigned cs_shared_size;

   uint32_t spill_offset;
   struct pipe_shader_buffer spill_buffer;

   struct mwv207_shader *next;

   struct mwv207_shader_state *info;
   struct mwv207_shader_key key;

   /* 编译这份二进制时，影响它的那几个状态的指纹（见 mwv207_context.h 的
    * fp_*）。相等就说明状态没变，可复用这份二进制。FIX(jm9100-oss) */
   uint32_t built_fp_tex_128bit;
   uint32_t built_fp_pixel_128bit;
   uint32_t built_fp_z32_shadow;
   uint32_t built_fp_is_dual;
   bool z32_shadow[PIPE_SHADER_TYPES][PIPE_MAX_SHADER_SAMPLER_VIEWS];

   uint32_t bo_offset;
   uint64_t ubo_mask[0x8];
};

struct mwv207_compile {
   nir_shader *nir;
   nir_function_impl *impl;

   struct mwv207_shader *shader;

   uint32_t block_ptr[0x10000];

   int inst_ptr;
   struct mwv207_inst code[0x20000];

   int const_count;
   uint64_t consts[0x800];

   struct ra_graph *g;
   struct ra_regs *regs;
   uint32_t *live_map;
   uint32_t num_nodes;
};

#define OPT(nir, pass, ...) ({                             \
      bool this_progress = false;                             \
      NIR_PASS(this_progress, nir, pass, ##__VA_ARGS__);      \
      this_progress;                                          \
   })

void
mwv207_lower(struct mwv207_context *ctx, struct mwv207_compile *c);

bool
mwv207_compile_shader(struct mwv207_context *ctx, struct mwv207_shader_state *shader, bool offline);

bool
mwv207_link_shader(struct mwv207_context *ctx,
                   struct mwv207_shader *shader,
                   struct mwv207_shader_link_info *info);

void
mwv207_dump_shader_nir(const struct mwv207_shader *shader);

#endif
