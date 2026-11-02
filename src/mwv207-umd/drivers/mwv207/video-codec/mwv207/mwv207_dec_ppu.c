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

#include <math.h>
// #include <va/va.h>

// #include "mwv207_decoder_cfg.h" 
#include "mwv207_dec_ppu.h"
#include "mwv207_video_dec_cfg.h"
#include "mwv207_video_dec_reg.h"
#include "mwv207_log.h"
#include "mwv207_resource.h"
#include "mwv207_video_buffer.h"
#include "vl/vl_video_buffer.h"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define PP_MAX_STRIDE 65536
#define DOWN_SCALE_SIZE(w, ds) (((w) / (ds)) & ~0x1)
#define NEXT_MULTIPLE(value, n) (((value) + (n)-1) & ~((n)-1))
#define ALIGN(a) (1 << (a))
#define TOFIX(d, q) ((uint64_t)((d) * ((uint64_t)1 << (q))))
#define FDIVI(a, b) ((a) / (b))

struct mwv207_pp_unit_regs ppu_regs[DEC_MAX_PPU_COUNT] = {
   {

      .PP_OUT_E_U = JMDEC_PP_OUT_E_U,
      .PP_OUT_TILE_E_U = JMDEC_PP_OUT_TILE_E_U,
      .PP_OUT_MODE = JMDEC_PP_OUT_MODE,
      .PP_CR_FIRST = JMDEC_PP_CR_FIRST,
      .PP_OUT_RGB_FMT_U = JMDEC_PP_OUT_RGB_FMT_U,
      .PP_RGB_PLANAR_U = JMDEC_PP_RGB_PLANAR_U,
      .PP_OUT_SWAP_U = JMDEC_PP_OUT_SWAP_U,
      .HOR_SCALE_MODE_U = JMDEC_HOR_SCALE_MODE_U,
      .VER_SCALE_MODE_U = JMDEC_VER_SCALE_MODE_U,
      .OUT_FORMAT_U = JMDEC_PP_OUT_FORMAT_U,
      .SCALE_HRATIO_U = JMDEC_SCALE_HRATIO_U,
      .SCALE_WRATIO_U = JMDEC_SCALE_WRATIO_U,
      .WSCALE_INVRA_U = JMDEC_WSCALE_INVRA_U,
      .WSCALE_INVRA_EXT_U = JMDEC_PP0_WSCALE_INVRA_EXT_U,
      .HSCALE_INVRA_U = JMDEC_HSCALE_INVRA_U,
      .HCALE_INVRA_EXT_U = JMDEC_PP0_HCALE_INVRA_EXT_U,
      .PP_OUT_LU_BASE_U_MSB = JMDEC_PP_OUT_LU_BASE_U_MSB,
      .PP_OUT_LU_BASE_U_LSB = JMDEC_PP_OUT_LU_BASE_U_LSB,
      .PP_OUT_CH_BASE_U_MSB = JMDEC_PP_OUT_CH_BASE_U_MSB,
      .PP_OUT_CH_BASE_U_LSB = JMDEC_PP_OUT_CH_BASE_U_LSB,
      .PP_OUT_Y_STRIDE = JMDEC_PP_OUT_Y_STRIDE,
      .PP_OUT_C_STRIDE = JMDEC_PP_OUT_C_STRIDE,
      .FLIP_MODE_U = JMDEC_FLIP_MODE_U,
      .CROP_STARTX_U = JMDEC_CROP_STARTX_U,
      .ROTATION_MODE_U = JMDEC_ROTATION_MODE_U,
      .CROP_STARTY_U = JMDEC_CROP_STARTY_U,
      .PP_IN_WIDTH_U = JMDEC_PP_IN_WIDTH_U,
      .PP_IN_HEIGHT_U = JMDEC_PP_IN_HEIGHT_U,
      .PP_OUT_WIDTH_U = JMDEC_PP_OUT_WIDTH_U,
      .PP_OUT_HEIGHT_U = JMDEC_PP_OUT_HEIGHT_U,
      .PP_OUT_P010_FMT_U = JMDEC_PP_OUT_P010_FMT_U,
      .PP_OUT_LU_BOT_BASE_U_MSB = JMDEC_PP_OUT_LU_BOT_BASE_U_MSB,
      .PP_OUT_LU_BOT_BASE_U_LSB = JMDEC_PP_OUT_LU_BOT_BASE_U_LSB,
      .PP_OUT_CH_BOT_BASE_U_MSB = JMDEC_PP_OUT_CH_BOT_BASE_U_MSB,
      .PP_OUT_CH_BOT_BASE_U_LSB = JMDEC_PP_OUT_CH_BOT_BASE_U_LSB,
      .PP_CROP2_STARTX_U = JMDEC_PP_CROP2_STARTX_U,
      .PP_CROP2_STARTY_U = JMDEC_PP_CROP2_STARTY_U,
      .PP_CROP2_OUT_WIDTH_U = JMDEC_PP_CROP2_OUT_WIDTH_U,
      .PP_CROP2_OUT_HEIGHT_U = JMDEC_PP_CROP2_OUT_HEIGHT_U,
      .PP_OUT_R_BASE_U_MSB = JMDEC_PP_OUT_R_BASE_U_MSB,
      .PP_OUT_R_BASE_U_LSB = JMDEC_PP_OUT_R_BASE_U_LSB,
      .PP_OUT_G_BASE_U_MSB = JMDEC_PP_OUT_G_BASE_U_MSB,
      .PP_OUT_G_BASE_U_LSB = JMDEC_PP_OUT_G_BASE_U_LSB,
      .PP_OUT_B_BASE_U_MSB = JMDEC_PP_OUT_B_BASE_U_MSB,
      .PP_OUT_B_BASE_U_LSB = JMDEC_PP_OUT_B_BASE_U_LSB,
      .PP_OUT_LANCZOS_TBL_BASE_U_MSB = JMDEC_PP0_LANCZOS_TBL_BASE_U_MSB,
      .PP_OUT_LANCZOS_TBL_BASE_U_LSB = JMDEC_PP0_LANCZOS_TBL_BASE_U_LSB,
      .PP_OUT_ALPHA_U = JMDEC_PP0_OUT_ALPHA_U,
      .PP_DUP_HOR_U = JMDEC_PP0_DUP_HOR_U,
      .PP_DUP_VER_U = JMDEC_PP0_DUP_VER_U,
      .DITHER_SELECT_R_U = JMDEC_DITHER_SELECT_R_U,
      .DITHER_SELECT_G_U = JMDEC_DITHER_SELECT_G_U,
      .DITHER_SELECT_B_U = JMDEC_DITHER_SELECT_B_U,
      .SCALE1_OUT_WIDTH_U = JMDEC_PP0_SCALE1_OUT_WIDTH_U,
      .SCALE1_OUT_HEIGHT_U = JMDEC_PP0_SCALE1_OUT_HEIGHT_U,
      .Y_FILTER_SIZE_U = JMDEC_PP0_Y_FILTER_SIZE_U,
      .X_FILTER_SIZE_U = JMDEC_PP0_X_FILTER_SIZE_U,
   },
   {

      .PP_OUT_E_U = JMDEC_PP1_OUT_E_U,
      .PP_OUT_TILE_E_U = JMDEC_PP1_OUT_TILE_E_U,
      .PP_OUT_MODE = JMDEC_PP1_OUT_MODE,
      .PP_CR_FIRST = JMDEC_PP1_CR_FIRST,
      .PP_OUT_RGB_FMT_U = JMDEC_PP1_OUT_RGB_FMT_U,
      .PP_RGB_PLANAR_U = JMDEC_PP1_RGB_PLANAR_U,
      .PP_OUT_SWAP_U = JMDEC_PP1_OUT_SWAP_U,
      .HOR_SCALE_MODE_U = JMDEC_PP1_HOR_SCALE_MODE_U,
      .VER_SCALE_MODE_U = JMDEC_PP1_VER_SCALE_MODE_U,
      .OUT_FORMAT_U = JMDEC_PP1_OUT_FORMAT_U,
      .SCALE_HRATIO_U = JMDEC_PP1_SCALE_HRATIO_U,
      .SCALE_WRATIO_U = JMDEC_PP1_SCALE_WRATIO_U,
      .WSCALE_INVRA_U = JMDEC_PP1_WSCALE_INVRA_U,
      .WSCALE_INVRA_EXT_U = JMDEC_PP1_WSCALE_INVRA_EXT_U,
      .HSCALE_INVRA_U = JMDEC_PP1_HSCALE_INVRA_U,
      .HCALE_INVRA_EXT_U = JMDEC_PP1_HCALE_INVRA_EXT_U,
      .PP_OUT_LU_BASE_U_MSB = JMDEC_PP1_OUT_LU_BASE_U_MSB,
      .PP_OUT_LU_BASE_U_LSB = JMDEC_PP1_OUT_LU_BASE_U_LSB,
      .PP_OUT_CH_BASE_U_MSB = JMDEC_PP1_OUT_CH_BASE_U_MSB,
      .PP_OUT_CH_BASE_U_LSB = JMDEC_PP1_OUT_CH_BASE_U_LSB,
      .PP_OUT_Y_STRIDE = JMDEC_PP1_OUT_Y_STRIDE,
      .PP_OUT_C_STRIDE = JMDEC_PP1_OUT_C_STRIDE,
      .FLIP_MODE_U = JMDEC_PP1_FLIP_MODE_U,
      .CROP_STARTX_U = JMDEC_PP1_CROP_STARTX_U,
      .ROTATION_MODE_U = JMDEC_PP1_ROTATION_MODE_U,
      .CROP_STARTY_U = JMDEC_PP1_CROP_STARTY_U,
      .PP_IN_WIDTH_U = JMDEC_PP1_IN_WIDTH_U,
      .PP_IN_HEIGHT_U = JMDEC_PP1_IN_HEIGHT_U,
      .PP_OUT_WIDTH_U = JMDEC_PP1_OUT_WIDTH_U,
      .PP_OUT_HEIGHT_U = JMDEC_PP1_OUT_HEIGHT_U,
      .PP_OUT_P010_FMT_U = JMDEC_PP1_OUT_P010_FMT_U,
      .PP_OUT_LU_BOT_BASE_U_MSB = JMDEC_PP1_OUT_LU_BOT_BASE_U_MSB,
      .PP_OUT_LU_BOT_BASE_U_LSB = JMDEC_PP1_OUT_LU_BOT_BASE_U_LSB,
      .PP_OUT_CH_BOT_BASE_U_MSB = JMDEC_PP1_OUT_CH_BOT_BASE_U_MSB,
      .PP_OUT_CH_BOT_BASE_U_LSB = JMDEC_PP1_OUT_CH_BOT_BASE_U_LSB,
      .PP_CROP2_STARTX_U = JMDEC_PP1_CROP2_STARTX_U,
      .PP_CROP2_STARTY_U = JMDEC_PP1_CROP2_STARTY_U,
      .PP_CROP2_OUT_WIDTH_U = JMDEC_PP1_CROP2_OUT_WIDTH_U,
      .PP_CROP2_OUT_HEIGHT_U = JMDEC_PP1_CROP2_OUT_HEIGHT_U,
      .PP_OUT_R_BASE_U_MSB = JMDEC_PP1_OUT_R_BASE_U_MSB,
      .PP_OUT_R_BASE_U_LSB = JMDEC_PP1_OUT_R_BASE_U_LSB,
      .PP_OUT_G_BASE_U_MSB = JMDEC_PP1_OUT_G_BASE_U_MSB,
      .PP_OUT_G_BASE_U_LSB = JMDEC_PP1_OUT_G_BASE_U_LSB,
      .PP_OUT_B_BASE_U_MSB = JMDEC_PP1_OUT_B_BASE_U_MSB,
      .PP_OUT_B_BASE_U_LSB = JMDEC_PP1_OUT_B_BASE_U_LSB,
      .PP_OUT_LANCZOS_TBL_BASE_U_MSB = JMDEC_PP1_LANCZOS_TBL_BASE_U_MSB,
      .PP_OUT_LANCZOS_TBL_BASE_U_LSB = JMDEC_PP1_LANCZOS_TBL_BASE_U_LSB,
      .PP_OUT_ALPHA_U = JMDEC_PP1_OUT_ALPHA_U,
      .PP_DUP_HOR_U = JMDEC_PP1_DUP_HOR_U,
      .PP_DUP_VER_U = JMDEC_PP1_DUP_VER_U,
      .DITHER_SELECT_R_U = JMDEC_PP1_DITHER_SELECT_R_U,
      .DITHER_SELECT_G_U = JMDEC_PP1_DITHER_SELECT_G_U,
      .DITHER_SELECT_B_U = JMDEC_PP1_DITHER_SELECT_B_U,
      .SCALE1_OUT_WIDTH_U = JMDEC_PP1_SCALE1_OUT_WIDTH_U,
      .SCALE1_OUT_HEIGHT_U = JMDEC_PP1_SCALE1_OUT_HEIGHT_U,
      .Y_FILTER_SIZE_U = JMDEC_PP1_Y_FILTER_SIZE_U,
      .X_FILTER_SIZE_U = JMDEC_PP1_X_FILTER_SIZE_U,
   },
   {

      .PP_OUT_E_U = JMDEC_PP2_OUT_E_U,
      .PP_OUT_TILE_E_U = JMDEC_PP2_OUT_TILE_E_U,
      .PP_OUT_MODE = JMDEC_PP2_OUT_MODE,
      .PP_CR_FIRST = JMDEC_PP2_CR_FIRST,
      .PP_OUT_RGB_FMT_U = JMDEC_PP2_OUT_RGB_FMT_U,
      .PP_RGB_PLANAR_U = JMDEC_PP2_RGB_PLANAR_U,
      .PP_OUT_SWAP_U = JMDEC_PP2_OUT_SWAP_U,
      .HOR_SCALE_MODE_U = JMDEC_PP2_HOR_SCALE_MODE_U,
      .VER_SCALE_MODE_U = JMDEC_PP2_VER_SCALE_MODE_U,
      .OUT_FORMAT_U = JMDEC_PP2_OUT_FORMAT_U,
      .SCALE_HRATIO_U = JMDEC_PP2_SCALE_HRATIO_U,
      .SCALE_WRATIO_U = JMDEC_PP2_SCALE_WRATIO_U,
      .WSCALE_INVRA_U = JMDEC_PP2_WSCALE_INVRA_U,
      .WSCALE_INVRA_EXT_U = JMDEC_PP2_WSCALE_INVRA_EXT_U,
      .HSCALE_INVRA_U = JMDEC_PP2_HSCALE_INVRA_U,
      .HCALE_INVRA_EXT_U = JMDEC_PP2_HCALE_INVRA_EXT_U,
      .PP_OUT_LU_BASE_U_MSB = JMDEC_PP2_OUT_LU_BASE_U_MSB,
      .PP_OUT_LU_BASE_U_LSB = JMDEC_PP2_OUT_LU_BASE_U_LSB,
      .PP_OUT_CH_BASE_U_MSB = JMDEC_PP2_OUT_CH_BASE_U_MSB,
      .PP_OUT_CH_BASE_U_LSB = JMDEC_PP2_OUT_CH_BASE_U_LSB,
      .PP_OUT_Y_STRIDE = JMDEC_PP2_OUT_Y_STRIDE,
      .PP_OUT_C_STRIDE = JMDEC_PP2_OUT_C_STRIDE,
      .FLIP_MODE_U = JMDEC_PP2_FLIP_MODE_U,
      .CROP_STARTX_U = JMDEC_PP2_CROP_STARTX_U,
      .ROTATION_MODE_U = JMDEC_PP2_ROTATION_MODE_U,
      .CROP_STARTY_U = JMDEC_PP2_CROP_STARTY_U,
      .PP_IN_WIDTH_U = JMDEC_PP2_IN_WIDTH_U,
      .PP_IN_HEIGHT_U = JMDEC_PP2_IN_HEIGHT_U,
      .PP_OUT_WIDTH_U = JMDEC_PP2_OUT_WIDTH_U,
      .PP_OUT_HEIGHT_U = JMDEC_PP2_OUT_HEIGHT_U,
      .PP_OUT_P010_FMT_U = JMDEC_PP2_OUT_P010_FMT_U,
      .PP_OUT_LU_BOT_BASE_U_MSB = JMDEC_PP2_OUT_LU_BOT_BASE_U_MSB,
      .PP_OUT_LU_BOT_BASE_U_LSB = JMDEC_PP2_OUT_LU_BOT_BASE_U_LSB,
      .PP_OUT_CH_BOT_BASE_U_MSB = JMDEC_PP2_OUT_CH_BOT_BASE_U_MSB,
      .PP_OUT_CH_BOT_BASE_U_LSB = JMDEC_PP2_OUT_CH_BOT_BASE_U_LSB,
      .PP_CROP2_STARTX_U = JMDEC_PP2_CROP2_STARTX_U,
      .PP_CROP2_STARTY_U = JMDEC_PP2_CROP2_STARTY_U,
      .PP_CROP2_OUT_WIDTH_U = JMDEC_PP2_CROP2_OUT_WIDTH_U,
      .PP_CROP2_OUT_HEIGHT_U = JMDEC_PP2_CROP2_OUT_HEIGHT_U,
      .PP_OUT_R_BASE_U_MSB = JMDEC_PP2_OUT_R_BASE_U_MSB,
      .PP_OUT_R_BASE_U_LSB = JMDEC_PP2_OUT_R_BASE_U_LSB,
      .PP_OUT_G_BASE_U_MSB = JMDEC_PP2_OUT_G_BASE_U_MSB,
      .PP_OUT_G_BASE_U_LSB = JMDEC_PP2_OUT_G_BASE_U_LSB,
      .PP_OUT_B_BASE_U_MSB = JMDEC_PP2_OUT_B_BASE_U_MSB,
      .PP_OUT_B_BASE_U_LSB = JMDEC_PP2_OUT_B_BASE_U_LSB,
      .PP_OUT_LANCZOS_TBL_BASE_U_MSB = JMDEC_PP2_LANCZOS_TBL_BASE_U_MSB,
      .PP_OUT_LANCZOS_TBL_BASE_U_LSB = JMDEC_PP2_LANCZOS_TBL_BASE_U_LSB,
      .PP_OUT_ALPHA_U = JMDEC_PP2_OUT_ALPHA_U,
      .PP_DUP_HOR_U = JMDEC_PP2_DUP_HOR_U,
      .PP_DUP_VER_U = JMDEC_PP2_DUP_VER_U,
      .DITHER_SELECT_R_U = JMDEC_PP2_DITHER_SELECT_R_U,
      .DITHER_SELECT_G_U = JMDEC_PP2_DITHER_SELECT_G_U,
      .DITHER_SELECT_B_U = JMDEC_PP2_DITHER_SELECT_B_U,
      .SCALE1_OUT_WIDTH_U = JMDEC_PP2_SCALE1_OUT_WIDTH_U,
      .SCALE1_OUT_HEIGHT_U = JMDEC_PP2_SCALE1_OUT_HEIGHT_U,
      .Y_FILTER_SIZE_U = JMDEC_PP2_Y_FILTER_SIZE_U,
      .X_FILTER_SIZE_U = JMDEC_PP2_X_FILTER_SIZE_U,
   },
   {

      .PP_OUT_E_U = JMDEC_PP3_OUT_E_U,
      .PP_OUT_TILE_E_U = JMDEC_PP3_OUT_TILE_E_U,
      .PP_OUT_MODE = JMDEC_PP3_OUT_MODE,
      .PP_CR_FIRST = JMDEC_PP3_CR_FIRST,
      .PP_OUT_RGB_FMT_U = JMDEC_PP3_OUT_RGB_FMT_U,
      .PP_RGB_PLANAR_U = JMDEC_PP3_RGB_PLANAR_U,
      .PP_OUT_SWAP_U = JMDEC_PP3_OUT_SWAP_U,
      .HOR_SCALE_MODE_U = JMDEC_PP3_HOR_SCALE_MODE_U,
      .VER_SCALE_MODE_U = JMDEC_PP3_VER_SCALE_MODE_U,
      .OUT_FORMAT_U = JMDEC_PP3_OUT_FORMAT_U,
      .SCALE_HRATIO_U = JMDEC_PP3_SCALE_HRATIO_U,
      .SCALE_WRATIO_U = JMDEC_PP3_SCALE_WRATIO_U,
      .WSCALE_INVRA_U = JMDEC_PP3_WSCALE_INVRA_U,
      .WSCALE_INVRA_EXT_U = JMDEC_PP3_WSCALE_INVRA_EXT_U,
      .HSCALE_INVRA_U = JMDEC_PP3_HSCALE_INVRA_U,
      .HCALE_INVRA_EXT_U = JMDEC_PP3_HCALE_INVRA_EXT_U,
      .PP_OUT_LU_BASE_U_MSB = JMDEC_PP3_OUT_LU_BASE_U_MSB,
      .PP_OUT_LU_BASE_U_LSB = JMDEC_PP3_OUT_LU_BASE_U_LSB,
      .PP_OUT_CH_BASE_U_MSB = JMDEC_PP3_OUT_CH_BASE_U_MSB,
      .PP_OUT_CH_BASE_U_LSB = JMDEC_PP3_OUT_CH_BASE_U_LSB,
      .PP_OUT_Y_STRIDE = JMDEC_PP3_OUT_Y_STRIDE,
      .PP_OUT_C_STRIDE = JMDEC_PP3_OUT_C_STRIDE,
      .FLIP_MODE_U = JMDEC_PP3_FLIP_MODE_U,
      .CROP_STARTX_U = JMDEC_PP3_CROP_STARTX_U,
      .ROTATION_MODE_U = JMDEC_PP3_ROTATION_MODE_U,
      .CROP_STARTY_U = JMDEC_PP3_CROP_STARTY_U,
      .PP_IN_WIDTH_U = JMDEC_PP3_IN_WIDTH_U,
      .PP_IN_HEIGHT_U = JMDEC_PP3_IN_HEIGHT_U,
      .PP_OUT_WIDTH_U = JMDEC_PP3_OUT_WIDTH_U,
      .PP_OUT_HEIGHT_U = JMDEC_PP3_OUT_HEIGHT_U,
      .PP_OUT_P010_FMT_U = JMDEC_PP3_OUT_P010_FMT_U,
      .PP_OUT_LU_BOT_BASE_U_MSB = JMDEC_PP3_OUT_LU_BOT_BASE_U_MSB,
      .PP_OUT_LU_BOT_BASE_U_LSB = JMDEC_PP3_OUT_LU_BOT_BASE_U_LSB,
      .PP_OUT_CH_BOT_BASE_U_MSB = JMDEC_PP3_OUT_CH_BOT_BASE_U_MSB,
      .PP_OUT_CH_BOT_BASE_U_LSB = JMDEC_PP3_OUT_CH_BOT_BASE_U_LSB,
      .PP_CROP2_STARTX_U = JMDEC_PP3_CROP2_STARTX_U,
      .PP_CROP2_STARTY_U = JMDEC_PP3_CROP2_STARTY_U,
      .PP_CROP2_OUT_WIDTH_U = JMDEC_PP3_CROP2_OUT_WIDTH_U,
      .PP_CROP2_OUT_HEIGHT_U = JMDEC_PP3_CROP2_OUT_HEIGHT_U,
      .PP_OUT_R_BASE_U_MSB = JMDEC_PP3_OUT_R_BASE_U_MSB,
      .PP_OUT_R_BASE_U_LSB = JMDEC_PP3_OUT_R_BASE_U_LSB,
      .PP_OUT_G_BASE_U_MSB = JMDEC_PP3_OUT_G_BASE_U_MSB,
      .PP_OUT_G_BASE_U_LSB = JMDEC_PP3_OUT_G_BASE_U_LSB,
      .PP_OUT_B_BASE_U_MSB = JMDEC_PP3_OUT_B_BASE_U_MSB,
      .PP_OUT_B_BASE_U_LSB = JMDEC_PP3_OUT_B_BASE_U_LSB,
      .PP_OUT_LANCZOS_TBL_BASE_U_MSB = JMDEC_PP3_LANCZOS_TBL_BASE_U_MSB,
      .PP_OUT_LANCZOS_TBL_BASE_U_LSB = JMDEC_PP3_LANCZOS_TBL_BASE_U_LSB,
      .PP_OUT_ALPHA_U = JMDEC_PP3_OUT_ALPHA_U,
      .PP_DUP_HOR_U = JMDEC_PP3_DUP_HOR_U,
      .PP_DUP_VER_U = JMDEC_PP3_DUP_VER_U,
      .DITHER_SELECT_R_U = JMDEC_PP3_DITHER_SELECT_R_U,
      .DITHER_SELECT_G_U = JMDEC_PP3_DITHER_SELECT_G_U,
      .DITHER_SELECT_B_U = JMDEC_PP3_DITHER_SELECT_B_U,
      .SCALE1_OUT_WIDTH_U = JMDEC_PP3_SCALE1_OUT_WIDTH_U,
      .SCALE1_OUT_HEIGHT_U = JMDEC_PP3_SCALE1_OUT_HEIGHT_U,
      .Y_FILTER_SIZE_U = JMDEC_PP3_Y_FILTER_SIZE_U,
      .X_FILTER_SIZE_U = JMDEC_PP3_X_FILTER_SIZE_U,
   },
   {

      .PP_OUT_E_U = JMDEC_PP4_OUT_E_U,
      .PP_OUT_TILE_E_U = JMDEC_PP4_OUT_TILE_E_U,
      .PP_OUT_MODE = JMDEC_PP4_OUT_MODE,
      .PP_CR_FIRST = JMDEC_PP4_CR_FIRST,
      .PP_OUT_RGB_FMT_U = JMDEC_PP4_OUT_RGB_FMT_U,
      .PP_RGB_PLANAR_U = JMDEC_PP4_RGB_PLANAR_U,
      .PP_OUT_SWAP_U = JMDEC_PP4_OUT_SWAP_U,
      .HOR_SCALE_MODE_U = JMDEC_PP4_HOR_SCALE_MODE_U,
      .VER_SCALE_MODE_U = JMDEC_PP4_VER_SCALE_MODE_U,
      .OUT_FORMAT_U = JMDEC_PP4_OUT_FORMAT_U,
      .SCALE_HRATIO_U = JMDEC_PP4_SCALE_HRATIO_U,
      .SCALE_WRATIO_U = JMDEC_PP4_SCALE_WRATIO_U,
      .WSCALE_INVRA_U = JMDEC_PP4_WSCALE_INVRA_U,
      .WSCALE_INVRA_EXT_U = JMDEC_PP4_WSCALE_INVRA_EXT_U,
      .HSCALE_INVRA_U = JMDEC_PP4_HSCALE_INVRA_U,
      .HCALE_INVRA_EXT_U = JMDEC_PP4_HCALE_INVRA_EXT_U,
      .PP_OUT_LU_BASE_U_MSB = JMDEC_PP4_OUT_LU_BASE_U_MSB,
      .PP_OUT_LU_BASE_U_LSB = JMDEC_PP4_OUT_LU_BASE_U_LSB,
      .PP_OUT_CH_BASE_U_MSB = JMDEC_PP4_OUT_CH_BASE_U_MSB,
      .PP_OUT_CH_BASE_U_LSB = JMDEC_PP4_OUT_CH_BASE_U_LSB,
      .PP_OUT_Y_STRIDE = JMDEC_PP4_OUT_Y_STRIDE,
      .PP_OUT_C_STRIDE = JMDEC_PP4_OUT_C_STRIDE,
      .FLIP_MODE_U = JMDEC_PP4_FLIP_MODE_U,
      .CROP_STARTX_U = JMDEC_PP4_CROP_STARTX_U,
      .ROTATION_MODE_U = JMDEC_PP4_ROTATION_MODE_U,
      .CROP_STARTY_U = JMDEC_PP4_CROP_STARTY_U,
      .PP_IN_WIDTH_U = JMDEC_PP4_IN_WIDTH_U,
      .PP_IN_HEIGHT_U = JMDEC_PP4_IN_HEIGHT_U,
      .PP_OUT_WIDTH_U = JMDEC_PP4_OUT_WIDTH_U,
      .PP_OUT_HEIGHT_U = JMDEC_PP4_OUT_HEIGHT_U,
      .PP_OUT_P010_FMT_U = JMDEC_PP4_OUT_P010_FMT_U,
      .PP_OUT_LU_BOT_BASE_U_MSB = JMDEC_PP4_OUT_LU_BOT_BASE_U_MSB,
      .PP_OUT_LU_BOT_BASE_U_LSB = JMDEC_PP4_OUT_LU_BOT_BASE_U_LSB,
      .PP_OUT_CH_BOT_BASE_U_MSB = JMDEC_PP4_OUT_CH_BOT_BASE_U_MSB,
      .PP_OUT_CH_BOT_BASE_U_LSB = JMDEC_PP4_OUT_CH_BOT_BASE_U_LSB,
      .PP_CROP2_STARTX_U = JMDEC_PP4_CROP2_STARTX_U,
      .PP_CROP2_STARTY_U = JMDEC_PP4_CROP2_STARTY_U,
      .PP_CROP2_OUT_WIDTH_U = JMDEC_PP4_CROP2_OUT_WIDTH_U,
      .PP_CROP2_OUT_HEIGHT_U = JMDEC_PP4_CROP2_OUT_HEIGHT_U,
      .PP_OUT_R_BASE_U_MSB = JMDEC_PP4_OUT_R_BASE_U_MSB,
      .PP_OUT_R_BASE_U_LSB = JMDEC_PP4_OUT_R_BASE_U_LSB,
      .PP_OUT_G_BASE_U_MSB = JMDEC_PP4_OUT_G_BASE_U_MSB,
      .PP_OUT_G_BASE_U_LSB = JMDEC_PP4_OUT_G_BASE_U_LSB,
      .PP_OUT_B_BASE_U_MSB = JMDEC_PP4_OUT_B_BASE_U_MSB,
      .PP_OUT_B_BASE_U_LSB = JMDEC_PP4_OUT_B_BASE_U_LSB,
      .PP_OUT_LANCZOS_TBL_BASE_U_MSB = JMDEC_PP4_LANCZOS_TBL_BASE_U_MSB,
      .PP_OUT_LANCZOS_TBL_BASE_U_LSB = JMDEC_PP4_LANCZOS_TBL_BASE_U_LSB,
      .DITHER_SELECT_R_U = JMDEC_PP4_DITHER_SELECT_R_U,
      .DITHER_SELECT_G_U = JMDEC_PP4_DITHER_SELECT_G_U,
      .DITHER_SELECT_B_U = JMDEC_PP4_DITHER_SELECT_B_U,
      .Y_FILTER_SIZE_U = JMDEC_PP4_Y_FILTER_SIZE_U,
      .X_FILTER_SIZE_U = JMDEC_PP4_X_FILTER_SIZE_U,
   }};

