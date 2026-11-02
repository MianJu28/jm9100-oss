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

#ifndef MWV207_9200_DEC_PPU_H
#define MWV207_9200_DEC_PPU_H
#include <stdint.h>
#include "pipe/p_video_codec.h"
#include "mwv207_dec_api_common.h"
#include "mwv207_video_dec_vpufeature.h"
#include "mwv207_video_reg_buf.h"

#define PP_CROP_MIN_WIDTH 48
#define PP_CROP_MIN_HEIGHT 48
#define PP_SCALE_IN_MAX_WIDTH 4096
#define PP_SCALE_IN_MAX_HEIGHT 2304
#define JPEG_PP_SCALE_IN_MAX_WIDTH 16384
#define JPEG_PP_SCALE_IN_MAX_HEIGHT 16384
#define PP_SCALE_OUT_MAX_WIDTH 4096
#define PP_SCALE_OUT_MAX_HEIGHT 2304

#define MWV207_SURFACE_ATTRIB_DEC400_WORK_MODE 2

#define PP_OUT_FMT_YUV420PACKED 0
#define PP_OUT_FMT_YUV420_P010 1
#define PP_OUT_FMT_YUV420_BIGE 2
#define PP_OUT_FMT_YUV420_8BIT 3
#define PP_OUT_FMT_YUV400 4 /* A.k.a., Monochrome/luma only*/
#define PP_OUT_FMT_YUV400_P010 5
#define PP_OUT_FMT_YUV400_8BIT 6
#define PP_OUT_FMT_IYUVPACKED 7
#define PP_OUT_FMT_IYUV_P010 8
#define PP_OUT_FMT_IYUV_8BIT 9
#define PP_OUT_FMT_YUV420_10 10
#define PP_OUT_FMT_RGB 11

#define PP_OUT_RGB888 0
#define PP_OUT_BGR888 1
#define PP_OUT_R16G16B16 2
#define PP_OUT_B16G16R16 3
#define PP_OUT_ARGB888 4
#define PP_OUT_ABGR888 5
#define PP_OUT_A2R10G10B10 6
#define PP_OUT_A2B10G10R10 7
#define PP_OUT_XRGB888 8
#define PP_OUT_XBGR888 9

#define IS_PACKED_RGB(fmt)                                                   \
   ((fmt == PP_OUT_RGB888) || (fmt == PP_OUT_ARGB888)                        \
    || (fmt == PP_OUT_A2R10G10B10) || (fmt == PP_OUT_R16G16B16)              \
    || (fmt == PP_OUT_ABGR888) || (fmt == PP_OUT_A2B10G10R10)                \
    || (fmt == PP_OUT_XRGB888) || (fmt == PP_OUT_XBGR888))

#define IS_PLANAR_RGB(fmt)                                                   \
   ((fmt == PP_OUT_RGB888) || (fmt == PP_OUT_BGR888)                         \
    || (fmt == PP_OUT_R16G16B16) || (fmt == PP_OUT_B16G16R16))

#define IS_ALPHA_FORMAT(fmt)                                                 \
   ((fmt == PP_OUT_ARGB888) || (fmt == PP_OUT_ABGR888)                       \
    || (fmt == PP_OUT_A2R10G10B10) || (fmt == PP_OUT_A2B10G10R10))

#define IS_4PLAN_FORMAT(fmt)                                                 \
   ((fmt) == PP_OUT_ARGB888 || (fmt) == PP_OUT_ABGR888                       \
    || (fmt) == PP_OUT_A2R10G10B10 || (fmt) == PP_OUT_A2B10G10R10            \
    || (fmt) == PP_OUT_XRGB888 || (fmt) == PP_OUT_XBGR888)
#define IS_8BITS_FORMAT(fmt)                                                 \
   ((fmt == PP_OUT_RGB888) || (fmt == PP_OUT_BGR888)                         \
    || (fmt == PP_OUT_ARGB888) || (fmt == PP_OUT_ABGR888)                    \
    || (fmt == PP_OUT_XRGB888) || (fmt == PP_OUT_XBGR888))

#define IS_16BITS_FORMAT(fmt)                                                \
   ((fmt == PP_OUT_R16G16B16) || (fmt == PP_OUT_B16G16R16))

#define PP_JM_LINEAR 0
#define PP_LANCZOS 1
#define PP_NEAREST 2
#define PP_BILINEAR 3
#define PP_BICUBIC 4
#define PP_SPLINE 5
#define PP_BOX 6

