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
#include "util/u_pack_color.h"

#include "mwv207_cmd.h"
#include "mwv207_context.h"
#include "mwv207_blit.h"

static uint32_t
mwv207_get_endian(enum pipe_format format)
{
   return 0x0;

   switch (util_format_get_blocksize(format)) {
   case 1:
   case 2:
      return 0x0;
   case 3:
   case 4:
      return 0x1;
   case 8:
      return 0x2;
   case 16:
      return 0x3;
   default:
      return 0x0;
   }
}

struct mwv207_hardware_Info texture_infos[] = {
   {PIPE_FORMAT_A8_UNORM,           TX_FORMAT(0x1, 0),                       SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_L8_UNORM,           TX_FORMAT(0x2, 0),                       SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_I8_UNORM,           TX_FORMAT(0x3, 0),                       SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_L8A8_UNORM,         TX_FORMAT(0x4, 0),                     SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_B4G4R4A4_UNORM,     TX_FORMAT(0x5, 0),                    SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R4G4B4A4_UNORM,     TX_FORMAT(0x5, 0),                    SWIZZLE_SETS(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_B4G4R4X4_UNORM,     TX_FORMAT(0x6, 0),                    SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_B8G8R8A8_UNORM,     TX_FORMAT(0x7, 0),                    SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8G8B8A8_UNORM,     TX_FORMAT(0x7, 0),                    SWIZZLE_SETS(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_A8B8G8R8_UNORM,     TX_FORMAT(0x7, 0),                    SWIZZLE_SETS(SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA, SWIZZLE_RED)},

   {PIPE_FORMAT_B8G8R8X8_UNORM,     TX_FORMAT(0x8, 0),                    SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R8G8B8X8_UNORM,     TX_FORMAT(0xA, 0),                    SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R5G6B5_UNORM,       TX_FORMAT(0xB, 0),                   SWIZZLE_SETS(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_B5G6R5_UNORM,       TX_FORMAT(0xB, 0),                   SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R5G5B5A1_UNORM,     TX_FORMAT(0xC, 0),                   SWIZZLE_SETS(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_B5G5R5A1_UNORM,     TX_FORMAT(0xC, 0),                   SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R9G9B9E5_FLOAT,     TX_FORMAT(0x1D, 0),                  SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R8_UNORM,           TX_FORMAT(0, 0x21),                   SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8_SNORM,           TX_FORMAT(0, 0xE),            SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R8_UINT,            TX_FORMAT(0, 0x15),                  SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_ZERO, SWIZZLE_ZERO, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8_SINT,            TX_FORMAT(0, 0x15),                  SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_ZERO, SWIZZLE_ZERO, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R8G8_UNORM,         TX_FORMAT(0, 0x6),                 SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8G8_SNORM,         TX_FORMAT(0, 0xF),           SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8G8_UINT,          TX_FORMAT(0, 0x16),               SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_ZERO, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8G8_SINT,          TX_FORMAT(0, 0x16),               SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_ZERO, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_A8B8G8R8_SNORM,     TX_FORMAT(0, 0x11),         SWIZZLE_SETS(SWIZZLE_ALPHA, SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED)},
   {PIPE_FORMAT_R8G8B8A8_SNORM,     TX_FORMAT(0, 0x11),         SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R8G8B8A8_UINT,      TX_FORMAT(0, 0x17),         SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8G8B8A8_SINT,      TX_FORMAT(0, 0x17),         SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_A8B8G8R8_UINT,      TX_FORMAT(0, 0x17),         SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_A8B8G8R8_SINT,      TX_FORMAT(0, 0x17),         SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R9G9B9E5_FLOAT,     TX_FORMAT(0x1D, 0),                  SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R11G11B10_FLOAT,    TX_FORMAT(0, 0x1B),         SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R10G10B10A2_UNORM,  TX_FORMAT(0, 0xC),          SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_B10G10R10A2_UNORM,  TX_FORMAT(0, 0xC),          SWIZZLE_SETS(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R10G10B10A2_UINT,   TX_FORMAT(0, 0x1C),      SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_B10G10R10A2_UINT,   TX_FORMAT(0, 0x1C),      SWIZZLE_SETS(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_ETC1_RGB8,          TX_FORMAT(0x1E, 0),                     SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_ETC2_RGB8,          TX_FORMAT(0, 0x0),            SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_ETC2_RGB8A1,        TX_FORMAT(0, 0x1),          SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_ETC2_RGBA8,         TX_FORMAT(0, 0x2),          SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_ETC2_R11_UNORM,     TX_FORMAT(0, 0x3),    SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_ETC2_R11_SNORM,     TX_FORMAT(0, 0xD),      SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_ETC2_RG11_UNORM,    TX_FORMAT(0, 0x4),   SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_ETC2_RG11_SNORM,    TX_FORMAT(0, 0x5),     SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_DXT1_RGB,           TX_FORMAT(0x13, 0),                     SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_DXT1_RGBA,          TX_FORMAT(0x13, 0),                     SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_DXT3_RGBA,          TX_FORMAT(0x14, 0),                     SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_DXT5_RGBA,          TX_FORMAT(0x15, 0),                     SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R16_UINT,           TX_FORMAT(0, 0x18),                 SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_ZERO, SWIZZLE_ZERO, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16_SINT,           TX_FORMAT(0, 0x18),                 SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_ZERO, SWIZZLE_ZERO, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16_FLOAT,          TX_FORMAT(0, 0x7),                 SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R32_UINT,           TX_FORMAT(0, 0x23),             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_ZERO, SWIZZLE_ZERO, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R32_SINT,           TX_FORMAT(0, 0x23),             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_ZERO, SWIZZLE_ZERO, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R32_FLOAT,          TX_FORMAT(0, 0xA),                 SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R16G16_UINT,        TX_FORMAT(0, 0x19),             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_ZERO, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16G16_SINT,        TX_FORMAT(0, 0x19),             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_ZERO, SWIZZLE_ALPHA)},
   /* FIX(jm9100-oss): 16-bit normalized sampler formats.  Required by
    * GL_ARB_ES3_compatibility (ETC2-emulation formats, st_extensions.c) which
    * in turn gates ARB_ES3_1_compatibility / ARB_ES3_2_compatibility, and also
    * by 10-bit P010/P016 video planes.  Codes taken from the vendor register
    * enum MWV207REG_TX_EXT_CONFIG_FORMAT_*:
    *   RG16_UNORM 0x34, RG16_SNORM 0x35, R16_SNORM 0x36.
    * There is no R16_UNORM code in that enum, so R16_UNORM shares the 0x36
    * ("16-bit single channel") code with R16_SNORM and is distinguished by
    * texture_desc->int_format, which mwv207_get_texture_desc() derives from
    * the pipe format's signedness (unsigned -> 0, signed 16-bit -> 2). */
   /* FIX(jm9100-oss) 2026-11-03: the 16-bit normalized sampler codes
    * 0x32-0x36 (RGBA16_UNORM/SNORM, RG16_UNORM/SNORM, R16_SNORM) decode to
    * zero on the 0x1F76 GPU: forcing the working R16_FLOAT texture to code
    * 0x36 still samples 0, while the same texture with code 7 returns the
    * uploaded value.  The descriptor is otherwise identical (verified by
    * dumping TXConfig/TXExtConfig/TXSize/TXStride), and swapping mode
    * (SUPERTILED/LINEAR), alignment, TX_CONFIG2 NORM_*, TX_CONFIG3
    * ENABLE_NEW_MODE and TX_EXT_CONFIG bit23/bit31 makes no difference.
    * 16-bit integer formats (codes 24-26) and 16-bit float (7-9) do fetch
    * correctly, and upload/read-back through the transfer paths is byte
    * exact, so the failure is the sampler's normalized-16-bit decode, not
    * the data or the descriptor packing.  Forcing LINEAR is also required
    * for correct upload (see mwv207_tile_supported_format()). */
   {PIPE_FORMAT_R16_UNORM,          TX_FORMAT(0, 0x36),             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_ZERO, SWIZZLE_ZERO, SWIZZLE_ONE)},
   {PIPE_FORMAT_R16_SNORM,          TX_FORMAT(0, 0x36),             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_ZERO, SWIZZLE_ZERO, SWIZZLE_ONE)},
   {PIPE_FORMAT_R16G16_UNORM,       TX_FORMAT(0, 0x34),             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_ZERO, SWIZZLE_ONE)},
   {PIPE_FORMAT_R16G16_SNORM,       TX_FORMAT(0, 0x35),             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_ZERO, SWIZZLE_ONE)},
   {PIPE_FORMAT_R16G16B16A16_UNORM, TX_FORMAT(0, 0x32),             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16G16B16A16_SNORM, TX_FORMAT(0, 0x33),             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16G16_FLOAT,       TX_FORMAT(0, 0x8),             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R32G32_UINT,        TX_FORMAT(0, 0x24),         SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_ZERO, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R32G32_SINT,        TX_FORMAT(0, 0x24),         SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_ZERO, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R32G32_FLOAT,       TX_FORMAT(0, 0xB),             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R16G16B16A16_UINT,  TX_FORMAT(0, 0x1A),     SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16G16B16A16_SINT,  TX_FORMAT(0, 0x1A),     SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R32G32B32A32_UINT,  TX_FORMAT(0, 0x27), SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R32G32B32A32_SINT,  TX_FORMAT(0, 0x28), SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R16G16B16A16_FLOAT, TX_FORMAT(0, 0x9),    SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R32G32B32A32_FLOAT, TX_FORMAT(0, 0x26),     SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_S8_UINT,            TX_FORMAT(0, 0x15),                  SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_Z16_UNORM,          TX_FORMAT(0x10, 0),                      SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_Z32_FLOAT,          TX_FORMAT(0, 0xA),                 SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_X8Z24_UNORM,        TX_FORMAT(0, 0x22),                SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_ZERO, SWIZZLE_ZERO, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_S8_UINT_Z24_UNORM,  TX_FORMAT(0, 0x22),                SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_ZERO, SWIZZLE_ZERO, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_X32_S8X24_UINT,     TX_FORMAT(0, 0xB),             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_Z32_FLOAT_S8X24_UINT, TX_FORMAT(0, 0xB),           SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_X24S8_UINT,         TX_FORMAT(0, 0x22),                SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_ZERO, SWIZZLE_ZERO, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_S8X24_UINT,         TX_FORMAT(0, 0x22),                SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_ZERO, SWIZZLE_ZERO, SWIZZLE_ALPHA)},
};