static const uint32_t coeff[6][5] = {{16384, 22970, 11700, 5638, 29032},
                                {19077, 26149, 13320, 6419, 33050},
                                {16384, 25802, 7670, 3069, 30402},
                                {19077, 29372, 8731, 3494, 34610},
                                {16384, 24160, 9361, 2696, 30825},
                                {19077, 27504, 10646, 3069, 35091}};

static void
CalOutTableIndex(uint32_t table_in_index,
                 uint32_t coeff_in_index,
                 uint32_t filter_size,
                 uint32_t *table_out_index,
                 uint32_t *coeff_out_index)
{
   if (table_in_index <= 16) {
      *table_out_index = table_in_index;
      *coeff_out_index = coeff_in_index;
   } else {
      *table_out_index = 32 - table_in_index;
      *coeff_out_index = filter_size - 1 - coeff_in_index;
   }
}

static double
getSplineCoeff(double a, double b, double c, double d, double dist)
{
   if (dist <= 1.0)
      return ((d * dist + c) * dist + b) * dist + a;
   else
      return getSplineCoeff(0.0,
                            b + 2.0 * c + 3.0 * d,
                            c + 3.0 * d,
                            -b - 3.0 * c - 6.0 * d,
                            dist - 1.0);
}

static void
InitPpUnitLanczosData(struct mwv207_video_dec_hw_features *hw_feature,
                      mwv207_pp_unit_int_config *ppu_cfg)
{
   uint32_t i = 0, j, m;
   int32_t k;
   int32_t xDstInSrc, yDstInSrc;
   int32_t scale_ratio_x_inv, scale_ratio_y_inv;
   int32_t start_pos, end_pos, table_index;
   int32_t pos, x_pos, y_pos;
   uint32_t start_count, end_count;
   uint32_t out_index, coeff_index;
   int32_t value;
   for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (i >= hw_feature->max_ppu_count)
         ppu_cfg->enabled = 0;
      if (!ppu_cfg->enabled)
         continue;
      if (((ppu_cfg->crop.width < ppu_cfg->scale.width
            || ppu_cfg->crop.height < ppu_cfg->scale.height)
           && hw_feature->pp_up_lanczos[i])
          || ((ppu_cfg->crop.width >= ppu_cfg->scale.width)
              && (ppu_cfg->crop.height >= ppu_cfg->scale.height)
              && hw_feature->pp_down_lanczos[i]))
         ppu_cfg->pp_lanczos = 1;
      else
         ppu_cfg->pp_lanczos = 0;
      if (ppu_cfg->pp_lanczos && ppu_cfg->lanczos_table->virtual_addr) {
         uint32_t x_filter_size, y_filter_size;
         uint32_t base_x_filter_size, base_y_filter_size;
         uint32_t x_filter_stride, y_filter_stride;
         uint32_t x_coeff_base, y_coeff_base;
         if (ppu_cfg->crop.width > ppu_cfg->scale.width) {
            if (ppu_cfg->pp_filter == PP_JM_LINEAR) {
               x_filter_size =
                  2 * ((ppu_cfg->crop.width) / ppu_cfg->scale.width);
            } else if (ppu_cfg->pp_filter == PP_LANCZOS
                       || ppu_cfg->pp_filter == PP_BILINEAR
                       || ppu_cfg->pp_filter == PP_BICUBIC
                       || ppu_cfg->pp_filter == PP_SPLINE) {
               x_filter_size =
                  (2 * ppu_cfg->x_filter_param * ppu_cfg->crop.width
                   + ppu_cfg->scale.width - 1)
                  / ppu_cfg->scale.width;
            } else if (ppu_cfg->pp_filter == PP_BOX) {
               x_filter_size =
                  ((ppu_cfg->x_filter_param - 1) * ppu_cfg->crop.width
                   + ppu_cfg->scale.width - 1)
                  / ppu_cfg->scale.width;
            } else {
               x_filter_size = 1;
            }
         } else if (ppu_cfg->crop.width < ppu_cfg->scale.width) {
            x_filter_size = (LAN_SIZE_HOR - 1);
         } else {
            x_filter_size = 0;
         }
         if (ppu_cfg->crop.height > ppu_cfg->scale.height) {
            if (ppu_cfg->pp_filter == PP_JM_LINEAR) {
               y_filter_size =
                  2 * ((ppu_cfg->crop.height) / ppu_cfg->scale.height);
            } else if (ppu_cfg->pp_filter == PP_LANCZOS
                       || ppu_cfg->pp_filter == PP_BILINEAR
                       || ppu_cfg->pp_filter == PP_BICUBIC
                       || ppu_cfg->pp_filter == PP_SPLINE) {
               y_filter_size =
                  (2 * ppu_cfg->y_filter_param * ppu_cfg->crop.height
                   + ppu_cfg->scale.height - 1)
                  / ppu_cfg->scale.height;
            } else if (ppu_cfg->pp_filter == PP_BOX) {
               y_filter_size =
                  ((ppu_cfg->y_filter_param - 1) * ppu_cfg->crop.height
                   + ppu_cfg->scale.height - 1)
                  / ppu_cfg->scale.height;
            } else {
               y_filter_size = 1;
            }
         } else if (ppu_cfg->crop.height < ppu_cfg->scale.height) {
            y_filter_size = (LAN_SIZE_VER - 1);
         } else {
            y_filter_size = 0;
         }
         if (ppu_cfg->crop.width != ppu_cfg->scale.width)
            x_filter_size = 1 + x_filter_size / 2 * 2;
         else
            x_filter_size = 0;
         if (ppu_cfg->crop.height != ppu_cfg->scale.height)
            y_filter_size = 1 + y_filter_size / 2 * 2;
         else
            y_filter_size = 0;
         if (x_filter_size
             > MIN(ppu_cfg->crop.width - 2, MAX_COEFF_SIZE_HOR_REAL))
            x_filter_size =
               MIN(ppu_cfg->crop.width - 2, MAX_COEFF_SIZE_HOR_REAL);
         if (y_filter_size
             > MIN(ppu_cfg->crop.height - 2, MAX_COEFF_SIZE_VER_REAL))
            y_filter_size =
               MIN(ppu_cfg->crop.height - 2, MAX_COEFF_SIZE_VER_REAL);

         if (ppu_cfg->crop.width > ppu_cfg->scale.width)
            base_x_filter_size = ((LAN_SIZE_HOR - 1) * ppu_cfg->crop.width
                                  + ppu_cfg->scale.width - 1)
               / ppu_cfg->scale.width;
         else if (ppu_cfg->crop.width < ppu_cfg->scale.width)
            base_x_filter_size = (LAN_SIZE_HOR - 1);
         else
            base_x_filter_size = 0;
         if (ppu_cfg->crop.height > ppu_cfg->scale.height)
            base_y_filter_size = ((LAN_SIZE_VER - 1) * ppu_cfg->crop.height
                                  + ppu_cfg->scale.height - 1)
               / ppu_cfg->scale.height;
         else if (ppu_cfg->crop.height < ppu_cfg->scale.height)
            base_y_filter_size = (LAN_SIZE_VER - 1);
         else
            base_y_filter_size = 0;
         if (ppu_cfg->crop.width != ppu_cfg->scale.width)
            base_x_filter_size = 1 + base_x_filter_size / 2 * 2;
         else
            base_x_filter_size = 0;
         if (ppu_cfg->crop.height != ppu_cfg->scale.height)
            base_y_filter_size = 1 + base_y_filter_size / 2 * 2;
         else
            base_y_filter_size = 0;
         if (base_x_filter_size
             > MIN(ppu_cfg->crop.width - 2, MAX_COEFF_SIZE_HOR_REAL))
            base_x_filter_size =
               MIN(ppu_cfg->crop.width - 2, MAX_COEFF_SIZE_HOR_REAL);
         if (base_y_filter_size
             > MIN(ppu_cfg->crop.height - 2, MAX_COEFF_SIZE_VER_REAL))
            base_y_filter_size =
               MIN(ppu_cfg->crop.height - 2, MAX_COEFF_SIZE_VER_REAL);

#ifdef SUPPORT_FLEXIBLE_FILTER
         ppu_cfg->x_filter_size = x_filter_size;
         ppu_cfg->y_filter_size = y_filter_size;
         x_filter_stride = ((x_filter_size + 3) / 4) * 4;
         y_filter_stride = ((y_filter_size + 3) / 4) * 4;
         x_coeff_base = y_coeff_base = 0;
#else
         ppu_cfg->x_filter_size = base_x_filter_size;
         ppu_cfg->y_filter_size = base_y_filter_size;
         x_filter_stride = ((base_x_filter_size + 3) / 4) * 4;
         y_filter_stride = ((base_y_filter_size + 3) / 4) * 4;
         x_coeff_base = base_x_filter_size / 2 - x_filter_size / 2;
         y_coeff_base = base_y_filter_size / 2 - y_filter_size / 2;
#endif
         double ratio = 0;
         double distance = ppu_cfg->x_filter_param;
         int32_t horizontal_weight[MAX_COEFF_SIZE_HOR] = {0};
         int32_t verizontal_weight[MAX_COEFF_SIZE_VER] = {0};
         int32_t *table_data = (int32_t *)ppu_cfg->lanczos_table->virtual_addr;
         if (ppu_cfg->scale.width != ppu_cfg->crop.width) {
            if (ppu_cfg->scale.width > ppu_cfg->crop.width)
               ratio = 1.0;
            else
               ratio = (double)ppu_cfg->scale.width / ppu_cfg->crop.width;
            for (k = 0; k <= TABLE_SIZE / 2; k++) {
               for (m = 0; m < x_filter_size; m++) {
                  if (ppu_cfg->pp_filter == PP_NEAREST)
                     horizontal_weight[m + x_coeff_base] = 65535;
                  else if (ppu_cfg->pp_filter == PP_JM_LINEAR) {
                     if (((x_filter_size / 2) % 2) == 1) {
                        if (m
                               == (x_filter_size / 2
                                   - (x_filter_size / 2 + 1) / 2)
                            || m
                               == (x_filter_size / 2
                                   + (x_filter_size / 2 + 1) / 2)) {
                           horizontal_weight[m + x_coeff_base] =
                              (65536
                               - (int32_t)((double)ppu_cfg->scale.width
                                       / ppu_cfg->crop.width * 65536)
                                  * (x_filter_size / 2))
                              / 2;
                        } else if ((m > (x_filter_size / 2
                                         - (x_filter_size / 2 + 1) / 2))
                                   && (m < (x_filter_size / 2
                                            + (x_filter_size / 2 + 1) / 2))) {
                           horizontal_weight[m + x_coeff_base] =
                              (int32_t)((double)ppu_cfg->scale.width
                                    / ppu_cfg->crop.width * 65536);
                        } else {
                           horizontal_weight[m + x_coeff_base] = 0;
                        }
                     } else {
                        if (m >= x_filter_size / 4
                            && m < 3 * x_filter_size / 4)
                           horizontal_weight[m + x_coeff_base] =
                              (int32_t)((double)ppu_cfg->scale.width
                                    / ppu_cfg->crop.width * 65536);
                        else if (m == 3 * x_filter_size / 4)
                           horizontal_weight[m + x_coeff_base] = 65536
                              - (int32_t)((double)ppu_cfg->scale.width
                                      / ppu_cfg->crop.width * 65536)
                                 * (x_filter_size / 2);
                        else
                           horizontal_weight[m + x_coeff_base] = 0;
                     }
                  } else {
                     double value = (double)(m)-x_filter_size / 2
                        - ((double)k - TABLE_SIZE / 2) / TABLE_SIZE;
                     if (value < 0)
                        value *= -1.0;
                     else
                        value *= 1.0;
                     double d = value * ratio;
                     if (ppu_cfg->pp_filter == PP_LANCZOS) {
                        if (d == 0) {
                           horizontal_weight[m + x_coeff_base] = 65535;
                        } else if (d > distance || d < -distance) {
                           horizontal_weight[m + x_coeff_base] = 0;
                        } else {
                           horizontal_weight[m + x_coeff_base] = (int32_t)(
                              (distance * sin(PI * d) * sin(PI * d / distance)
                               / (PI * PI * d * d))
                              * 65536);
                        }
                     } else if (ppu_cfg->pp_filter == PP_BILINEAR) {
                        if (d >= 1)
                           horizontal_weight[m + x_coeff_base] = 0;
                        else
                           horizontal_weight[m + x_coeff_base] =
                              (int32_t)((1 - d) * 65536);
                     } else if (ppu_cfg->pp_filter == PP_BICUBIC) {
                        if (d >= 2)
                           horizontal_weight[m + x_coeff_base] = 0;
                        else if (d < 1)
                           horizontal_weight[m + x_coeff_base] = (int32_t)(
                              (3 * d * d * d - 5 * d * d + 2) / 2 * 65536);
                        else
                           horizontal_weight[m + x_coeff_base] =
                              (int32_t)((5 * d * d - d * d * d - 8 * d + 4) / 2
                                    * 65536);
                     } else if (ppu_cfg->pp_filter == PP_SPLINE) {
                        double p = -2.196152422706632;
                        horizontal_weight[m + x_coeff_base] = (int32_t)(
                           getSplineCoeff(1.0, 0.0, p, -p - 1.0, d) * 65536);
                     } else if (ppu_cfg->pp_filter == PP_BOX) {
                        horizontal_weight[m + x_coeff_base] =
                           (int32_t)(65536.0 / x_filter_size);
                     } else {
                        printf("Not Supported\n");
                     }
                  }
               }
               int32_t total = 0;
               for (m = 0; m < ppu_cfg->x_filter_size; m++) {
                  total += horizontal_weight[m];
               }
               for (m = 0; m < ppu_cfg->x_filter_size; m++) {
                  *table_data++ =
                     (int32_t)((double)horizontal_weight[m] / total * 65536);
               }
               for (m = 0; m < x_filter_stride - ppu_cfg->x_filter_size;
                    m++) {
                  *table_data++ = 0;
               }
            }
         }
         distance = ppu_cfg->y_filter_param;
         if (ppu_cfg->scale.height != ppu_cfg->crop.height) {
            if (ppu_cfg->scale.height > ppu_cfg->crop.height)
               ratio = 1.0;
            else
               ratio = (double)ppu_cfg->scale.height / ppu_cfg->crop.height;
            for (k = 0; k <= TABLE_SIZE / 2; k++) {
               for (m = 0; m < y_filter_size; m++) {
                  if (ppu_cfg->pp_filter == PP_NEAREST)
                     verizontal_weight[m + y_coeff_base] = 65535;
                  else if (ppu_cfg->pp_filter == PP_JM_LINEAR) {
                     if (((y_filter_size / 2) % 2) == 1) {
                        if (m
                               == (y_filter_size / 2
                                   - (y_filter_size / 2 + 1) / 2)
                            || m
                               == (y_filter_size / 2
                                   + (y_filter_size / 2 + 1) / 2)) {
                           verizontal_weight[m + y_coeff_base] =
                              (65536
                               - (int32_t)((double)ppu_cfg->scale.height
                                       / ppu_cfg->crop.height * 65536)
                                  * (y_filter_size / 2))
                              / 2;
                        } else if ((m > (y_filter_size / 2
                                         - (y_filter_size / 2 + 1) / 2))
                                   && (m < (y_filter_size / 2
                                            + (y_filter_size / 2 + 1) / 2))) {
                           verizontal_weight[m + y_coeff_base] =
                              (int32_t)((double)ppu_cfg->scale.height
                                    / ppu_cfg->crop.height * 65536);
                        } else {
                           verizontal_weight[m + y_coeff_base] = 0;
                        }
                     } else {
                        if (m >= y_filter_size / 4
                            && m < 3 * y_filter_size / 4)
                           verizontal_weight[m + y_coeff_base] =
                              (int32_t)((double)ppu_cfg->scale.height
                                    / ppu_cfg->crop.height * 65536);
                        else if (m == 3 * y_filter_size / 4)
                           verizontal_weight[m + y_coeff_base] = 65536
                              - (int32_t)((double)ppu_cfg->scale.height
                                      / ppu_cfg->crop.height * 65536)
                                 * (y_filter_size / 2);
                        else
                           verizontal_weight[m + y_coeff_base] = 0;
                     }
                  } else {
                     double value = (double)(m)-y_filter_size / 2
                        - ((double)k - TABLE_SIZE / 2) / TABLE_SIZE;
                     if (value < 0)
                        value *= -1.0;
                     else
                        value *= 1.0;
                     double d = value * ratio;
                     if (ppu_cfg->pp_filter == PP_LANCZOS) {
                        if (d == 0) {
                           verizontal_weight[m + y_coeff_base] = 65535;
                        } else if (d > distance || d < -distance) {
                           verizontal_weight[m + y_coeff_base] = 0;
                        } else {
                           verizontal_weight[m + y_coeff_base] = (int32_t)(
                              (distance * sin(PI * d) * sin(PI * d / distance)
                               / (PI * PI * d * d))
                              * 65536);
                        }
                     } else if (ppu_cfg->pp_filter == PP_BILINEAR) {
                        if (d >= 1)
                           verizontal_weight[m + y_coeff_base] = 0;
                        else
                           verizontal_weight[m + y_coeff_base] =
                              (int32_t)((1 - d) * 65536);
                     } else if (ppu_cfg->pp_filter == PP_BICUBIC) {
                        if (d >= 2)
                           verizontal_weight[m + y_coeff_base] = 0;
                        else if (d < 1)
                           verizontal_weight[m + y_coeff_base] = (int32_t)(
                              (3 * d * d * d - 5 * d * d + 2) / 2 * 65536);
                        else
                           verizontal_weight[m + y_coeff_base] =
                              (int32_t)((5 * d * d - d * d * d - 8 * d + 4) / 2
                                    * 65536);
                     } else if (ppu_cfg->pp_filter == PP_SPLINE) {
                        double p = -2.196152422706632;
                        verizontal_weight[m + y_coeff_base] = (int32_t)(
                           getSplineCoeff(1.0, 0.0, p, -p - 1.0, d) * 65536);
                     } else if (ppu_cfg->pp_filter == PP_BOX) {
                        verizontal_weight[m + y_coeff_base] =
                           (int32_t)(65536.0 / y_filter_size);
                     } else {
                        printf("Not Supported\n");
                     }
                  }
               }
               int32_t total = 0;
               for (m = 0; m < ppu_cfg->y_filter_size; m++) {
                  total += verizontal_weight[m];
               }
               for (m = 0; m < ppu_cfg->y_filter_size; m++) {
                  *table_data++ =
                     (int32_t)((double)verizontal_weight[m] / total * 65536);
               }
               for (m = 0; m < y_filter_stride - ppu_cfg->y_filter_size;
                    m++) {
                  *table_data++ = 0;
               }
            }
         }
         scale_ratio_x_inv =
            FDIVI(TOFIX(ppu_cfg->crop.width, 16) + ppu_cfg->scale.width / 2,
                  ppu_cfg->scale.width);
         scale_ratio_y_inv =
            FDIVI(TOFIX(ppu_cfg->crop.height, 16) + ppu_cfg->scale.height / 2,
                  ppu_cfg->scale.height);
         xDstInSrc = scale_ratio_x_inv - 65536;
         pos = xDstInSrc / 2 - scale_ratio_x_inv;
         x_pos = 0;
         start_count = 0;
         end_count = 0;
         int32_t start_value[4] = {0, 0, 0, 0};
         int32_t end_value[4] = {0, 0, 0, 0};
         if (ppu_cfg->crop.width != ppu_cfg->scale.width) {
            for (j = 0; j < ppu_cfg->scale.width; j++) {
               pos += scale_ratio_x_inv;
               x_pos = FLOOR(pos + 32768);
               start_pos = x_pos - x_filter_size / 2;
               end_pos = x_pos + x_filter_size / 2;
               table_index = ((pos + 32768 - MAKE_FIXED_UNI(x_pos))
                              >> (FIXED_BITS_UNI - TABLE_LENGTH));
               value = 0;
               if (start_pos < 0) {
                  for (k = start_pos; k <= 0; k++) {
                     CalOutTableIndex(table_index,
                                      k - start_pos,
                                      x_filter_size,
                                      &out_index,
                                      &coeff_index);
                     value += *((int32_t *)ppu_cfg->lanczos_table->virtual_addr
                                + out_index * x_filter_stride + coeff_index);
                  }
                  start_value[start_count++] = value;
               } else {
                  if (start_count < 4) {
                     CalOutTableIndex(table_index,
                                      0,
                                      x_filter_size,
                                      &out_index,
                                      &coeff_index);
                     value = *((int32_t *)ppu_cfg->lanczos_table->virtual_addr
                               + out_index * x_filter_stride + coeff_index);
                     start_value[start_count++] = value;
                  }
               }
               value = 0;
               if (end_pos >= ppu_cfg->crop.width) {
                  for (k = ppu_cfg->crop.width - 1; k <= end_pos; k++) {
                     CalOutTableIndex(table_index,
                                      k - start_pos,
                                      x_filter_size,
                                      &out_index,
                                      &coeff_index);
                     value += *((int32_t *)ppu_cfg->lanczos_table->virtual_addr
                                + out_index * x_filter_stride + coeff_index);
                  }
                  end_value[end_count++] = value;
               }
            }
            for (j = 0; j < 4; j++) {
               *table_data++ = start_value[j];
               start_value[j] = 0;
            }
            for (j = 0; j < 4; j++) {
               *table_data++ = end_value[j];
               end_value[j] = 0;
            }
         }
         yDstInSrc = scale_ratio_y_inv - 65536;
         pos = yDstInSrc / 2 - scale_ratio_y_inv;
         y_pos = 0;
         start_count = 0;
         end_count = 0;
         if (ppu_cfg->scale.height != ppu_cfg->crop.height) {
            for (j = 0; j < ppu_cfg->scale.height; j++) {
               pos += scale_ratio_y_inv;
               y_pos = FLOOR(pos + 32768);
               start_pos = y_pos - y_filter_size / 2;
               end_pos = y_pos + y_filter_size / 2;
               table_index = ((pos + 32768 - MAKE_FIXED_UNI(y_pos))
                              >> (FIXED_BITS_UNI - TABLE_LENGTH));
               value = 0;
               if (start_pos < 0) {
                  for (k = start_pos; k <= 0; k++) {
                     CalOutTableIndex(table_index,
                                      k - start_pos,
                                      y_filter_size,
                                      &out_index,
                                      &coeff_index);
                     value += *((int32_t *)ppu_cfg->lanczos_table->virtual_addr
                                + (TABLE_SIZE / 2 + 1) * x_filter_stride
                                + out_index * y_filter_stride + coeff_index);
                  }
                  start_value[start_count++] = value;
               } else {
                  if (start_count < 3) {
                     CalOutTableIndex(table_index,
                                      0,
                                      y_filter_size,
                                      &out_index,
                                      &coeff_index);
                     value = *((int32_t *)ppu_cfg->lanczos_table->virtual_addr
                               + (TABLE_SIZE / 2 + 1) * x_filter_stride
                               + out_index * y_filter_stride + coeff_index);
                     start_value[start_count++] = value;
                  }
               }
               value = 0;
               if (end_pos >= ppu_cfg->crop.height) {
                  for (k = ppu_cfg->crop.height - 1; k <= end_pos; k++) {
                     CalOutTableIndex(table_index,
                                      k - start_pos,
                                      y_filter_size,
                                      &out_index,
                                      &coeff_index);
                     value += *((int32_t *)ppu_cfg->lanczos_table->virtual_addr
                                + (TABLE_SIZE / 2 + 1) * x_filter_stride
                                + out_index * y_filter_stride + coeff_index);
                  }
                  end_value[end_count++] = value;
               }
            }
            for (j = 0; j < 4; j++)
               *table_data++ = start_value[j];
            for (j = 0; j < 4; j++)
               *table_data++ = end_value[j];
         }

         xDstInSrc = scale_ratio_x_inv - 65536;
         pos = xDstInSrc / 2 - scale_ratio_x_inv;
         x_pos = 0;
         end_count = 0;
         int32_t chroma_end_value[4] = {0, 0, 0, 0};
         if (ppu_cfg->crop.width != ppu_cfg->scale.width) {
            for (j = 0; j < ppu_cfg->scale.width / 2; j++) {
               pos += scale_ratio_x_inv;
               x_pos = FLOOR(pos + 32768);
               start_pos = x_pos - x_filter_size / 2;
               end_pos = x_pos + x_filter_size / 2;
               table_index = ((pos + 32768 - MAKE_FIXED_UNI(x_pos))
                              >> (FIXED_BITS_UNI - TABLE_LENGTH));
               value = 0;
               if (end_pos >= ppu_cfg->crop.width / 2) {
                  for (k = ppu_cfg->crop.width / 2 - 1; k <= end_pos; k++) {
                     CalOutTableIndex(table_index,
                                      k - start_pos,
                                      x_filter_size,
                                      &out_index,
                                      &coeff_index);
                     value += *((int32_t *)ppu_cfg->lanczos_table->virtual_addr
                                + out_index * x_filter_stride + coeff_index);
                  }
                  chroma_end_value[end_count++] = value;
               }
            }
            for (j = 0; j < 4; j++) {
               *table_data++ = chroma_end_value[j];
               chroma_end_value[j] = 0;
            }
         }
         yDstInSrc = scale_ratio_y_inv - 65536;
         pos = yDstInSrc / 2 - scale_ratio_y_inv;
         y_pos = 0;
         end_count = 0;
         if (ppu_cfg->scale.height != ppu_cfg->crop.height) {
            for (j = 0; j < ppu_cfg->scale.height / 2; j++) {
               pos += scale_ratio_y_inv;
               y_pos = FLOOR(pos + 32768);
               start_pos = y_pos - y_filter_size / 2;
               end_pos = y_pos + y_filter_size / 2;
               table_index = ((pos + 32768 - MAKE_FIXED_UNI(y_pos))
                              >> (FIXED_BITS_UNI - TABLE_LENGTH));
               value = 0;
               if (end_pos >= ppu_cfg->crop.height / 2) {
                  for (k = ppu_cfg->crop.height / 2 - 1; k <= end_pos; k++) {
                     CalOutTableIndex(table_index,
                                      k - start_pos,
                                      y_filter_size,
                                      &out_index,
                                      &coeff_index);
                     value += *((int32_t *)ppu_cfg->lanczos_table->virtual_addr
                                + (TABLE_SIZE / 2 + 1) * x_filter_stride
                                + out_index * y_filter_stride + coeff_index);
                  }
                  chroma_end_value[end_count++] = value;
               }
            }
            for (j = 0; j < 4; j++)
               *table_data++ = chroma_end_value[j];
         }
      }
   }
}