struct mwv207_pp_unit_regs {
   uint32_t PP_OUT_E_U;
   uint32_t PP_OUT_TILE_E_U;
   uint32_t PP_OUT_MODE;
   uint32_t PP_CR_FIRST;
   uint32_t PP_OUT_RGB_FMT_U;
   uint32_t PP_RGB_PLANAR_U;
   uint32_t PP_OUT_SWAP_U;
   uint32_t HOR_SCALE_MODE_U;
   uint32_t VER_SCALE_MODE_U;
   uint32_t OUT_FORMAT_U;
   uint32_t SCALE_HRATIO_U;
   uint32_t SCALE_WRATIO_U;
   uint32_t WSCALE_INVRA_U;
   uint32_t WSCALE_INVRA_EXT_U;
   uint32_t HSCALE_INVRA_U;
   uint32_t HCALE_INVRA_EXT_U;
   uint32_t PP_OUT_LU_BASE_U_MSB;
   uint32_t PP_OUT_LU_BASE_U_LSB;
   uint32_t PP_OUT_CH_BASE_U_MSB;
   uint32_t PP_OUT_CH_BASE_U_LSB;
   uint32_t PP_OUT_Y_STRIDE;
   uint32_t PP_OUT_C_STRIDE;
   uint32_t FLIP_MODE_U;
   uint32_t CROP_STARTX_U;
   uint32_t ROTATION_MODE_U;
   uint32_t CROP_STARTY_U;
   uint32_t PP_IN_WIDTH_U;
   uint32_t PP_IN_HEIGHT_U;
   uint32_t PP_OUT_WIDTH_U;
   uint32_t PP_OUT_HEIGHT_U;
   uint32_t PP_OUT_P010_FMT_U;
   uint32_t PP_OUT_LU_BOT_BASE_U_MSB;
   uint32_t PP_OUT_LU_BOT_BASE_U_LSB;
   uint32_t PP_OUT_CH_BOT_BASE_U_MSB;
   uint32_t PP_OUT_CH_BOT_BASE_U_LSB;
   uint32_t PP_CROP2_STARTX_U;
   uint32_t PP_CROP2_STARTY_U;
   uint32_t PP_CROP2_OUT_WIDTH_U;
   uint32_t PP_CROP2_OUT_HEIGHT_U;
   uint32_t PP_OUT_R_BASE_U_MSB;
   uint32_t PP_OUT_R_BASE_U_LSB;
   uint32_t PP_OUT_G_BASE_U_MSB;
   uint32_t PP_OUT_G_BASE_U_LSB;
   uint32_t PP_OUT_B_BASE_U_MSB;
   uint32_t PP_OUT_B_BASE_U_LSB;
   uint32_t PP_OUT_LANCZOS_TBL_BASE_U_MSB;
   uint32_t PP_OUT_LANCZOS_TBL_BASE_U_LSB;
   uint32_t PP_OUT_ALPHA_U;
   uint32_t PP_DUP_HOR_U;
   uint32_t PP_DUP_VER_U;
   uint32_t DITHER_SELECT_R_U;
   uint32_t DITHER_SELECT_G_U;
   uint32_t DITHER_SELECT_B_U;
   uint32_t SCALE1_OUT_WIDTH_U;
   uint32_t SCALE1_OUT_HEIGHT_U;
   uint32_t Y_FILTER_SIZE_U;
   uint32_t X_FILTER_SIZE_U;
};

#define DEC_MAX_OUT_COUNT 5

#define DEC_MAX_PPU_COUNT 5

extern struct mwv207_pp_unit_regs ppu_regs[DEC_MAX_OUT_COUNT];

/* Compared with mwv207_pp_unit_int_config, more internal member variables are
        added, which should not be exposed to external user. */
typedef struct {
   uint32_t enabled;
   uint32_t tiled_e;
   uint32_t rgb;
   uint32_t rgb_planar;
   uint32_t cr_first;
   uint32_t
      luma_offset;
   uint32_t chroma_offset;
   uint32_t luma_size; /* size of luma/chroma for ppu buffer */
   uint32_t chroma_size;
   uint32_t pixel_width;
   uint32_t stream_pixel_width;
   uint32_t shaper_enabled;
   uint32_t shaper_no_pad;
   uint32_t dec400_enabled;
   uint32_t planar;
   mwv207_dec_pic_alignment align;
   uint32_t ystride;
   uint32_t hstride;
   uint32_t cstride;
   uint32_t false_ystride;
   uint32_t false_cstride;
   struct {
      uint32_t enabled;
      uint32_t set_by_user;
      uint32_t x;
      uint32_t y;
      uint32_t width;
      uint32_t height;
   } crop;
   struct {
      uint32_t enabled;
      uint32_t x;
      uint32_t y;
      uint32_t width;
      uint32_t height;
   } crop2;
   struct {
      uint32_t enabled;
      uint32_t set_by_user;
      uint32_t x; 
      uint32_t y;
      uint32_t ratio_x;
      uint32_t ratio_y;
      uint32_t width;
      uint32_t height;
   } scale;
   uint32_t monochrome;
   uint32_t out_format;
   uint32_t out_p010;
   uint32_t out_1010;
   uint32_t out_I010;
   uint32_t out_L010;
   uint32_t out_cut_8bits;
   uint32_t video_range;
   uint32_t range_max;
   uint32_t range_min;
   uint32_t rgb_format;
   uint32_t rgb_stan;
   uint32_t rgb_alpha;
   uint32_t dither_r;
   uint32_t dither_g;
   uint32_t dither_b;
   uint32_t x_filter_size;
   uint32_t y_filter_size;
   uint32_t out_width;
   uint32_t out_height;
   uint32_t out_ratio_x;
   uint32_t out_ratio_y;
   uint32_t pp_lanczos;
   uint32_t pp_filter;
   uint32_t x_filter_param;
   uint32_t y_filter_param;
   struct mwv207_video_buffer *lanczos_table;
   struct pipe_video_buffer *outsurface;
   uint32_t outsurface_origin_width;
   uint32_t outsurface_origin_height;
   uint64_t pp_addr;
   uint32_t dec400_luma_table_size;
   uint32_t dec400_chroma_table_size;
   uint32_t pp_pic_size;
} mwv207_pp_unit_int_config;