enum pipe_error
mwv207_get_texture_desc(enum pipe_format format, struct mwv207_texture_desc *texture_desc)
{
   uint32_t i;
   enum pipe_error result = PIPE_OK;
   enum pipe_format linerFormat = util_format_linear(format);
   const struct util_format_description *desc = util_format_description(format);

   for (i = 0; i < ARRAY_SIZE(texture_infos); i++) {
      if (texture_infos[i].mesa_format == linerFormat) {
         texture_desc->format = texture_infos[i].format;
         texture_desc->swizzles[0] = texture_infos[i].swizzles[0];
         texture_desc->swizzles[1] = texture_infos[i].swizzles[1];
         texture_desc->swizzles[2] = texture_infos[i].swizzles[2];
         texture_desc->swizzles[3] = texture_infos[i].swizzles[3];
         break;
      }
   }

   if (i >= ARRAY_SIZE(texture_infos)) {
      if (desc->layout == UTIL_FORMAT_LAYOUT_ASTC) {
         texture_desc->format = TX_FORMAT(0, 0x14);
         texture_desc->swizzles[0] = SWIZZLE_RED;
         texture_desc->swizzles[1] = SWIZZLE_GREEN;
         texture_desc->swizzles[2] = SWIZZLE_BLUE;
         texture_desc->swizzles[3] = SWIZZLE_ALPHA;
      } else
         result = PIPE_ERROR_BAD_INPUT;
   }

   /* FIX(jm9100-oss): TX_CONFIG2 INTEGER_SIGN_EXTENSION (bits 19:18) only applies
    * to PURE INTEGER textures (SIGNED8/SIGNED16).  The old test fired on SNORM
    * formats too, which set SIGNED16 on e.g. R16_SNORM/RG16_SNORM and made the
    * sampler return 0 for them.  Keep it for UINT/SINT only. */
   if (desc->channel[0].type == UTIL_FORMAT_TYPE_SIGNED &&
       !desc->channel[0].normalized) {
      if (desc->channel[0].size == 8)
         texture_desc->int_format = 0x1;
      else if (desc->channel[0].size == 16)
         texture_desc->int_format = 0x2;
      else
         texture_desc->int_format = 0x0;
   } else
      texture_desc->int_format = 0x0;

   if (format == PIPE_FORMAT_R5G6B5_UNORM || format == PIPE_FORMAT_B5G6R5_UNORM)
      texture_desc->int_format = 0x2;

   if (format == PIPE_FORMAT_X8Z24_UNORM || format == PIPE_FORMAT_S8_UINT_Z24_UNORM)
      texture_desc->int_format = 0x0;

   return result;
}