void
mwv207_cal_second_up_scale_ratio(mwv207_pp_unit_int_config *ppu_cfg,
                                uint32_t interlace)
{
   uint32_t i = 0;
   uint32_t found = 0;
   if (ppu_cfg->scale.width > 3 * ppu_cfg->crop.width) {
      for (i = 3 * ppu_cfg->crop.width; i > ppu_cfg->crop.width; i -= 2) {
         if ((ppu_cfg->scale.width % i) == 0) {
            ppu_cfg->out_width = i;
            ppu_cfg->out_ratio_x = ppu_cfg->scale.width / i;
            found = 1;
            break;
         }
      }

      if (!found) {
         for (i = 3 * ppu_cfg->crop.width; i > ppu_cfg->crop.width; i -= 2) {
            if ((ppu_cfg->scale.width / i)
                != (ppu_cfg->scale.width / (i - 2))) {
               ppu_cfg->out_width = i;
               ppu_cfg->out_ratio_x = ppu_cfg->scale.width / (i - 2);
               break;
            }
         }
      }
   } else {
      ppu_cfg->out_width = ppu_cfg->scale.width;
      ppu_cfg->out_ratio_x = 1;
   }
   found = 0;
   if (ppu_cfg->scale.height > 3 * ppu_cfg->crop.height) {
      for (i = 3 * ppu_cfg->crop.height; i > ppu_cfg->crop.height;
           i -= (interlace ? 4 : 2)) {
         if ((ppu_cfg->scale.height % i) == 0) {
            ppu_cfg->out_height = i;
            ppu_cfg->out_ratio_y = ppu_cfg->scale.height / i;
            found = 1;
            break;
         }
      }
      if (!found) {
         for (i = 3 * ppu_cfg->crop.height; i > ppu_cfg->crop.height;
              i -= (interlace ? 4 : 2)) {
            if ((ppu_cfg->scale.height / i)
                != (ppu_cfg->scale.height / (i - 2))) {
               ppu_cfg->out_height = i;
               ppu_cfg->out_ratio_y =
                  ppu_cfg->scale.height / (i - (interlace ? 4 : 2));
               break;
            }
         }
      }
   } else {
      ppu_cfg->out_height = ppu_cfg->scale.height;
      ppu_cfg->out_ratio_y = 1;
   }
}