void mwv207_calc_each_pp_buffer_size(mwv207_pp_unit_int_config *ppu_cfg,
                                    uint32_t dec400_mode,
                                    uint32_t mono_chrome);

uint32_t mwv207_check_pp_unit_config(struct mwv207_video_dec_hw_features *hw_feature,
                               uint32_t in_width,
                               uint32_t in_height,
                               uint32_t interlace,
                               mwv207_pp_unit_int_config *ppu_cfg);

void mwv207_cal_second_up_scale_ratio(mwv207_pp_unit_int_config *ppu_cfg,
                                     uint32_t interlace);

void mwv207_pp_unit_set_int_config(mwv207_pp_unit_int_config *ppu_int_cfg,
                                  mwv207_pp_unit_config *ppu_ext_cfg,
                                  uint32_t pixel_width,
                                  uint32_t frame_only,
                                  uint32_t mono_chrome);

void mwv207_pp_set_regs(struct mwv207_video_reg_buf *reg_buf,
                       struct mwv207_video_dec_hw_features *hw_feature,
                       mwv207_pp_unit_int_config *ppu_cfg,
                       uint32_t mono_chrome,
                       uint32_t bottom_field_flag);

enum mwv207_dec_picture_format
mwv207_trans_unit_config_2_format(mwv207_pp_unit_int_config *ppu_int_cfg);
void mwv207_check_output_format(mwv207_pp_unit_int_config *ppu_cfg,
                               enum mwv207_dec_picture_format *output_format,
                               uint32_t index);
uint32_t mwv207_calc_pp_unit_buffer_size(mwv207_pp_unit_int_config *ppu_cfg,
                                   uint32_t mono_chrome);

#define PI 3.141592653589793
#define TABLE_LENGTH 5
#define TABLE_SIZE 32
#define LAN_WINDOW_HOR 4
#define LAN_WINDOW_VER 3
#define LAN_SIZE_HOR (2 * LAN_WINDOW_HOR + 1)
#define LAN_SIZE_VER (2 * LAN_WINDOW_VER + 1)
#define LANCZOS_MAX_DOWN_RATIO 32
#define MAX_COEFF_SIZE_HOR_REAL                                              \
   (2 * LAN_WINDOW_HOR * LANCZOS_MAX_DOWN_RATIO + 1)
#define MAX_COEFF_SIZE_VER_REAL                                              \
   (2 * LAN_WINDOW_VER * LANCZOS_MAX_DOWN_RATIO + 1)
#define MAX_COEFF_SIZE_HOR (((MAX_COEFF_SIZE_HOR_REAL + 3) / 4) * 4)
#define MAX_COEFF_SIZE_VER (((MAX_COEFF_SIZE_VER_REAL + 3) / 4) * 4)
#define LANCZOS_TABLE_SIZE                                                   \
   (TABLE_SIZE / 2 + 1) * (MAX_COEFF_SIZE_HOR + MAX_COEFF_SIZE_VER)
#define LANCZOS_EDGE_SIZE                                                    \
   (2 * (((LAN_WINDOW_HOR + 3) / 4) * 4 + ((LAN_WINDOW_VER + 3) / 4) * 4))
#define LANCZOS_MAX_HEIGHT 4096
#define LANCZOS_TILE_EDGE_SIZE (3 * LANCZOS_MAX_HEIGHT * 64 / 4)
#define LANCZOS_BUFFER_SIZE (512 * 1024 / 4 + LANCZOS_TILE_EDGE_SIZE)
#define FIXED_BITS_UNI 16
#define MAKE_FIXED_UNI(x) ((x) << FIXED_BITS_UNI)
#define FIXED_RADIX MAKE_FIXED_UNI(1)
#define FLOOR(x) ((x) / (FIXED_RADIX))
#define DEC400_YUV_TABLE_SIZE (4 * 2304)
#define DEC400_PP_TABLE_SIZE (2 * DEC400_YUV_TABLE_SIZE)
#define DEC400_PPn_TABLE_OFFSET(n) (((n) * (DEC400_PP_TABLE_SIZE)))
#define DEC400_PPn_Y_TABLE_OFFSET(n) (DEC400_PPn_TABLE_OFFSET(n))
#define DEC400_PPn_UV_TABLE_OFFSET(n)                                        \
   (DEC400_PPn_TABLE_OFFSET(n) + DEC400_YUV_TABLE_SIZE)

#endif

