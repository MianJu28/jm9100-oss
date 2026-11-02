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
#ifndef MWV207_FORMAT_H
#define MWV207_FORMAT_H

#include "util/format/u_format.h"

#include "mwv207_cmd.h"

#define BLT_SWIZZLE_EX(r, g, b, a) \
   (ALIGN_BIT((uint32_t) (r), 0x0) & (0x7)) |\
   (ALIGN_BIT((uint32_t) (g), 0x3) & (0x38)) |\
   (ALIGN_BIT((uint32_t) (b), 0x6) & (0x1C0)) |\
   (ALIGN_BIT((uint32_t) (a), 0x9) & (0xE00))

#define SWIZZLE_SETS(r, g, b, a) {r, g, b ,a }

#define SWIZZLE_STATE(incoming_swizzle) \
   { \
      int state = 1; \
      if (incoming_swizzle[0] != PIPE_SWIZZLE_X || \
          incoming_swizzle[1] != PIPE_SWIZZLE_Y || \
          incoming_swizzle[2] != PIPE_SWIZZLE_Z || \
          incoming_swizzle[3] != PIPE_SWIZZLE_W) { \
         state = 0; \
      } \
      (state); \
   }

#define TX_FORMAT(oldFormat, newFormat) \
   (oldFormat << 0x0) | \
   (newFormat << 0x8)

enum {
   SWIZZLE_RED   = 0x0,
   SWIZZLE_GREEN = 0x1,
   SWIZZLE_BLUE  = 0x2,
   SWIZZLE_ALPHA = 0x3,
   SWIZZLE_ZERO  = 0x4,
   SWIZZLE_ONE   = 0x5,
};

struct mwv207_blit_Info {
   enum pipe_format mesa_format;
   uint32_t format;
   uint32_t swizzle;
};

struct mwv207_hardware_Info {
   enum pipe_format mesa_format;
   uint32_t format;
   uint32_t swizzles[4];
};

struct mwv207_blit_desc {
   bool     srgb;
   bool     fake_format;
   uint32_t format;
   uint32_t pixel_size;
   uint32_t swizzle;
   uint32_t sample_mode;
   uint32_t endian;
};

struct mwv207_pixel_desc {
   bool     srgb;
   bool     saturation;
   uint32_t format;
   uint32_t output_mode;
   uint32_t swizzles[4];
   uint32_t endian;
};

struct mwv207_texture_config {
   uint32_t address[16];
   uint32_t tex_config;
   uint32_t tex_size;
   uint32_t tex_stride;
   uint32_t tex_config_ext;
   uint32_t tex_yuv_control;
   uint32_t tex_yuv_stride;
   uint32_t tex_astc0;
   uint32_t tex_astc1;
   uint32_t tex_astc2;
   uint32_t tex_astc3;
   uint32_t tex_base_lod;
   uint32_t tex_config2;
   uint32_t tex_config3;
   uint32_t tex_size_ext;
   uint32_t tex_volume_ext;
   uint32_t tex_slice;
   uint32_t tex_border_color;
   uint32_t tex_3d;
   uint32_t tex_log_size;
   uint32_t tex_border_blue32;
   uint32_t tex_border_green32;
   uint32_t tex_border_red32;
   uint32_t tex_border_alpha32;
};

struct mwv207_texture_desc {
   bool srgb;
   bool fast_filter;
   bool stencil;
   bool msaa;
   bool cube;
   bool tex_128bit;

   uint32_t format;
   uint32_t int_format;
   uint32_t swizzles[4];

   uint32_t tex_fetch[4];

   struct cmd_dat dat;
   struct mwv207_texture_config tex_config;
};

struct mwv207_element;
struct mwv207_clear_info;

enum pipe_error
mwv207_get_pixel_desc(enum pipe_format format, struct mwv207_pixel_desc *pixel_desc);

enum pipe_error
mwv207_get_element_desc(struct mwv207_element *element);

enum pipe_error
mwv207_get_blit_desc(enum pipe_format format, struct mwv207_blit_desc *blt_desc);

enum pipe_error
mwv207_get_texture_desc(enum pipe_format format, struct mwv207_texture_desc *texture_desc);

uint32_t
mwv207_get_format_desc(enum pipe_format format);

enum pipe_error
mwv207_get_clearval(uint32_t format,
                    struct mwv207_clear_info info,
                    uint32_t *clear_val,
                    uint32_t *clear_mask);

uint32_t
mwv207_get_compressed_format(enum pipe_format format);

uint32_t
mwv207_update_colormask(struct mwv207_context *ctx, int num);

bool
mwv207_get_sample_info(int num_samples,
                       uint8_t *xscale_out,
                       uint8_t *yscale_out,
                       uint8_t *nr_samples_out);

uint32_t
mwv207_get_blit_msaa_mode(uint8_t x_scale, uint8_t y_scale);

#endif