uint32_t
mwv207_check_pp_unit_config(struct mwv207_video_dec_hw_features *hw_feature,
                           uint32_t in_width,
                           uint32_t in_height,
                           uint32_t interlace,
                           mwv207_pp_unit_int_config *ppu_cfg)
{
   uint32_t i = 0;
   uint32_t pp_width = 0, pp_height = 0, ystride = 0, cstride = 0, hstride = 0;
   mwv207_pp_unit_int_config *ppu_cfg_temp = ppu_cfg;
   uint32_t crop_align = (1 << hw_feature->crop_step_rshift) - 1;
   for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (i >= hw_feature->max_ppu_count)
         ppu_cfg->enabled = 0;
      if (!ppu_cfg->enabled)
         continue;

      if (in_width == 0 && in_height == 0)
         return 0;

      if (!hw_feature->pp_planar_support && ppu_cfg->planar) {
         MWV207_LOG(MWV207_LEVEL_ERROR, "PLANAR output is not supuported.\n");
         return 1;
      }

      if (!hw_feature->pp_yuv420_101010_support && ppu_cfg->out_1010) {
         MWV207_LOG(MWV207_LEVEL_ERROR, "101010 output is not supuported.\n");
         return 1;
      }
      if (!hw_feature->fmt_p010_support
          && (ppu_cfg->out_p010 || ppu_cfg->out_I010 || ppu_cfg->out_L010)) {
         MWV207_LOG(MWV207_LEVEL_ERROR, "P010 output is not supuported.\n");
         return 1;
      }
      if (!hw_feature->second_crop_support && ppu_cfg->crop2.enabled) {
         MWV207_LOG(MWV207_LEVEL_ERROR, "second crop  is not supuported.\n");
         return 1;
      }

#if 0
        if (!hw_feature->fmt_rgb_support && (ppu_cfg->rgb || ppu_cfg->rgb_planar)) {
            ERROR_PRINT("RGB is not supported.\n");
            return 1;
        }
#endif
      if (ppu_cfg->planar && ppu_cfg->monochrome) {
         ppu_cfg->planar = 0;
      }

      if (!ppu_cfg->crop.width || !ppu_cfg->crop.height) {
         ppu_cfg->crop.enabled = 0;
      }
      if (!ppu_cfg->crop.enabled) {
         ppu_cfg->crop.x = ppu_cfg->crop.y = 0;
         ppu_cfg->crop.width = in_width / 2 * 2;
         ppu_cfg->crop.height = in_height / 2 * 2;
         ppu_cfg->crop.set_by_user = 0;
         ppu_cfg->crop.enabled = 1;
      }

      if (!ppu_cfg->scale.width || !ppu_cfg->scale.height) {
         ppu_cfg->scale.enabled = 0;
      }
      if (!ppu_cfg->scale.enabled) {
         ppu_cfg->scale.width = ppu_cfg->crop.width;
         ppu_cfg->scale.height = ppu_cfg->crop.height;
         ppu_cfg->scale.ratio_x = 0;
         ppu_cfg->scale.ratio_y = 0;
         ppu_cfg->scale.x = 0;
         ppu_cfg->scale.y = 0;
         ppu_cfg->scale.set_by_user = 0;
         ppu_cfg->scale.enabled = 1;
      }

      if (ppu_cfg->tiled_e) {
         pp_width = NEXT_MULTIPLE(ppu_cfg->scale.width, 4);
         ystride = cstride =
            NEXT_MULTIPLE(4 * pp_width * ppu_cfg->pixel_width,
                          ALIGN(ppu_cfg->align) * 8)
            / 8;
         hstride = NEXT_MULTIPLE(ppu_cfg->scale.height, 4) / 4;
      } else if (ppu_cfg->out_1010) {
         pp_width = ((ppu_cfg->scale.width + 2) / 3) * 3;
         ystride = cstride =
            NEXT_MULTIPLE(4 * pp_width / 3, ALIGN(ppu_cfg->align));
      } else {
         if (ppu_cfg->rgb) {
            pp_width = ppu_cfg->scale.width * 4;
         } else {
            pp_width = ppu_cfg->scale.width;
         }

         pp_height = ppu_cfg->scale.height;
         ystride = NEXT_MULTIPLE(pp_width * ppu_cfg->pixel_width,
                                 ALIGN(ppu_cfg->align) * 8)
            / 8;
         hstride = NEXT_MULTIPLE(
            pp_height,
            ALIGN(DEC_ALIGN_64B)); 
         if (ppu_cfg->planar)
            cstride = NEXT_MULTIPLE(pp_width / 2 * ppu_cfg->pixel_width,
                                    ALIGN(ppu_cfg->align) * 8)
               / 8;
         else
            cstride = ystride;
      }
      if (ppu_cfg->ystride == 0) {
         ppu_cfg->ystride = ystride;
         ppu_cfg->false_ystride = 1;
      } else {
         if (ppu_cfg->false_ystride)
            ppu_cfg->ystride = ystride;
         else {
            if (ppu_cfg->ystride < ystride) {
               return 1;
            }
         }
      }

      if (ppu_cfg->hstride == 0) {
         ppu_cfg->hstride = hstride;
      }

      if (ppu_cfg->cstride == 0) {
         ppu_cfg->cstride = cstride;
         ppu_cfg->false_cstride = 1;
      } else {
         if (ppu_cfg->false_cstride)
            ppu_cfg->cstride = cstride;
         else {
            if (ppu_cfg->cstride < cstride)
               return 1;
         }
      }

      if ((ppu_cfg->crop.x & crop_align) || (ppu_cfg->crop.y & crop_align)
          || (ppu_cfg->crop.width & crop_align)
          || (ppu_cfg->crop.height & crop_align)
          || (ppu_cfg->crop.width < PP_CROP_MIN_WIDTH
              && ppu_cfg->crop.width != in_width)
          || (ppu_cfg->crop.height < PP_CROP_MIN_HEIGHT
              && ppu_cfg->crop.height != in_height)
          || (ppu_cfg->crop.x + ppu_cfg->crop.width > in_width)
          || (ppu_cfg->crop.y + ppu_cfg->crop.height > in_height)) {
         MWV207_LOG(MWV207_LEVEL_ERROR, "Illegal cropping parameters.\n");
         return 1;
      }
      if ((ppu_cfg->crop2.x & crop_align) || (ppu_cfg->crop2.y & crop_align)
          || (ppu_cfg->crop2.width & crop_align)
          || (ppu_cfg->crop2.height & crop_align)
          || (ppu_cfg->crop2.x + ppu_cfg->crop2.width > ppu_cfg->scale.width)
          || (ppu_cfg->crop2.y + ppu_cfg->crop2.height
              > ppu_cfg->scale.height)
          || (ppu_cfg->crop2.enabled
              && ((ppu_cfg->crop2.width < 48)
                  || (ppu_cfg->scale.height < 48)))) {
         MWV207_LOG(MWV207_LEVEL_ERROR, "Illegal cropping parameters.\n");
         return 1;
      }

      if (ppu_cfg->crop.width <= hw_feature->max_pp_out_pic_width[i]
          && ppu_cfg->crop.height <= hw_feature->max_pp_out_pic_height[i]) {
         if (!ppu_cfg->scale.width || !ppu_cfg->scale.height
             || (!hw_feature->uscale_support[i]
                 && (ppu_cfg->scale.width > ppu_cfg->crop.width
                     || ppu_cfg->scale.height > ppu_cfg->crop.height))
             || (((ppu_cfg->scale.width > ppu_cfg->crop.width)
                  && ((ppu_cfg->crop.x & 3) || (ppu_cfg->crop.width & 3)))
                 || ((ppu_cfg->scale.height > ppu_cfg->crop.height)
                     && ((ppu_cfg->crop.y & 3)
                         || (ppu_cfg->crop.height & 3))))
             || (!hw_feature->dscale_support[i]
                 && (ppu_cfg->scale.width < ppu_cfg->crop.width
                     || ppu_cfg->scale.height < ppu_cfg->crop.height))
             || (!hw_feature->uscale_support_big_3[i]
                 && (ppu_cfg->scale.width
                     > MIN(hw_feature->max_pp_out_pic_width[i],
                           3 * ppu_cfg->crop.width)))
             || (!hw_feature->uscale_support_big_3[i]
                 && (ppu_cfg->scale.height
                     > MIN(hw_feature->max_pp_out_pic_height[i],
                           3 * ppu_cfg->crop.height)))
             || (ppu_cfg->scale.width & 1)
             || (ppu_cfg->scale.height & (interlace ? 3 : 1))
             || (ppu_cfg->scale.width > ppu_cfg->crop.width
                 && ppu_cfg->scale.height < ppu_cfg->crop.height)
             || (ppu_cfg->scale.width < ppu_cfg->crop.width
                 && ppu_cfg->scale.height > ppu_cfg->crop.height)) {
            MWV207_LOG(MWV207_LEVEL_ERROR,
                      "Illegal scaling output parameters.\n");
            return 1;
         }
      } else if (!hw_feature->pp_dscaling_exceeding_max_size
                 && (ppu_cfg->scale.width != ppu_cfg->crop.width
                     || ppu_cfg->scale.height != ppu_cfg->crop.height)) {
         MWV207_LOG(MWV207_LEVEL_ERROR,
                   "Illegal scaling output parameters:\n"
                   "HW DOESN'T support scaling when scaling input size "
                   "greater than the maximum output size.\n");
         return 1;
      }
      if (ppu_cfg->tiled_e && ppu_cfg->planar) {
         MWV207_LOG(MWV207_LEVEL_ERROR,
                   "PLANAR is not supported with TILED4x4 output.\n");
         return 1;
      }
      if ((ppu_cfg->scale.width > ppu_cfg->crop.width)
          || (ppu_cfg->scale.height > ppu_cfg->crop.height))
         mwv207_cal_second_up_scale_ratio(ppu_cfg, interlace);
   }
   InitPpUnitLanczosData(hw_feature, ppu_cfg_temp);
   return 0; 
}