struct mwv207_hardware_Info pixel_infos[] = {
   {PIPE_FORMAT_B4G4R4X4_UNORM,     0x0,             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_B4G4R4A4_UNORM,     0x1,             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R4G4B4A4_UNORM,     0x1,             SWIZZLE_SETS(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_A4R4G4B4_UNORM,     0x1,             SWIZZLE_SETS(SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA, SWIZZLE_BLUE)},
   {PIPE_FORMAT_A4B4G4R4_UNORM,     0x1,             SWIZZLE_SETS(SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA, SWIZZLE_RED)},

   {PIPE_FORMAT_B5G5R5X1_UNORM,     0x2,             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_X1B5G5R5_UNORM,     0x2,             SWIZZLE_SETS(SWIZZLE_ALPHA, SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE)},

   {PIPE_FORMAT_B5G5R5A1_UNORM,     0x3,             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R5G5B5A1_UNORM,     0x3,             SWIZZLE_SETS(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_A1R5G5B5_UNORM,     0x3,             SWIZZLE_SETS(SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA, SWIZZLE_BLUE)},
   {PIPE_FORMAT_A1B5G5R5_UNORM,     0x3,             SWIZZLE_SETS(SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA, SWIZZLE_RED)},

   {PIPE_FORMAT_B5G6R5_UNORM,       0x4,               SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R5G6B5_UNORM,       0x4,               SWIZZLE_SETS(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R8_UNORM,           0x23,                   SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8_SNORM,           0x23,                   SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8_UINT,            0x17,                  SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8_SINT,            0x17,                  SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R8G8_UNORM,         0x1F,                 SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8G8_SNORM,         0x1F,                 SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8G8_UINT,          0x18,               SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8G8_SINT,          0x18,               SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_B8G8R8_UNORM,       0x5,             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_B8G8R8_SNORM,       0x5,             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8G8B8_UNORM,       0x5,             SWIZZLE_SETS(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8G8B8_SNORM,       0x5,             SWIZZLE_SETS(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_B8G8R8X8_UNORM,     0x5,             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_X8B8G8R8_UNORM,     0x5,             SWIZZLE_SETS(SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA, SWIZZLE_RED)},
   {PIPE_FORMAT_X8B8G8R8_SNORM,     0x5,             SWIZZLE_SETS(SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA, SWIZZLE_RED)},
   {PIPE_FORMAT_X8R8G8B8_UNORM,     0x5,             SWIZZLE_SETS(SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA, SWIZZLE_BLUE)},

   {PIPE_FORMAT_B8G8R8A8_UNORM,     0x6,             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_B8G8R8A8_SNORM,     0x6,             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R8G8B8A8_UNORM,     0x6,             SWIZZLE_SETS(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8G8B8A8_SNORM,     0x6,             SWIZZLE_SETS(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_A8R8G8B8_UNORM,     0x6,             SWIZZLE_SETS(SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA, SWIZZLE_BLUE)},

   {PIPE_FORMAT_A8B8G8R8_UNORM,     0x6,             SWIZZLE_SETS(SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA, SWIZZLE_RED)},
   {PIPE_FORMAT_A8B8G8R8_SNORM,     0x6,             SWIZZLE_SETS(SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA, SWIZZLE_RED)},

   {PIPE_FORMAT_R8G8B8A8_UINT,      0x19,         SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8G8B8A8_SINT,      0x19,         SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R10G10B10A2_UNORM,  0x16,          SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_B10G10R10A2_UNORM,  0x16,          SWIZZLE_SETS(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R10G10B10A2_UINT,   0x1E,      SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_B10G10R10A2_UINT,   0x1E,      SWIZZLE_SETS(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R11G11B10_FLOAT,    0x1D,         SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_A8_UNORM,           0x10,                   SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R16_UINT,           0x1A,                 SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16_SINT,           0x1A,                 SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16_FLOAT,          0x11,                 SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R32_UINT,           0x14,                 SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R32_SINT,           0x14,                 SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R32_FLOAT,          0x14,                 SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R16G16_UINT,        0x1B,             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16G16_SINT,        0x1B,             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16G16_FLOAT,       0x12,             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R32G32_UINT,        0x15,             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R32G32_SINT,        0x15,             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R32G32_FLOAT,       0x15,             SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R16G16B16A16_UINT,  0x1C,     SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16G16B16A16_SINT,  0x1C,     SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R32G32B32A32_UINT,  0x26, SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R32G32B32A32_SINT,  0x27, SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R16G16B16A16_FLOAT, 0x13,     SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R32G32B32A32_FLOAT, 0x25,     SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)}
};

enum pipe_error
mwv207_get_pixel_desc(enum pipe_format format, struct mwv207_pixel_desc *pixel_desc)
{
   uint32_t i;
   uint32_t output_mode = 0;
   enum pipe_format linerFormat = 0;

   const struct util_format_description *desc;

   linerFormat = util_format_linear(format);

    for (i = 0; i < ARRAY_SIZE(pixel_infos); i++) {
      if (pixel_infos[i].mesa_format == linerFormat) {
         pixel_desc->format = pixel_infos[i].format;
         pixel_desc->swizzles[0] = pixel_infos[i].swizzles[0];
         pixel_desc->swizzles[1] = pixel_infos[i].swizzles[1];
         pixel_desc->swizzles[2] = pixel_infos[i].swizzles[2];
         pixel_desc->swizzles[3] = pixel_infos[i].swizzles[3];
         break;
      }
   }

   if (i >= ARRAY_SIZE(pixel_infos))
      return PIPE_ERROR_BAD_INPUT;

   desc = util_format_description(format);
   pixel_desc->saturation = false;

   if (format == PIPE_FORMAT_R10G10B10A2_UINT ||
       format == PIPE_FORMAT_B10G10R10A2_UINT)
      output_mode = 0x4;
   else if (format == PIPE_FORMAT_R10G10B10A2_SINT ||
            format == PIPE_FORMAT_B10G10R10A2_SINT)
      output_mode = 0x6;
   else if (util_format_is_pure_integer(format)) {
      switch (desc->channel[0].type) {
      case UTIL_FORMAT_TYPE_UNSIGNED:
         switch (desc->channel[0].size) {
         case 8:
            output_mode = 0x3;
            break;
         case 16:
            output_mode = 0x4;
            break;
         case 32:
            output_mode = 0x2;
            break;
         }
         break;

      case UTIL_FORMAT_TYPE_SIGNED:
         switch (desc->channel[0].size) {
         case 8:
            output_mode = 0x5;
            break;
         case 16:
            output_mode = 0x6;
            break;
         case 32:
            output_mode = 0x2;
            break;
         }
         break;
      default:
         return PIPE_ERROR_BAD_INPUT;
      }
   } else {
      switch (desc->channel[0].size) {
      case 32:
         output_mode = 0x2;
         break;
      default:
         output_mode = 0x0;
         break;
      }
      pixel_desc->saturation = true;
   }

   pixel_desc->output_mode = output_mode;
   pixel_desc->srgb = util_format_is_srgb(format);
   pixel_desc->endian = mwv207_get_endian(format);

   return PIPE_OK;
}

static const struct mwv207_blit_Info blitInfos[] = {
   {PIPE_FORMAT_B4G4R4X4_UNORM,    0x0,    BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},

   {PIPE_FORMAT_B4G4R4A4_UNORM,    0x1,    BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R4G4B4A4_UNORM,    0x1,    BLT_SWIZZLE_EX(SWIZZLE_BLUE,  SWIZZLE_GREEN, SWIZZLE_RED,   SWIZZLE_ALPHA)},
   {PIPE_FORMAT_A4R4G4B4_UNORM,    0x1,    BLT_SWIZZLE_EX(SWIZZLE_GREEN, SWIZZLE_RED,   SWIZZLE_ALPHA, SWIZZLE_BLUE)},
   {PIPE_FORMAT_A4B4G4R4_UNORM,    0x1,    BLT_SWIZZLE_EX(SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA, SWIZZLE_RED)},

   {PIPE_FORMAT_B5G5R5X1_UNORM,    0x2,    BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_X1B5G5R5_UNORM,    0x2,    BLT_SWIZZLE_EX(SWIZZLE_ALPHA, SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE)},

   {PIPE_FORMAT_B5G5R5A1_UNORM,    0x3,    BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R5G5B5A1_UNORM,    0x3,    BLT_SWIZZLE_EX(SWIZZLE_BLUE,  SWIZZLE_GREEN, SWIZZLE_RED,   SWIZZLE_ALPHA)},

   {PIPE_FORMAT_B5G6R5_UNORM,      0x4,      BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R5G6B5_UNORM,      0x4,      BLT_SWIZZLE_EX(SWIZZLE_BLUE,  SWIZZLE_GREEN, SWIZZLE_RED,   SWIZZLE_ALPHA)},

   {PIPE_FORMAT_R8_UNORM,          0x23,          BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8G8_UNORM,        0x24,        BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},

   {PIPE_FORMAT_B8G8R8_UNORM,      0x22,      BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8G8B8_UNORM,      0x22,      BLT_SWIZZLE_EX(SWIZZLE_BLUE,  SWIZZLE_GREEN, SWIZZLE_RED,   SWIZZLE_ALPHA)},

   {PIPE_FORMAT_B8G8R8X8_UNORM,    0x5,    BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_X8B8G8R8_UNORM,    0x5,    BLT_SWIZZLE_EX(SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA, SWIZZLE_RED)},
   {PIPE_FORMAT_X8R8G8B8_UNORM,    0x5,    BLT_SWIZZLE_EX(SWIZZLE_GREEN, SWIZZLE_RED,   SWIZZLE_ALPHA, SWIZZLE_BLUE)},

   {PIPE_FORMAT_B8G8R8A8_UNORM,    0x6,    BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R8G8B8A8_UNORM,    0x6,    BLT_SWIZZLE_EX(SWIZZLE_BLUE,  SWIZZLE_GREEN, SWIZZLE_RED,   SWIZZLE_ALPHA)},

   {PIPE_FORMAT_A8R8G8B8_UNORM,    0x6,    BLT_SWIZZLE_EX(SWIZZLE_GREEN, SWIZZLE_RED,   SWIZZLE_ALPHA, SWIZZLE_BLUE)},
   {PIPE_FORMAT_A8B8G8R8_UNORM,    0x6,    BLT_SWIZZLE_EX(SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA, SWIZZLE_RED)},

   {PIPE_FORMAT_A2B10G10R10_UNORM, 0x16, BLT_SWIZZLE_EX(SWIZZLE_ALPHA, SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE)},
   {PIPE_FORMAT_A2R10G10B10_UNORM, 0x16, BLT_SWIZZLE_EX(SWIZZLE_GREEN, SWIZZLE_RED,   SWIZZLE_ALPHA, SWIZZLE_BLUE)},

   {PIPE_FORMAT_R10G10B10A2_UNORM, 0x16, BLT_SWIZZLE_EX(SWIZZLE_BLUE, SWIZZLE_GREEN, SWIZZLE_RED, SWIZZLE_ALPHA)},

   {PIPE_FORMAT_L8_UNORM,          0x21,          BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_A8_UNORM,          0x10,          BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_L8A8_UNORM,        0x20,        BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},

   {PIPE_FORMAT_S8_UINT_Z24_UNORM, 0x17,       BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_X8Z24_UNORM,       0x17,       BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_Z16_UNORM,         0x18,         BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},

   /* FIX(jm9100-oss): 16-bit normalized formats for the BLT/image-layout space.
    * mwv207_get_image_info() takes the resource's image format from
    * mwv207_get_blit_desc(), NOT from texture_infos[].  Without entries here the
    * fallback guesses by block size and lands on 0x24 (G8R8) for 2-byte and
    * 0x6 (A8R8G8B8) for 4-byte formats, so e.g. RG16_UNORM was laid out and
    * decoded as RGBA8 and sampled as 0.  Codes from the vendor register enum
    * MWV207REG_BLT_SRC_CONFIG_FORMAT_*:
    *   R16 = 0x27, R16G16 = 0x2F, R16G16B16A16 = 0x30. */
   {PIPE_FORMAT_R16_UNORM,         0x27,       BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16_SNORM,         0x27,       BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16G16_UNORM,      0x2F,       BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16G16_SNORM,      0x2F,       BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16G16B16A16_UNORM, 0x30,      BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
   {PIPE_FORMAT_R16G16B16A16_SNORM, 0x30,      BLT_SWIZZLE_EX(SWIZZLE_RED,   SWIZZLE_GREEN, SWIZZLE_BLUE,  SWIZZLE_ALPHA)},
};

enum pipe_error
mwv207_get_blit_desc(enum pipe_format format, struct mwv207_blit_desc *blt_desc)
{
   uint32_t i;
   uint32_t blit_format = 0;
   uint32_t swizzle = 0;
   enum pipe_format linerFormat = 0;

   linerFormat = util_format_linear(format);

   for (i = 0; i < ARRAY_SIZE(blitInfos); i++) {
      if (linerFormat == blitInfos[i].mesa_format) {
         blit_format = blitInfos[i].format;
         swizzle  = blitInfos[i].swizzle;
         break;
      }
   }

   if (i >= ARRAY_SIZE(blitInfos)) {
      switch (util_format_get_blocksize(format)) {
      case 1:
         blit_format =  0x23;
         break;
      case 2:
         if (util_format_get_nr_components(format) == 4)
            blit_format =  0x1;
         else
            blit_format =  0x24;
         break;
      case 3:
         blit_format =  0x22;
         break;
      case 4:
         blit_format =  0x6;
         break;
      case 8:
         blit_format =  0x1C;
         break;
      case 16:
         blit_format =  0x26;
         break;
      default:
         blit_format = PIPE_FORMAT_NONE;
      }

      swizzle = BLT_SWIZZLE_EX(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA);
      blt_desc->fake_format = true;
   } else
      blt_desc->fake_format = false;

   blt_desc->format      = blit_format;
   blt_desc->swizzle     = swizzle;
   if (util_format_is_pure_integer(format)) {
      blt_desc->sample_mode = 0x1;
   } else {
      blt_desc->sample_mode = 0x0;
   }
   blt_desc->srgb        = util_format_is_srgb(format);
   blt_desc->pixel_size  = util_format_get_blockwidth(format);
   blt_desc->endian      = mwv207_get_endian(format);

   return PIPE_OK;
}

enum pipe_error
mwv207_get_element_desc(struct mwv207_element *element)
{

   uint32_t type = 0;
   uint32_t size = 0;
   uint32_t normalized = 0x0;
   uint32_t endian = mwv207_get_endian(element->base.src_format);
   const struct util_format_description *desc = util_format_description(element->base.src_format);

   switch(desc->nr_channels) {
   case 1:
      size = 0x1;
      break;
   case 2:
      size = 0x2;
      break;
   case 3:
      size = 0x3;
      break;
   case 4:
      size = 0x0;
      break;
   default:
      return PIPE_ERROR_BAD_INPUT;
   }

   if (desc->channel[0].normalized) {
      if (desc->channel[0].size == 10)
         normalized = 0x1;
      else
         normalized = 0x2;
   }

   if (util_format_is_pure_integer(element->base.src_format)) {
      switch (desc->channel[0].size) {
      case 8:
         type = 0xE;
         break;
      case 16:
         type = 0xF;
         break;
      case 32:
         type = 0x10;
         break;
      default:
         return PIPE_ERROR_BAD_INPUT;
      }
      element->weight = 1;
      if (desc->channel[0].type == UTIL_FORMAT_TYPE_SIGNED)
         normalized = 0x1;
   } else {
      switch (desc->channel[0].type) {
      case UTIL_FORMAT_TYPE_UNSIGNED:
         switch (desc->channel[0].size) {
         case 8:
            type = 0x1;
            break;
         case 10:
            type = 0x7;
            break;
         case 16:
            type = 0x3;
            break;
         case 32:
            type = 0x5;
            break;
         }
         break;
      case UTIL_FORMAT_TYPE_SIGNED:
         switch (desc->channel[0].size) {
         case 8:
            type = 0x0;
            break;
         case 10:
            type = 0x6;
            break;
         case 16:
            type = 0x2;
            break;
         case 32:
            type = 0x4;
            break;
         }
         break;
      case UTIL_FORMAT_TYPE_FLOAT:
         switch (desc->channel[0].size) {
         case 16:
            type = 0x9;
            break;
         case 32:
            type = 0x8;
            break;
         }
         break;
      default:
         return PIPE_ERROR_BAD_INPUT;
      }

      element->weight = 0x3F800000;
   }

   element->fetch_size = util_format_get_blocksize(element->base.src_format);

   element->attribue_config =
      (ALIGN_BIT((uint32_t) (size), 0xC) & (0x3000)) |
      (ALIGN_BIT((uint32_t) (type), 0x0) & (0xF)) |
      (ALIGN_BIT((uint32_t) (type >> 4), 0x7) & (0x80)) |
      (ALIGN_BIT((uint32_t) (normalized), 0xE) & (0xC000)) |
      (ALIGN_BIT((uint32_t) (0x0), 0x6) & (0x40)) |
      (ALIGN_BIT((uint32_t) (element->base.vertex_buffer_index), 0x8) & (0xF00)) |
      (ALIGN_BIT((uint32_t) (element->base.src_offset), 0x10) & (0xFFFF0000)) |
      (ALIGN_BIT((uint32_t) (endian), 0x4) & (0x30));

   return PIPE_OK;
}

uint32_t
mwv207_get_format_desc(enum pipe_format format)
{
   uint32_t support = 0;
   struct mwv207_texture_desc texture_desc;

   if (mwv207_get_texture_desc(format, &texture_desc) == PIPE_OK)
      support |= PIPE_BIND_SAMPLER_VIEW;

   struct mwv207_pixel_desc pixel_desc;

   if (mwv207_get_pixel_desc(format, &pixel_desc) == PIPE_OK)
      support |= (PIPE_BIND_RENDER_TARGET | PIPE_BIND_BLENDABLE);

   struct mwv207_element element;
   element.base.src_format = format;

   if (mwv207_get_element_desc(&element) == PIPE_OK)
      support |= PIPE_BIND_VERTEX_BUFFER;

   if (format == PIPE_FORMAT_Z16_UNORM ||
       format == PIPE_FORMAT_Z32_FLOAT ||
       format == PIPE_FORMAT_X8Z24_UNORM ||
       format == PIPE_FORMAT_S8_UINT_Z24_UNORM ||
       format == PIPE_FORMAT_S8_UINT)
      support |= PIPE_BIND_DEPTH_STENCIL;

   if (format == PIPE_FORMAT_Z32_FLOAT_S8X24_UINT)
      return PIPE_BIND_DEPTH_STENCIL | PIPE_BIND_SAMPLER_VIEW;

   return support;
}

static void
mwv207_get_tile_mode(
   struct mwv207_resource *res,
   uint32_t *tiling,
   uint32_t *super_tile
)
{
   switch (res->mode) {
   case MWV207_TILED:
      *tiling     = ENABLE;
      *super_tile = DISABLE;
      break;

   case MWV207_SUPERTILED:
      *tiling     = ENABLE;
      *super_tile = ENABLE;
      break;

   default:
      *tiling     = DISABLE;
      *super_tile = DISABLE;
      break;
   }
}

enum pipe_error
mwv207_get_image_info(struct mwv207_resource *res)
{
   uint32_t level, super_tile, tiling, cache_mode;
   uint32_t size_config, src_tile_config, dst_tile_config;
   uint32_t src_compress = 0, dst_compress = 0;
   uint32_t msaa_mode = mwv207_get_blit_msaa_mode(res->sample_info.xscale, res->sample_info.yscale);
   struct mwv207_blit_desc blit_desc = {0};

   cache_mode = (CACHE_MODE == 0x1) ? ENABLE : DISABLE;

   mwv207_get_tile_mode(res, &tiling, &super_tile);

   if (util_format_get_blocksize(res->base.format) >= 8)
      size_config = 0x1;
   else
      size_config = 0x0;

   mwv207_get_blit_desc(res->base.format, &blit_desc);

   src_tile_config =
      (ALIGN_BIT((uint32_t) (DISABLE), 0x1) & (0x2)) |
      (ALIGN_BIT((uint32_t) (DISABLE), 0x0) & (0x1));

   dst_tile_config =
      (ALIGN_BIT((uint32_t) (DISABLE), 0x1) & (0x2)) |
      (ALIGN_BIT((uint32_t) (DISABLE), 0x0) & (0x1));

   if (res->ts.bo) {
      src_compress =
         (ALIGN_BIT((uint32_t) (res->ts.compressed), 0x1) & (0x2)) |
         (ALIGN_BIT((uint32_t) (res->ts.compressed_format), 0x4) & (0xF0));

      dst_compress =
         (ALIGN_BIT((uint32_t) (res->ts.compressed), 0x1) & (0x2)) |
         (ALIGN_BIT((uint32_t) (res->ts.compressed_format), 0x4) & (0xF0));

      if (res->mode == MWV207_SUPERTILED) {
         src_tile_config |= (ALIGN_BIT(0x1, 0x15) & (0x600000));
         dst_tile_config |= (ALIGN_BIT(0x1, 0x1A) & (0xC000000));
      }

      if (res->sample_info.is_msaa && res->ts.compressed) {
         src_tile_config |= (ALIGN_BIT((uint32_t) (ENABLE), 0x17) & (0x800000));
         dst_tile_config |= (ALIGN_BIT((uint32_t) (ENABLE), 0x1C) & (0x10000000));
      }
   }

   for (level = 0; level <= res->base.last_level; ++level) {
      struct mwv207_level_info *pLevel = &res->level_infos[level];

      pLevel->config =
         (ALIGN_BIT((uint32_t) (pLevel->stride), 0x0) & (0x1FFFFF)) |
         (ALIGN_BIT((uint32_t) (blit_desc.format), 0x15) & (0x7E00000)) |
         (ALIGN_BIT((uint32_t) (msaa_mode), 0x1B) & (0x18000000)) |
         (ALIGN_BIT((uint32_t) (tiling), 0x1D) & (0x20000000)) |
         (ALIGN_BIT((uint32_t) (super_tile), 0x1E) & (0x40000000)) |
         (ALIGN_BIT((uint32_t) (DISABLE), 0x1F) & (0x80000000));

      pLevel->dst_compress = dst_compress;
      pLevel->dst_configex = dst_tile_config |
         (ALIGN_BIT((uint32_t) (size_config), 0x14) & (0x100000)) |
         (ALIGN_BIT((uint32_t) (cache_mode), 0x11) & (0x20000)) |
         (ALIGN_BIT((uint32_t) (DISABLE), 0x0) & (0x1)) |
         (ALIGN_BIT((uint32_t) (DISABLE), 0x1) & (0x2));

      pLevel->src_compress = src_compress;
      pLevel->src_configex = src_tile_config |
         (ALIGN_BIT((uint32_t) (DISABLE), 0x1) & (0x2)) |
         (ALIGN_BIT((uint32_t) (DISABLE), 0x0) & (0x1)) |
         (ALIGN_BIT((uint32_t) (cache_mode), 0x11) & (0x20000)) |
         (ALIGN_BIT((uint32_t) (size_config), 0x13) & (0x80000));

      pLevel->blit_config =
         (ALIGN_BIT((uint32_t) (util_format_get_blocksize(res->base.format) - 1), 0x7) & (0x380)) |
         (ALIGN_BIT((uint32_t) (blit_desc.sample_mode), 0xC) & (0x1000));
   }

   return PIPE_OK;
}

enum pipe_error
mwv207_get_clearval(uint32_t format,
                    struct mwv207_clear_info info,
                    uint32_t *clear_val,
                    uint32_t *clear_mask)
{

   if (info.aspectMask & PIPE_CLEAR_COLOR) {
      union util_color uc = {0};
      union pipe_color_union color = info.color;

      if(format != PIPE_FORMAT_R8G8_R8B8_UNORM){
         util_pack_color_union(format, &uc, &color);
      }

      if (util_format_get_blocksize(format) == 1)
         clear_val[0] = clear_val[1] = (uc.ui[0] | uc.ui[0] << 8 | uc.ui[0] << 16 | uc.ui[0] << 24);
      else if (util_format_get_blocksize(format) == 2)
         clear_val[0] = clear_val[1] = (uc.ui[0] | uc.ui[0] << 16);
      else
         clear_val[0] = clear_val[1] = uc.ui[0];

      clear_mask[0] = clear_mask[1] = 0xFFFFFFFF;

      return PIPE_OK;
   } else if (info.aspectMask & PIPE_CLEAR_DEPTHSTENCIL) {
      uint32_t clear_bits_depth, clear_bits_stencil = 0;

      switch (format) {
      case PIPE_FORMAT_S8_UINT:
         clear_bits_depth = 0xffffff00;
         clear_bits_stencil = 0xff;
         break;

      case PIPE_FORMAT_Z16_UNORM:
      case PIPE_FORMAT_X8Z24_UNORM:
         clear_bits_depth = 0xffffffff;
         clear_bits_stencil = 0x00000000;
         break;

      case PIPE_FORMAT_S8_UINT_Z24_UNORM:
         clear_bits_depth = 0xffffff00;
         clear_bits_stencil = 0x000000ff;
         break;

      case PIPE_FORMAT_Z24_UNORM_S8_UINT:
         clear_bits_depth = 0x00ffffff;
         clear_bits_stencil = 0xff000000;
         break;

      case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT:
         clear_bits_depth = 0xffffff00;
         clear_bits_stencil = 0x000000ff;
         break;

      default:
         clear_bits_depth = clear_bits_stencil = 0xffffffff;
         break;
      }

      if (info.aspectMask & PIPE_CLEAR_DEPTH)
         clear_mask[0] |= clear_bits_depth;
      else
         info.depth = 0;

      if (info.aspectMask & PIPE_CLEAR_STENCIL)
         clear_mask[0] |= clear_bits_stencil;
      else
         info.stencil = 0;

      clear_val[0] = util_pack64_z_stencil(format, info.depth, (info.stencil & 0xff));

      if (format == PIPE_FORMAT_Z16_UNORM)
         clear_val[0] |= (clear_val[0] << 16);

      clear_val[1] = clear_val[0];
      clear_mask[1] = clear_mask[0];

      return PIPE_OK;
   }

   return PIPE_ERROR;
}

uint32_t
mwv207_get_compressed_format(enum pipe_format format)
{
   switch (format) {
   case PIPE_FORMAT_B8G8R8X8_UNORM:
   case PIPE_FORMAT_B8G8R8X8_SRGB:
   case PIPE_FORMAT_R8G8B8X8_UNORM:
      return 0x3;

   case PIPE_FORMAT_B8G8R8A8_UNORM:
   case PIPE_FORMAT_B8G8R8A8_SRGB:
   case PIPE_FORMAT_R8G8B8A8_UNORM:
      return 0x3;

   case PIPE_FORMAT_X8Z24_UNORM:
      return 0x6;

   case PIPE_FORMAT_S8X24_UINT:
   case PIPE_FORMAT_S8_UINT_Z24_UNORM:
      return 0x5;

   default:
      return -1;
   }
}

uint32_t
mwv207_update_colormask(struct mwv207_context *ctx, int num)
{
   uint32_t colormask = 0;

   if (ctx->blend_state->color_mask[num] == PIPE_MASK_RGBA)
      return ctx->blend_state->color_mask[num];

   if (ctx->blend_state->color_mask[num] & PIPE_MASK_R)
      colormask |= 0x1 << ctx->info.swizzle_record[num][0];
   if (ctx->blend_state->color_mask[num] & PIPE_MASK_G)
      colormask |= 0x1 << ctx->info.swizzle_record[num][1];
   if (ctx->blend_state->color_mask[num] & PIPE_MASK_B)
      colormask |= 0x1 << ctx->info.swizzle_record[num][2];
   if (ctx->blend_state->color_mask[num] & PIPE_MASK_A)
      colormask |= 0x1 << ctx->info.swizzle_record[num][3];

   return colormask;
}

bool
mwv207_get_sample_info(int num_samples,
                       uint8_t *xscale_out,
                       uint8_t *yscale_out,
                       uint8_t *nr_samples_out)
{
   uint8_t xscale, yscale, nr_samples;

   switch (num_samples) {
   case 0:
   case 1:
      xscale = 1;
      yscale = 1;
      nr_samples = 1;
      break;
   case 2:
   case 4:
      xscale = 2;
      yscale = 2;
      nr_samples = 4;
      break;
   default:
      return false;
   }

   if (xscale_out)
      *xscale_out = xscale;
   if (yscale_out)
      *yscale_out = yscale;
   if (nr_samples_out)
      *nr_samples_out = nr_samples;

   return true;
}

uint32_t
mwv207_get_blit_msaa_mode(uint8_t x_scale, uint8_t y_scale)
{
   if (x_scale == 2 && y_scale == 2)
      return 0x3;
   else
      return 0x0;
}