/*------------------------------------------------------------------------------
         Function name :
         Description   :

         Return type   : None
         Argument      : DecAsicBuffers_t * p_asic_buff
------------------------------------------------------------------------------*/
/*------------------------------------------------------------------------------

         Function: mwv207_pp_set_regs

                  Functional description:
                                Set registers based on pp configurations.

                  Input:
                                pp_regs     register array base pointer
(swreg0) hw_feature  hw feature struct pointer ppu_out_bus_addr  base address
of pp buffer mono_chrome   whether input picture is mono-chrome
                                bottom_field_flag set 1 if current is the
bottom field of a picture, otherwise (top field or frame) 0 align alignment
setting

                  Output:

                  Returns:

------------------------------------------------------------------------------*/

void
mwv207_pp_set_regs(struct mwv207_video_reg_buf *reg_buf,
                  struct mwv207_video_dec_hw_features *hw_feature,
                  mwv207_pp_unit_int_config *ppu_cfg,
                  uint32_t mono_chrome,
                  uint32_t bottom_field_flag)
{
   // uint32_t *pp_regs
   uint32_t i;

   if (1) {
      uint32_t ppw;

      for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
         reg_buf->set_reg(reg_buf, ppu_regs[i].PP_OUT_E_U, ppu_cfg->enabled);

         if (!ppu_cfg->enabled)
            continue;

         reg_buf->set_reg(
            reg_buf, ppu_regs[i].PP_CR_FIRST, ppu_cfg->cr_first);
         reg_buf->set_reg(
            reg_buf, ppu_regs[i].PP_OUT_TILE_E_U, ppu_cfg->tiled_e);
         reg_buf->set_reg(
            reg_buf, ppu_regs[i].OUT_FORMAT_U, ppu_cfg->out_format);
         reg_buf->set_reg(
            reg_buf, ppu_regs[i].PP_OUT_RGB_FMT_U, ppu_cfg->rgb_format);
         reg_buf->set_reg(
            reg_buf, ppu_regs[i].PP_RGB_PLANAR_U, ppu_cfg->rgb_planar);
         reg_buf->set_reg(
            reg_buf, ppu_regs[i].PP_OUT_ALPHA_U, ppu_cfg->rgb_alpha);
         reg_buf->set_reg(
            reg_buf, ppu_regs[i].DITHER_SELECT_R_U, ppu_cfg->dither_r);
         reg_buf->set_reg(
            reg_buf, ppu_regs[i].DITHER_SELECT_G_U, ppu_cfg->dither_g);
         reg_buf->set_reg(
            reg_buf, ppu_regs[i].DITHER_SELECT_B_U, ppu_cfg->dither_b);
         if (ppu_cfg->rgb || ppu_cfg->rgb_planar) {
            reg_buf->set_reg(
               reg_buf, JMDEC_COLOR_COEFFA2_U, coeff[ppu_cfg->rgb_stan][0]);
            reg_buf->set_reg(
               reg_buf, JMDEC_COLOR_COEFFA1_U, coeff[ppu_cfg->rgb_stan][0]);
            reg_buf->set_reg(
               reg_buf, JMDEC_COLOR_COEFFB_U, coeff[ppu_cfg->rgb_stan][1]);
            reg_buf->set_reg(
               reg_buf, JMDEC_COLOR_COEFFC_U, coeff[ppu_cfg->rgb_stan][2]);
            reg_buf->set_reg(
               reg_buf, JMDEC_COLOR_COEFFD_U, coeff[ppu_cfg->rgb_stan][3]);
            reg_buf->set_reg(
               reg_buf, JMDEC_COLOR_COEFFE_U, coeff[ppu_cfg->rgb_stan][4]);
            reg_buf->set_reg(
               reg_buf, JMDEC_YCBCR_RANGE_U, ppu_cfg->video_range);
            reg_buf->set_reg(
               reg_buf, JMDEC_RGB_RANGE_MAX_U, ppu_cfg->range_max);
            reg_buf->set_reg(
               reg_buf, JMDEC_RGB_RANGE_MIN_U, ppu_cfg->range_min);
         }
         reg_buf->set_reg(
            reg_buf,
            ppu_regs[i].PP_OUT_P010_FMT_U,
            (ppu_cfg->out_L010 ? 2 : (ppu_cfg->out_I010 ? 1 : 0)));

         {

            uint32_t in_width = ppu_cfg->crop.width;
            uint32_t in_height = ppu_cfg->crop.height;
            uint32_t out_width = ppu_cfg->scale.width;
            uint32_t out_height = ppu_cfg->scale.height;
            uint32_t out_ratio_x = 1;
            uint32_t out_ratio_y = 1;
            if ((in_width < out_width) || (in_height < out_height)) {
               out_width = ppu_cfg->out_width;
               out_height = ppu_cfg->out_height;
               out_ratio_x = ppu_cfg->out_ratio_x;
               out_ratio_y = ppu_cfg->out_ratio_y;
            }
            ppw = ppu_cfg->ystride;
          
            reg_buf->set_reg(reg_buf,
                             ppu_regs[i].CROP_STARTX_U,
                             ppu_cfg->crop.x >> hw_feature->crop_step_rshift);
            reg_buf->set_reg(reg_buf,
                             ppu_regs[i].CROP_STARTY_U,
                             ppu_cfg->crop.y >> hw_feature->crop_step_rshift);
            reg_buf->set_reg(reg_buf,
                             ppu_regs[i].PP_IN_WIDTH_U,
                             ppu_cfg->crop.width
                                >> hw_feature->crop_step_rshift);
            reg_buf->set_reg(reg_buf,
                             ppu_regs[i].PP_IN_HEIGHT_U,
                             ppu_cfg->crop.height
                                >> hw_feature->crop_step_rshift);
            reg_buf->set_reg(reg_buf,
                             ppu_regs[i].PP_CROP2_STARTX_U,
                             ppu_cfg->crop2.x
                                >> hw_feature->crop_step_rshift);
            reg_buf->set_reg(reg_buf,
                             ppu_regs[i].PP_CROP2_STARTY_U,
                             ppu_cfg->crop2.y
                                >> hw_feature->crop_step_rshift);
            reg_buf->set_reg(reg_buf,
                             ppu_regs[i].PP_CROP2_OUT_WIDTH_U,
                             ppu_cfg->crop2.width
                                >> hw_feature->crop_step_rshift);
            reg_buf->set_reg(reg_buf,
                             ppu_regs[i].PP_CROP2_OUT_HEIGHT_U,
                             ppu_cfg->crop2.height
                                >> hw_feature->crop_step_rshift);
            if (hw_feature->uscale_support_big_3[i]) {
               reg_buf->set_reg(
                  reg_buf, ppu_regs[i].SCALE1_OUT_WIDTH_U, out_width);
               reg_buf->set_reg(
                  reg_buf, ppu_regs[i].SCALE1_OUT_HEIGHT_U, out_height);
            }
            reg_buf->set_reg(
               reg_buf, ppu_regs[i].PP_OUT_WIDTH_U, ppu_cfg->scale.width);
            reg_buf->set_reg(
               reg_buf, ppu_regs[i].PP_OUT_HEIGHT_U, ppu_cfg->scale.height);

            if (in_width < out_width) {

               if (ppu_cfg->pp_lanczos) {
                  uint32_t W, inv_w;

                  reg_buf->set_reg(reg_buf, ppu_regs[i].HOR_SCALE_MODE_U, 1);

                  W = FDIVI(TOFIX(out_width, 32),
                            (TOFIX(in_width, 16) + out_width / 2));
                  inv_w =
                     FDIVI(TOFIX(in_width, 16) + out_width / 2, out_width);

                  reg_buf->set_reg(reg_buf, ppu_regs[i].SCALE_WRATIO_U, W);
                  reg_buf->set_reg(
                     reg_buf, ppu_regs[i].WSCALE_INVRA_U, (inv_w & 0xFFFF));
                  reg_buf->set_reg(
                     reg_buf, ppu_regs[i].WSCALE_INVRA_EXT_U, (inv_w >> 16));
               } else {
                  uint32_t W, inv_w;

                  reg_buf->set_reg(reg_buf, ppu_regs[i].HOR_SCALE_MODE_U, 1);

                  W = FDIVI(TOFIX((out_width - 1), 16), (in_width - 1));
                  inv_w = FDIVI(TOFIX((in_width - 1), 16), (out_width - 1));

                  reg_buf->set_reg(reg_buf, ppu_regs[i].SCALE_WRATIO_U, W);
                  reg_buf->set_reg(
                     reg_buf, ppu_regs[i].WSCALE_INVRA_U, inv_w);
                  reg_buf->set_reg(
                     reg_buf, ppu_regs[i].PP_DUP_HOR_U, out_ratio_x);
               }
            } else if (in_width > out_width) {

               if (ppu_cfg->pp_lanczos) {
                  uint32_t W, inv_w;

                  reg_buf->set_reg(reg_buf, ppu_regs[i].HOR_SCALE_MODE_U, 2);

                  W = FDIVI(TOFIX(out_width, 32),
                            (TOFIX(in_width, 16) + out_width / 2));
                  inv_w =
                     FDIVI(TOFIX(in_width, 16) + out_width / 2, out_width);

                  reg_buf->set_reg(reg_buf, ppu_regs[i].SCALE_WRATIO_U, W);
                  reg_buf->set_reg(
                     reg_buf, ppu_regs[i].WSCALE_INVRA_U, (inv_w & 0xFFFF));
                  reg_buf->set_reg(
                     reg_buf, ppu_regs[i].WSCALE_INVRA_EXT_U, (inv_w >> 16));
               } else {
                  uint32_t hnorm;

                  reg_buf->set_reg(reg_buf, ppu_regs[i].HOR_SCALE_MODE_U, 2);

                  hnorm = FDIVI(TOFIX(out_width, 16), in_width);
                  if (TOFIX(out_width, 16) % in_width)
                     hnorm++;

                  reg_buf->set_reg(
                     reg_buf, ppu_regs[i].WSCALE_INVRA_U, hnorm);
               }
            } else {
               reg_buf->set_reg(reg_buf, ppu_regs[i].WSCALE_INVRA_U, 0);
               reg_buf->set_reg(reg_buf, ppu_regs[i].HOR_SCALE_MODE_U, 0);
               reg_buf->set_reg(reg_buf, ppu_regs[i].PP_DUP_HOR_U, 1);
            }

            if (in_height < out_height) {

               if (ppu_cfg->pp_lanczos) {
                  uint32_t H, inv_h;

                  reg_buf->set_reg(reg_buf, ppu_regs[i].VER_SCALE_MODE_U, 1);

                  H = FDIVI(TOFIX(out_height, 32),
                            (TOFIX(in_height, 16) + out_height / 2));
                  inv_h =
                     FDIVI(TOFIX(in_height, 16) + out_height / 2, out_height);

                  reg_buf->set_reg(reg_buf, ppu_regs[i].SCALE_HRATIO_U, H);
                  reg_buf->set_reg(
                     reg_buf, ppu_regs[i].HSCALE_INVRA_U, (inv_h & 0xFFFF));
                  reg_buf->set_reg(
                     reg_buf, ppu_regs[i].HCALE_INVRA_EXT_U, (inv_h >> 16));
               } else {
                  uint32_t H, inv_h;

                  reg_buf->set_reg(reg_buf, ppu_regs[i].VER_SCALE_MODE_U, 1);

                  H = FDIVI(TOFIX((out_height - 1), 16), (in_height - 1));

                  reg_buf->set_reg(reg_buf, ppu_regs[i].SCALE_HRATIO_U, H);

                  inv_h = FDIVI(TOFIX((in_height - 1), 16), (out_height - 1));

                  reg_buf->set_reg(
                     reg_buf, ppu_regs[i].HSCALE_INVRA_U, inv_h);
                  reg_buf->set_reg(
                     reg_buf, ppu_regs[i].PP_DUP_VER_U, out_ratio_y);
               }
            } else if (in_height > out_height) {

               if (ppu_cfg->pp_lanczos) {
                  uint32_t H, inv_h;

                  reg_buf->set_reg(reg_buf, ppu_regs[i].VER_SCALE_MODE_U, 2);

                  H = FDIVI(TOFIX(out_height, 32),
                            (TOFIX(in_height, 16) + out_height / 2));
                  inv_h =
                     FDIVI(TOFIX(in_height, 16) + out_height / 2, out_height);

                  reg_buf->set_reg(reg_buf, ppu_regs[i].SCALE_HRATIO_U, H);
                  reg_buf->set_reg(
                     reg_buf, ppu_regs[i].HSCALE_INVRA_U, (inv_h & 0xFFFF));
                  reg_buf->set_reg(
                     reg_buf, ppu_regs[i].HCALE_INVRA_EXT_U, (inv_h >> 16));
               } else {
                  uint32_t Cv;

                  Cv = FDIVI(TOFIX(out_height, 16), in_height);
                  if (TOFIX(out_height, 16) % in_height)
                     Cv++;

                  reg_buf->set_reg(reg_buf, ppu_regs[i].VER_SCALE_MODE_U, 2);

                  reg_buf->set_reg(reg_buf, ppu_regs[i].HSCALE_INVRA_U, Cv);
               }
            } else {
               reg_buf->set_reg(reg_buf, ppu_regs[i].HSCALE_INVRA_U, 0);
               reg_buf->set_reg(reg_buf, ppu_regs[i].VER_SCALE_MODE_U, 0);
               reg_buf->set_reg(reg_buf, ppu_regs[i].PP_DUP_VER_U, 1);
            }
         }

         if (ppu_cfg->pp_lanczos) {
            reg_buf->set_addr_reg_hi(
               reg_buf,
               ppu_regs[i].PP_OUT_LANCZOS_TBL_BASE_U_MSB,
               ppu_cfg->lanczos_table->res->bo,
               0,
               MWV207_BO_ACC_FLAG_WRITE);
            reg_buf->set_addr_reg(reg_buf,
                                  ppu_regs[i].PP_OUT_LANCZOS_TBL_BASE_U_LSB,
                                  ppu_cfg->lanczos_table->res->bo,
                                  0,
                                  MWV207_BO_ACC_FLAG_WRITE);
            reg_buf->set_reg(
               reg_buf, ppu_regs[i].Y_FILTER_SIZE_U, ppu_cfg->y_filter_size);
            reg_buf->set_reg(
               reg_buf, ppu_regs[i].X_FILTER_SIZE_U, ppu_cfg->x_filter_size);
         }

         struct mwv207_video_buffer *buf =
            (struct mwv207_video_buffer *)ppu_cfg->outsurface;
         struct mwv207_resource *buf_res_y;
         struct mwv207_resource *buf_res_uv;
         buf_res_y = mwv207_resource(buf->resources[0]);
         buf_res_uv = mwv207_resource(buf->resources[1]);

         reg_buf->set_addr_reg_hi(reg_buf,
                                  ppu_regs[i].PP_OUT_LU_BASE_U_MSB,
                                  buf_res_y->bo,
                                  0,
                                  MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg(reg_buf,
                               ppu_regs[i].PP_OUT_LU_BASE_U_LSB,
                               buf_res_y->bo,
                               0,
                               MWV207_BO_ACC_FLAG_WRITE);

         reg_buf->set_addr_reg_hi(reg_buf,
                                  ppu_regs[i].PP_OUT_CH_BASE_U_MSB,
                                  buf_res_uv->bo,
                                  0,
                                  MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg(reg_buf,
                               ppu_regs[i].PP_OUT_CH_BASE_U_LSB,
                               buf_res_uv->bo,
                               0,
                               MWV207_BO_ACC_FLAG_WRITE);

         if (hw_feature->pp_stride_support) {
            reg_buf->set_reg(reg_buf, ppu_regs[i].PP_OUT_Y_STRIDE, ppw);
            reg_buf->set_reg(reg_buf, ppu_regs[i].PP_OUT_C_STRIDE, buf_res_uv->level_infos[0].stride);
         }

      }
   }
}

static void
CalcDec400TableSize(mwv207_pp_unit_int_config *ppu_cfg, uint32_t mono_chrome_flag)
{
   ppu_cfg->dec400_luma_table_size = NEXT_MULTIPLE(
      (ppu_cfg->ystride * ppu_cfg->scale.height / 256 * 2 + 7) / 8, 16);
   ppu_cfg->dec400_chroma_table_size = mono_chrome_flag
      ? 0
      : NEXT_MULTIPLE(
         (ppu_cfg->cstride * ppu_cfg->scale.height / 2 / 256 * 2 + 7) / 8,
         16);
}
void
mwv207_calc_each_pp_buffer_size(mwv207_pp_unit_int_config *ppu_cfg,
                               uint32_t dec400_mode,
                               uint32_t mono_chrome)
{
   uint32_t i = 0;
   uint32_t pp_stride, pp_hstride;
   for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled)
         continue;

      pp_stride = ppu_cfg->ystride;
      pp_hstride = ppu_cfg->hstride;
      ppu_cfg->luma_size = pp_stride * pp_hstride;
      ppu_cfg->luma_offset = 0;
      ppu_cfg->chroma_offset = ppu_cfg->luma_size;

      if (!ppu_cfg->monochrome && !mono_chrome) {
         if (ppu_cfg->tiled_e)
            pp_hstride = ppu_cfg->hstride / 2;
         else {
            if (ppu_cfg->planar) {
               pp_hstride = ppu_cfg->hstride;
            } else {
               pp_hstride = ppu_cfg->hstride / 2;
            }
         }
         pp_stride = ppu_cfg->cstride;
         ppu_cfg->chroma_size = pp_stride * pp_hstride;
      }

      if (ppu_cfg->rgb) {
         ppu_cfg->luma_size = ppu_cfg->ystride * ppu_cfg->hstride;
         ppu_cfg->luma_offset = 0;
         ppu_cfg->chroma_offset = ppu_cfg->luma_size;
         ppu_cfg->chroma_size = 0;
      }
      ppu_cfg->pp_pic_size =
         NEXT_MULTIPLE(ppu_cfg->luma_size + ppu_cfg->chroma_size, 16);
      if (MWV207_SURFACE_ATTRIB_DEC400_WORK_MODE == dec400_mode) {
         CalcDec400TableSize(ppu_cfg, mono_chrome);
      }
   }
}

uint32_t
mwv207_calc_pp_unit_buffer_size(mwv207_pp_unit_int_config *ppu_cfg,
                               uint32_t mono_chrome)
{
   uint32_t i = 0;
   uint32_t pp_stride, pp_buff_size, pp_hstride;
   uint32_t pp_data_size = 0;

   for (i = 0; i < DEC_MAX_PPU_COUNT; i++, ppu_cfg++) {
      if (!ppu_cfg->enabled)
         continue;

      pp_stride = ppu_cfg->ystride;
      pp_hstride = ppu_cfg->hstride;
      pp_buff_size = pp_stride * pp_hstride;
      ppu_cfg->luma_offset = pp_data_size;
      ppu_cfg->chroma_offset = pp_data_size + pp_buff_size;
      ppu_cfg->luma_size = pp_buff_size;

      if (!ppu_cfg->monochrome && !mono_chrome) {
         if (ppu_cfg->tiled_e)
            pp_hstride = ppu_cfg->hstride / 2;
         else {
            if (ppu_cfg->planar) {
               pp_hstride = ppu_cfg->hstride;
            } else {
               pp_hstride = ppu_cfg->hstride / 2;
            }
         }
         pp_stride = ppu_cfg->cstride;
         ppu_cfg->chroma_size = pp_stride * pp_hstride;
         pp_buff_size += pp_stride * pp_hstride;
      }
      pp_data_size += NEXT_MULTIPLE(pp_buff_size, 16);
   }
   return (pp_data_size);
}
#define PP_OUT_FMT_IYUVPACKED 7

void
mwv207_pp_unit_set_int_config(mwv207_pp_unit_int_config *ppu_int_cfg,
                             mwv207_pp_unit_config *ppu_ext_cfg,
                             uint32_t pixel_width,
                             uint32_t frame_only,
                             uint32_t mono_chrome)
{
   uint32_t i;
   if (ppu_int_cfg == NULL || ppu_ext_cfg == NULL)
      return;

   for (i = 0; i < DEC_MAX_PPU_COUNT; i++) {
      
      
      
      
      
      ppu_int_cfg->enabled = ppu_ext_cfg->enabled;
      ppu_int_cfg->tiled_e = ppu_ext_cfg->tiled_e;
      ppu_int_cfg->rgb = ppu_ext_cfg->rgb;
      ppu_int_cfg->rgb_planar = ppu_ext_cfg->rgb_planar;
      ppu_int_cfg->cr_first = ppu_ext_cfg->cr_first;
      ppu_int_cfg->shaper_enabled = ppu_ext_cfg->shaper_enabled;
      ppu_int_cfg->shaper_no_pad = ppu_ext_cfg->shaper_no_pad;
      ppu_int_cfg->dec400_enabled = ppu_ext_cfg->dec400_enabled;
      ppu_int_cfg->crop.enabled = ppu_ext_cfg->crop.enabled;
      ppu_int_cfg->crop.set_by_user = ppu_ext_cfg->crop.set_by_user;
      ppu_int_cfg->crop.x = ppu_ext_cfg->crop.x;
      ppu_int_cfg->crop.y = ppu_ext_cfg->crop.y;
      ppu_int_cfg->crop.width = ppu_ext_cfg->crop.width;
      ppu_int_cfg->crop.height = ppu_ext_cfg->crop.height;
      ppu_int_cfg->crop2.enabled = ppu_ext_cfg->crop2.enabled;
      ppu_int_cfg->crop2.x = ppu_ext_cfg->crop2.x;
      ppu_int_cfg->crop2.y = ppu_ext_cfg->crop2.y;
      ppu_int_cfg->crop2.width = ppu_ext_cfg->crop2.width;
      ppu_int_cfg->crop2.height = ppu_ext_cfg->crop2.height;
      ppu_int_cfg->scale.enabled = ppu_ext_cfg->scale.enabled;
      ppu_int_cfg->scale.set_by_user = ppu_ext_cfg->scale.set_by_user;
      ppu_int_cfg->scale.ratio_x = ppu_ext_cfg->scale.ratio_x;
      ppu_int_cfg->scale.ratio_y = ppu_ext_cfg->scale.ratio_y;
      ppu_int_cfg->scale.x = ppu_ext_cfg->scale.x;
      ppu_int_cfg->scale.y = ppu_ext_cfg->scale.y;
      ppu_int_cfg->scale.width = ppu_ext_cfg->scale.width;
      ppu_int_cfg->scale.height = ppu_ext_cfg->scale.height;
      ppu_int_cfg->monochrome = ppu_ext_cfg->monochrome;
      ppu_int_cfg->out_p010 = ppu_ext_cfg->out_p010;
      ppu_int_cfg->out_I010 = ppu_ext_cfg->out_I010;
      ppu_int_cfg->out_L010 = ppu_ext_cfg->out_L010;
      ppu_int_cfg->out_1010 = ppu_ext_cfg->out_1010;
      ppu_int_cfg->out_cut_8bits = ppu_ext_cfg->out_cut_8bits;
      ppu_int_cfg->planar = ppu_ext_cfg->planar;
      ppu_int_cfg->align = ppu_ext_cfg->align;
      ppu_int_cfg->ystride = ppu_ext_cfg->ystride;
      ppu_int_cfg->cstride = ppu_ext_cfg->cstride;
      ppu_int_cfg->video_range = ppu_ext_cfg->video_range;
      ppu_int_cfg->outsurface_origin_width =
         ppu_ext_cfg->outsurface_origin_width;
      ppu_int_cfg->outsurface_origin_height =
         ppu_ext_cfg->outsurface_origin_height;
      if (ppu_ext_cfg->rgb || ppu_ext_cfg->rgb_planar) {
         switch (ppu_ext_cfg->rgb_format) {
         case DEC_OUT_FRM_RGB888:
            ppu_int_cfg->rgb_format = PP_OUT_RGB888;
            break;
         case DEC_OUT_FRM_BGR888:
            ppu_int_cfg->rgb_format = PP_OUT_BGR888;
            break;
         case DEC_OUT_FRM_R16G16B16:
            ppu_int_cfg->rgb_format = PP_OUT_R16G16B16;
            break;
         case DEC_OUT_FRM_B16G16R16:
            ppu_int_cfg->rgb_format = PP_OUT_B16G16R16;
            break;
         case DEC_OUT_FRM_ARGB888:
            ppu_int_cfg->rgb_format = PP_OUT_ARGB888;
            break;
         case DEC_OUT_FRM_ABGR888:
            ppu_int_cfg->rgb_format = PP_OUT_ABGR888;
            break;
         case DEC_OUT_FRM_A2R10G10B10:
            ppu_int_cfg->rgb_format = PP_OUT_A2R10G10B10;
            break;
         case DEC_OUT_FRM_A2B10G10R10:
            ppu_int_cfg->rgb_format = PP_OUT_A2B10G10R10;
            break;
         case DEC_OUT_FRM_XRGB888:
            ppu_int_cfg->rgb_format = PP_OUT_XRGB888;
            break;
         case DEC_OUT_FRM_XBGR888:
            ppu_int_cfg->rgb_format = PP_OUT_XBGR888;
            break;
         default:
            ASSERT(0);
            break;
         }
         ppu_int_cfg->rgb_stan = ppu_ext_cfg->rgb_stan;
         ppu_int_cfg->rgb_alpha = ppu_ext_cfg->rgb_alpha;
      }

      if (ppu_ext_cfg->video_range) {
         if (pixel_width == 8) {
            ppu_int_cfg->range_max = 255;
            ppu_int_cfg->range_min = 0;
         } else {
            ppu_int_cfg->range_max = 1023;
            ppu_int_cfg->range_min = 0;
         }
      } else {
         if (pixel_width == 8) {
            ppu_int_cfg->range_max = 235;
            ppu_int_cfg->range_min = 16;
         } else {
            ppu_int_cfg->range_max = 940;
            ppu_int_cfg->range_min = 64;
         }
      }

      ppu_int_cfg->outsurface = ppu_ext_cfg->outsurface;
      if (ppu_ext_cfg->tiled_e && pixel_width > 8)
         ppu_int_cfg->out_cut_8bits = 0;
      if (pixel_width == 8 || ppu_int_cfg->rgb || ppu_int_cfg->rgb_planar) {
         ppu_int_cfg->out_cut_8bits = 0;
         ppu_int_cfg->out_p010 = 0;
         ppu_int_cfg->out_1010 = 0;
         ppu_int_cfg->out_I010 = 0;
         ppu_int_cfg->out_L010 = 0;
      }
      if (ppu_int_cfg->out_cut_8bits) {
         ppu_int_cfg->out_p010 = 0;
         ppu_int_cfg->out_1010 = 0;
         ppu_int_cfg->out_I010 = 0;
         ppu_int_cfg->out_L010 = 0;
      }
      if (!IS_PLANAR_RGB(ppu_int_cfg->rgb_format)) {
         ppu_int_cfg->rgb_planar = 0;
      }
      if (ppu_int_cfg->rgb || ppu_int_cfg->rgb_planar) {
         ppu_int_cfg->out_format = PP_OUT_FMT_RGB;
      } else if (ppu_int_cfg->monochrome || mono_chrome) {
         if (ppu_int_cfg->out_p010 || ppu_int_cfg->out_I010
             || ppu_int_cfg->out_L010
             || (ppu_ext_cfg->tiled_e && pixel_width > 8))
            ppu_int_cfg->out_format = PP_OUT_FMT_YUV400_P010;
         else if (ppu_int_cfg->out_cut_8bits)
            ppu_int_cfg->out_format = PP_OUT_FMT_YUV400_8BIT;
         else
            ppu_int_cfg->out_format = PP_OUT_FMT_YUV400;
         ppu_int_cfg->monochrome = 1;
      } else if (ppu_int_cfg->planar) {
         if (ppu_int_cfg->out_p010 || ppu_int_cfg->out_I010
             || ppu_int_cfg->out_L010)
            ppu_int_cfg->out_format = PP_OUT_FMT_IYUV_P010;
         else if (ppu_int_cfg->out_cut_8bits)
            ppu_int_cfg->out_format = PP_OUT_FMT_IYUV_8BIT;
         else
            ppu_int_cfg->out_format = PP_OUT_FMT_IYUVPACKED;
      } else if (ppu_int_cfg->out_1010) {
         ppu_int_cfg->out_format = PP_OUT_FMT_YUV420_10;
      } else {
         if (ppu_int_cfg->out_p010 || ppu_int_cfg->out_I010
             || ppu_int_cfg->out_L010
             || (ppu_ext_cfg->tiled_e && pixel_width > 8))
            ppu_int_cfg->out_format = PP_OUT_FMT_YUV420_P010;
         else if (ppu_int_cfg->out_cut_8bits)
            ppu_int_cfg->out_format = PP_OUT_FMT_YUV420_8BIT;
         else
            ppu_int_cfg->out_format = PP_OUT_FMT_YUV420PACKED;
      }
      if (!frame_only)
         ppu_int_cfg->tiled_e = 0;
      ppu_int_cfg->pixel_width =
         (ppu_int_cfg->out_cut_8bits
          || ((ppu_int_cfg->rgb || ppu_int_cfg->rgb_planar)
              && (IS_4PLAN_FORMAT(ppu_int_cfg->rgb_format)
                  || IS_8BITS_FORMAT(ppu_int_cfg->rgb_format)))
          || pixel_width == 8)
         ? 8
         : ((ppu_int_cfg->out_p010 || ppu_int_cfg->out_I010
             || ppu_int_cfg->out_L010
             || ((ppu_int_cfg->rgb || ppu_int_cfg->rgb_planar)
                 && IS_16BITS_FORMAT(ppu_int_cfg->rgb_format))
             || (ppu_ext_cfg->tiled_e && pixel_width > 8))
               ? 16
               : pixel_width);
      ppu_int_cfg->stream_pixel_width = pixel_width;
      if (ppu_int_cfg->rgb || ppu_int_cfg->rgb_planar) {
         if (pixel_width > 8 && IS_8BITS_FORMAT(ppu_int_cfg->rgb_format)) {
            ppu_int_cfg->dither_r = ppu_int_cfg->dither_g =
               ppu_int_cfg->dither_b = 3;
         } else {
            ppu_int_cfg->dither_r = ppu_int_cfg->dither_g =
               ppu_int_cfg->dither_b = 0;
         }
      }
      ppu_ext_cfg++;
      ppu_int_cfg++;
   }
}

enum mwv207_dec_picture_format
mwv207_trans_unit_config_2_format(mwv207_pp_unit_int_config *ppu_int_cfg)
{
   enum mwv207_dec_picture_format output_format = DEC_OUT_FRM_TILED_4X4;
   if (ppu_int_cfg->tiled_e) {
      if (ppu_int_cfg->monochrome) {
         if (ppu_int_cfg->pixel_width != 8) {
            if (ppu_int_cfg->out_cut_8bits == 1)
               output_format = DEC_OUT_FRM_YUV400TILE;
            else if (ppu_int_cfg->out_p010 || ppu_int_cfg->out_I010
                     || ppu_int_cfg->out_L010)
               output_format = DEC_OUT_FRM_YUV400TILE_P010;
            else if (ppu_int_cfg->out_1010 == 1)
               output_format = DEC_OUT_FRM_YUV400TILE_1010;
         } else
            output_format = DEC_OUT_FRM_YUV400TILE;
      } else
         output_format = DEC_OUT_FRM_TILED_4X4;
   } else if (ppu_int_cfg->rgb || ppu_int_cfg->rgb_planar) {
      switch (ppu_int_cfg->rgb_format) {
      case PP_OUT_RGB888:
         if (ppu_int_cfg->rgb_planar)
            output_format = DEC_OUT_FRM_RGB888_P;
         else
            output_format = DEC_OUT_FRM_RGB888;
         break;
      case PP_OUT_BGR888:
         if (ppu_int_cfg->rgb_planar)
            output_format = DEC_OUT_FRM_BGR888_P;
         else
            output_format = DEC_OUT_FRM_BGR888;
         break;
      case PP_OUT_R16G16B16:
         if (ppu_int_cfg->rgb_planar)
            output_format = DEC_OUT_FRM_R16G16B16_P;
         else
            output_format = DEC_OUT_FRM_R16G16B16;
         break;
      case PP_OUT_B16G16R16:
         if (ppu_int_cfg->rgb_planar)
            output_format = DEC_OUT_FRM_B16G16R16_P;
         else
            output_format = DEC_OUT_FRM_B16G16R16;
         break;
      case PP_OUT_ARGB888:
         output_format = DEC_OUT_FRM_ARGB888;
         break;
      case PP_OUT_ABGR888:
         output_format = DEC_OUT_FRM_ABGR888;
         break;
      case PP_OUT_A2R10G10B10:
         output_format = DEC_OUT_FRM_A2R10G10B10;
         break;
      case PP_OUT_A2B10G10R10:
         output_format = DEC_OUT_FRM_A2B10G10R10;
         break;
      case PP_OUT_XRGB888:
         output_format = DEC_OUT_FRM_XRGB888;
         break;
      case PP_OUT_XBGR888:
         output_format = DEC_OUT_FRM_XBGR888;
         break;
      default:
         ASSERT(0);
         break;
      }
   } else if (ppu_int_cfg->out_p010 || ppu_int_cfg->out_I010
              || ppu_int_cfg->out_L010) {
      if (ppu_int_cfg->monochrome)
         output_format = DEC_OUT_FRM_YUV400_P010;
      else if (ppu_int_cfg->planar)
         output_format = DEC_OUT_FRM_YUV420P_P010;
      else
         output_format = DEC_OUT_FRM_YUV420SP_P010;
   } else if (ppu_int_cfg->out_1010) {
      if (ppu_int_cfg->monochrome)
         output_format = DEC_OUT_FRM_YUV400_1010;
      else
         output_format = DEC_OUT_FRM_YUV420SP_1010;
   } else if (ppu_int_cfg->out_cut_8bits) {
      if (ppu_int_cfg->monochrome)
         output_format = DEC_OUT_FRM_YUV400;
      else {
         if (ppu_int_cfg->planar)
            output_format = DEC_OUT_FRM_YUV420P;
         else if (ppu_int_cfg->cr_first)
            output_format = DEC_OUT_FRM_NV21SP;
         else
            output_format = DEC_OUT_FRM_YUV420SP;
      }
   } else {
      if (ppu_int_cfg->monochrome)
         output_format = DEC_OUT_FRM_YUV400;
      else {
         if (ppu_int_cfg->planar)
            output_format = DEC_OUT_FRM_PLANAR_420;
         else if (ppu_int_cfg->cr_first)
            output_format = DEC_OUT_FRM_NV21SP;
         else
            output_format = DEC_OUT_FRM_RASTER_SCAN;
      }
   }
   return output_format;
}

void
mwv207_check_output_format(mwv207_pp_unit_int_config *ppu_cfg,
                          enum mwv207_dec_picture_format *output_format,
                          uint32_t index)
{
   uint32_t i = index;

   if (ppu_cfg[i].tiled_e) {
      if (ppu_cfg[i].monochrome) {
         if (ppu_cfg[i].out_p010 || ppu_cfg[i].out_I010
             || ppu_cfg[i].out_L010) {
            *output_format = DEC_OUT_FRM_YUV400TILE_P010;
         } else if (ppu_cfg[i].out_1010) {
            *output_format = DEC_OUT_FRM_YUV400TILE_1010;
         } else {
            *output_format = DEC_OUT_FRM_YUV400TILE;
         }
      } else if (ppu_cfg[i].cr_first) {
         if (ppu_cfg[i].out_p010 || ppu_cfg[i].out_I010
             || ppu_cfg[i].out_L010) {
            *output_format = DEC_OUT_FRM_NV21TILE_P010;
         } else if (ppu_cfg[i].out_1010) {
            *output_format = DEC_OUT_FRM_NV21TILE_1010;
         } else {
            *output_format = DEC_OUT_FRM_NV21TILE;
         }
      } else {
         if (ppu_cfg[i].out_p010 || ppu_cfg[i].out_I010
             || ppu_cfg[i].out_L010) {
            *output_format = DEC_OUT_FRM_YUV420TILE_P010;
         } else if (ppu_cfg[i].out_1010) {
            *output_format = DEC_OUT_FRM_YUV420TILE_1010;
         } else {
            *output_format = DEC_OUT_FRM_YUV420TILE;
         }
      }
   } else if (ppu_cfg[i].rgb || ppu_cfg[i].rgb_planar) {
      switch (ppu_cfg[i].rgb_format) {
      case PP_OUT_RGB888:
         if (ppu_cfg[i].rgb_planar)
            *output_format = DEC_OUT_FRM_RGB888_P;
         else
            *output_format = DEC_OUT_FRM_RGB888;
         break;
      case PP_OUT_BGR888:
         if (ppu_cfg[i].rgb_planar)
            *output_format = DEC_OUT_FRM_BGR888_P;
         else
            *output_format = DEC_OUT_FRM_BGR888;
         break;
      case PP_OUT_R16G16B16:
         if (ppu_cfg[i].rgb_planar)
            *output_format = DEC_OUT_FRM_R16G16B16_P;
         else
            *output_format = DEC_OUT_FRM_R16G16B16;
         break;
      case PP_OUT_B16G16R16:
         if (ppu_cfg[i].rgb_planar)
            *output_format = DEC_OUT_FRM_B16G16R16_P;
         else
            *output_format = DEC_OUT_FRM_B16G16R16;
         break;
      case PP_OUT_ARGB888:
         *output_format = DEC_OUT_FRM_ARGB888;
         break;
      case PP_OUT_ABGR888:
         *output_format = DEC_OUT_FRM_ABGR888;
         break;
      case PP_OUT_A2R10G10B10:
         *output_format = DEC_OUT_FRM_A2R10G10B10;
         break;
      case PP_OUT_A2B10G10R10:
         *output_format = DEC_OUT_FRM_A2B10G10R10;
         break;
      default:
         ASSERT(0);
         break;
      }
   } else {
      if (ppu_cfg[i].monochrome) {
         if (ppu_cfg[i].out_p010 || ppu_cfg[i].out_I010
             || ppu_cfg[i].out_L010) {
            *output_format = DEC_OUT_FRM_YUV400_P010;
         } else if (ppu_cfg[i].out_1010) {
            *output_format = DEC_OUT_FRM_YUV400_1010;
         } else {
            *output_format = DEC_OUT_FRM_YUV400;
         }
      } else if (ppu_cfg[i].cr_first) {
         if (ppu_cfg[i].out_p010 || ppu_cfg[i].out_I010
             || ppu_cfg[i].out_L010) {
            if (ppu_cfg[i].planar) {
               *output_format = DEC_OUT_FRM_NV21P_P010;
            } else {
               *output_format = DEC_OUT_FRM_NV21SP_P010;
            }
         } else if (ppu_cfg[i].out_1010) {
            if (ppu_cfg[i].planar) {
               *output_format = DEC_OUT_FRM_NV21P_1010;
            } else {
               *output_format = DEC_OUT_FRM_NV21SP_1010;
            }
         } else {
            if (ppu_cfg[i].planar) {
               *output_format = DEC_OUT_FRM_NV21P;
            } else {
               *output_format = DEC_OUT_FRM_NV21SP;
            }
         }
      } else {
         if (ppu_cfg[i].out_p010 || ppu_cfg[i].out_I010
             || ppu_cfg[i].out_L010) {
            if (ppu_cfg[i].planar) {
               *output_format = DEC_OUT_FRM_YUV420P_P010;
            } else {
               *output_format = DEC_OUT_FRM_YUV420SP_P010;
            }
         } else if (ppu_cfg[i].out_1010) {
            if (ppu_cfg[i].planar) {
               *output_format = DEC_OUT_FRM_YUV420P_1010;
            } else {
               *output_format = DEC_OUT_FRM_YUV420SP_1010;
            }
         } else {
            if (ppu_cfg[i].planar) {
               *output_format = DEC_OUT_FRM_YUV420P;
            } else {
               *output_format = DEC_OUT_FRM_YUV420SP;
            }
         }
      }
   }
}

