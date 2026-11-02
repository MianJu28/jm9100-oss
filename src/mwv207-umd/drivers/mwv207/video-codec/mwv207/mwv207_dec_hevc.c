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

#include "mwv207_dec_hevc.h"
#include "mwv207_dec_h264.h"
#include "mwv207_dec_ppu.h"
#include "mwv207_video_dec_cfg.h"
#include "vl/vl_video_buffer.h"
#include "mwv207_decoder_sw_util.h"
#include "mwv207_decoder_hevc_slice_header.h"
#include "mwv207_utils.h"
// #include <va/va.h>

/* 8 dc + 8 to boundary + 6*16 + 2*6*64 + 2*64 -> 63 * 16 bytes */
#define ASIC_SCALING_LIST_SIZE 16 * 64

#define ASIC_VERT_SAO_RAM_SIZE 48

#define MAX_TILE_COLS_HEVC_DEC 20
#define MAX_TILE_ROWS_HEVC_DEC 22

#define ASIC_VERT_FILTER_RAM_SIZE 8
/* BSD control data of current picture at tile border
 * 128 bits per 4x4 tile = 128/(8*4) bytes per row */
#define ASIC_BSD_CTRL_RAM_SIZE 4
#define USE_FAKE_RFC_TABLE

struct RefPicDeltaPoc {
   uint32_t index;
   uint32_t flags;
   int32_t delta_poc;
};

static const uint32_t ref_ybase[16] = {JMDEC_REFER0_YBASE_LSB,
                                  JMDEC_REFER1_YBASE_LSB,
                                  JMDEC_REFER2_YBASE_LSB,
                                  JMDEC_REFER3_YBASE_LSB,
                                  JMDEC_REFER4_YBASE_LSB,
                                  JMDEC_REFER5_YBASE_LSB,
                                  JMDEC_REFER6_YBASE_LSB,
                                  JMDEC_REFER7_YBASE_LSB,
                                  JMDEC_REFER8_YBASE_LSB,
                                  JMDEC_REFER9_YBASE_LSB,
                                  JMDEC_REFER10_YBASE_LSB,
                                  JMDEC_REFER11_YBASE_LSB,
                                  JMDEC_REFER12_YBASE_LSB,
                                  JMDEC_REFER13_YBASE_LSB,
                                  JMDEC_REFER14_YBASE_LSB,
                                  JMDEC_REFER15_YBASE_LSB};

static const uint32_t ref_cbase[16] = {JMDEC_REFER0_CBASE_LSB,
                                  JMDEC_REFER1_CBASE_LSB,
                                  JMDEC_REFER2_CBASE_LSB,
                                  JMDEC_REFER3_CBASE_LSB,
                                  JMDEC_REFER4_CBASE_LSB,
                                  JMDEC_REFER5_CBASE_LSB,
                                  JMDEC_REFER6_CBASE_LSB,
                                  JMDEC_REFER7_CBASE_LSB,
                                  JMDEC_REFER8_CBASE_LSB,
                                  JMDEC_REFER9_CBASE_LSB,
                                  JMDEC_REFER10_CBASE_LSB,
                                  JMDEC_REFER11_CBASE_LSB,
                                  JMDEC_REFER12_CBASE_LSB,
                                  JMDEC_REFER13_CBASE_LSB,
                                  JMDEC_REFER14_CBASE_LSB,
                                  JMDEC_REFER15_CBASE_LSB};

static const uint32_t ref_dbase[16] = {JMDEC_REFER0_DBASE_LSB,
                                  JMDEC_REFER1_DBASE_LSB,
                                  JMDEC_REFER2_DBASE_LSB,
                                  JMDEC_REFER3_DBASE_LSB,
                                  JMDEC_REFER4_DBASE_LSB,
                                  JMDEC_REFER5_DBASE_LSB,
                                  JMDEC_REFER6_DBASE_LSB,
                                  JMDEC_REFER7_DBASE_LSB,
                                  JMDEC_REFER8_DBASE_LSB,
                                  JMDEC_REFER9_DBASE_LSB,
                                  JMDEC_REFER10_DBASE_LSB,
                                  JMDEC_REFER11_DBASE_LSB,
                                  JMDEC_REFER12_DBASE_LSB,
                                  JMDEC_REFER13_DBASE_LSB,
                                  JMDEC_REFER14_DBASE_LSB,
                                  JMDEC_REFER15_DBASE_LSB};

/* static const uint32_t ref_tybase[16] = {JMDEC_REFER0_TYBASE_LSB,
                                   JMDEC_REFER1_TYBASE_LSB,
                                   JMDEC_REFER2_TYBASE_LSB,
                                   JMDEC_REFER3_TYBASE_LSB,
                                   JMDEC_REFER4_TYBASE_LSB,
                                   JMDEC_REFER5_TYBASE_LSB,
                                   JMDEC_REFER6_TYBASE_LSB,
                                   JMDEC_REFER7_TYBASE_LSB,
                                   JMDEC_REFER8_TYBASE_LSB,
                                   JMDEC_REFER9_TYBASE_LSB,
                                   JMDEC_REFER10_TYBASE_LSB,
                                   JMDEC_REFER11_TYBASE_LSB,
                                   JMDEC_REFER12_TYBASE_LSB,
                                   JMDEC_REFER13_TYBASE_LSB,
                                   JMDEC_REFER14_TYBASE_LSB,
                                   JMDEC_REFER15_TYBASE_LSB}; */

/* static const uint32_t ref_tcbase[16] = {JMDEC_REFER0_TCBASE_LSB,
                                   JMDEC_REFER1_TCBASE_LSB,
                                   JMDEC_REFER2_TCBASE_LSB,
                                   JMDEC_REFER3_TCBASE_LSB,
                                   JMDEC_REFER4_TCBASE_LSB,
                                   JMDEC_REFER5_TCBASE_LSB,
                                   JMDEC_REFER6_TCBASE_LSB,
                                   JMDEC_REFER7_TCBASE_LSB,
                                   JMDEC_REFER8_TCBASE_LSB,
                                   JMDEC_REFER9_TCBASE_LSB,
                                   JMDEC_REFER10_TCBASE_LSB,
                                   JMDEC_REFER11_TCBASE_LSB,
                                   JMDEC_REFER12_TCBASE_LSB,
                                   JMDEC_REFER13_TCBASE_LSB,
                                   JMDEC_REFER14_TCBASE_LSB,
                                   JMDEC_REFER15_TCBASE_LSB}; */

static const uint32_t ref_ybase_msb[16] = {JMDEC_REFER0_YBASE_MSB,
                                      JMDEC_REFER1_YBASE_MSB,
                                      JMDEC_REFER2_YBASE_MSB,
                                      JMDEC_REFER3_YBASE_MSB,
                                      JMDEC_REFER4_YBASE_MSB,
                                      JMDEC_REFER5_YBASE_MSB,
                                      JMDEC_REFER6_YBASE_MSB,
                                      JMDEC_REFER7_YBASE_MSB,
                                      JMDEC_REFER8_YBASE_MSB,
                                      JMDEC_REFER9_YBASE_MSB,
                                      JMDEC_REFER10_YBASE_MSB,
                                      JMDEC_REFER11_YBASE_MSB,
                                      JMDEC_REFER12_YBASE_MSB,
                                      JMDEC_REFER13_YBASE_MSB,
                                      JMDEC_REFER14_YBASE_MSB,
                                      JMDEC_REFER15_YBASE_MSB};

static const uint32_t ref_cbase_msb[16] = {JMDEC_REFER0_CBASE_MSB,
                                      JMDEC_REFER1_CBASE_MSB,
                                      JMDEC_REFER2_CBASE_MSB,
                                      JMDEC_REFER3_CBASE_MSB,
                                      JMDEC_REFER4_CBASE_MSB,
                                      JMDEC_REFER5_CBASE_MSB,
                                      JMDEC_REFER6_CBASE_MSB,
                                      JMDEC_REFER7_CBASE_MSB,
                                      JMDEC_REFER8_CBASE_MSB,
                                      JMDEC_REFER9_CBASE_MSB,
                                      JMDEC_REFER10_CBASE_MSB,
                                      JMDEC_REFER11_CBASE_MSB,
                                      JMDEC_REFER12_CBASE_MSB,
                                      JMDEC_REFER13_CBASE_MSB,
                                      JMDEC_REFER14_CBASE_MSB,
                                      JMDEC_REFER15_CBASE_MSB};

static const uint32_t ref_dbase_msb[16] = {JMDEC_REFER0_DBASE_MSB,
                                      JMDEC_REFER1_DBASE_MSB,
                                      JMDEC_REFER2_DBASE_MSB,
                                      JMDEC_REFER3_DBASE_MSB,
                                      JMDEC_REFER4_DBASE_MSB,
                                      JMDEC_REFER5_DBASE_MSB,
                                      JMDEC_REFER6_DBASE_MSB,
                                      JMDEC_REFER7_DBASE_MSB,
                                      JMDEC_REFER8_DBASE_MSB,
                                      JMDEC_REFER9_DBASE_MSB,
                                      JMDEC_REFER10_DBASE_MSB,
                                      JMDEC_REFER11_DBASE_MSB,
                                      JMDEC_REFER12_DBASE_MSB,
                                      JMDEC_REFER13_DBASE_MSB,
                                      JMDEC_REFER14_DBASE_MSB,
                                      JMDEC_REFER15_DBASE_MSB};

/* static const uint32_t ref_tybase_msb[16] = {JMDEC_REFER0_TYBASE_MSB,
                                       JMDEC_REFER1_TYBASE_MSB,
                                       JMDEC_REFER2_TYBASE_MSB,
                                       JMDEC_REFER3_TYBASE_MSB,
                                       JMDEC_REFER4_TYBASE_MSB,
                                       JMDEC_REFER5_TYBASE_MSB,
                                       JMDEC_REFER6_TYBASE_MSB,
                                       JMDEC_REFER7_TYBASE_MSB,
                                       JMDEC_REFER8_TYBASE_MSB,
                                       JMDEC_REFER9_TYBASE_MSB,
                                       JMDEC_REFER10_TYBASE_MSB,
                                       JMDEC_REFER11_TYBASE_MSB,
                                       JMDEC_REFER12_TYBASE_MSB,
                                       JMDEC_REFER13_TYBASE_MSB,
                                       JMDEC_REFER14_TYBASE_MSB,
                                       JMDEC_REFER15_TYBASE_MSB}; */

/* static const uint32_t ref_tcbase_msb[16] = {JMDEC_REFER0_TCBASE_MSB,
                                       JMDEC_REFER1_TCBASE_MSB,
                                       JMDEC_REFER2_TCBASE_MSB,
                                       JMDEC_REFER3_TCBASE_MSB,
                                       JMDEC_REFER4_TCBASE_MSB,
                                       JMDEC_REFER5_TCBASE_MSB,
                                       JMDEC_REFER6_TCBASE_MSB,
                                       JMDEC_REFER7_TCBASE_MSB,
                                       JMDEC_REFER8_TCBASE_MSB,
                                       JMDEC_REFER9_TCBASE_MSB,
                                       JMDEC_REFER10_TCBASE_MSB,
                                       JMDEC_REFER11_TCBASE_MSB,
                                       JMDEC_REFER12_TCBASE_MSB,
                                       JMDEC_REFER13_TCBASE_MSB,
                                       JMDEC_REFER14_TCBASE_MSB,
                                       JMDEC_REFER15_TCBASE_MSB}; */

static const uint32_t ref_pic_list0[16] = {JMDEC_INIT_RLIST_F0,
                                      JMDEC_INIT_RLIST_F1,
                                      JMDEC_INIT_RLIST_F2,
                                      JMDEC_INIT_RLIST_F3,
                                      JMDEC_INIT_RLIST_F4,
                                      JMDEC_INIT_RLIST_F5,
                                      JMDEC_INIT_RLIST_F6,
                                      JMDEC_INIT_RLIST_F7,
                                      JMDEC_INIT_RLIST_F8,
                                      JMDEC_INIT_RLIST_F9,
                                      JMDEC_INIT_RLIST_F10,
                                      JMDEC_INIT_RLIST_F11,
                                      JMDEC_INIT_RLIST_F12,
                                      JMDEC_INIT_RLIST_F13,
                                      JMDEC_INIT_RLIST_F14,
                                      JMDEC_INIT_RLIST_F15};

static const uint32_t ref_pic_list1[16] = {JMDEC_INIT_RLIST_B0,
                                      JMDEC_INIT_RLIST_B1,
                                      JMDEC_INIT_RLIST_B2,
                                      JMDEC_INIT_RLIST_B3,
                                      JMDEC_INIT_RLIST_B4,
                                      JMDEC_INIT_RLIST_B5,
                                      JMDEC_INIT_RLIST_B6,
                                      JMDEC_INIT_RLIST_B7,
                                      JMDEC_INIT_RLIST_B8,
                                      JMDEC_INIT_RLIST_B9,
                                      JMDEC_INIT_RLIST_B10,
                                      JMDEC_INIT_RLIST_B11,
                                      JMDEC_INIT_RLIST_B12,
                                      JMDEC_INIT_RLIST_B13,
                                      JMDEC_INIT_RLIST_B14,
                                      JMDEC_INIT_RLIST_B15};

static const uint32_t ref_poc_regs[16] = {JMDEC_CUR_POC_00,
                                     JMDEC_CUR_POC_01,
                                     JMDEC_CUR_POC_02,
                                     JMDEC_CUR_POC_03,
                                     JMDEC_CUR_POC_04,
                                     JMDEC_CUR_POC_05,
                                     JMDEC_CUR_POC_06,
                                     JMDEC_CUR_POC_07,
                                     JMDEC_CUR_POC_08,
                                     JMDEC_CUR_POC_09,
                                     JMDEC_CUR_POC_10,
                                     JMDEC_CUR_POC_11,
                                     JMDEC_CUR_POC_12,
                                     JMDEC_CUR_POC_13,
                                     JMDEC_CUR_POC_14,
                                     JMDEC_CUR_POC_15};

static void
dec_hevc_calc_sur_offset(struct mwv207_video_decoder *decoder,
                         struct mwv207_h265_picture_desc *pic_param,
                         struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;

   struct mwv207_video_buffer *buf;

   buf = (struct mwv207_video_buffer *)target;

   uint8_t ctb_size =
      (1 << (pic_param->pps.sps.log2_min_luma_coding_block_size_minus3 + 3
             + pic_param->pps.sps.log2_diff_max_min_luma_coding_block_size));

   uint32_t pic_width_ctbs_nums =
      NEXT_MULTIPLE(pic_param->pps.sps.pic_width_in_luma_samples, ctb_size)
      / ctb_size;
   uint32_t pic_height_ctbs_nums =
      NEXT_MULTIPLE(pic_param->pps.sps.pic_height_in_luma_samples, ctb_size)
      / ctb_size;

   uint32_t luma_size;
   uint32_t tbl_sizey, tbl_sizec;

   uint32_t out_w, out_h;
   uint32_t ref_size;

   out_w = NEXT_MULTIPLE(4 * pic_param->pps.sps.pic_width_in_luma_samples
                            * dec->dec_param.bit_depth,
                         ALIGN(dec->dec_param.w_align) * 8)
      / 8;
   out_h = pic_param->pps.sps.pic_height_in_luma_samples / 4;
   ref_size = out_w * out_h;

   luma_size = ref_size;

   if (dec->dec_param.use_video_compressor) {
      uint32_t pic_width_in_cbsy, pic_height_in_cbsy;
      uint32_t pic_width_in_cbsc, pic_height_in_cbsc;
      pic_width_in_cbsy =
         ((pic_param->pps.sps.pic_width_in_luma_samples + 8 - 1) / 8);
      pic_width_in_cbsy = NEXT_MULTIPLE(pic_width_in_cbsy, 16);
      pic_width_in_cbsc =
         ((pic_param->pps.sps.pic_width_in_luma_samples + 16 - 1) / 16);
      pic_width_in_cbsc = NEXT_MULTIPLE(pic_width_in_cbsc, 16);
      pic_height_in_cbsy =
         (pic_param->pps.sps.pic_height_in_luma_samples + 8 - 1) / 8;
      pic_height_in_cbsc =
         (pic_param->pps.sps.pic_height_in_luma_samples / 2 + 4 - 1) / 4;

      tbl_sizey = NEXT_MULTIPLE(pic_width_in_cbsy * pic_height_in_cbsy, 16);
      tbl_sizec = NEXT_MULTIPLE(pic_width_in_cbsc * pic_height_in_cbsc, 16);
   } else
      tbl_sizey = tbl_sizec = 0;

   uint32_t dmv_mem_size =

      (pic_width_ctbs_nums * pic_height_ctbs_nums *

       (1 << (2
              * (pic_param->pps.sps.log2_min_luma_coding_block_size_minus3 + 3
                 + pic_param->pps.sps.log2_diff_max_min_luma_coding_block_size
                 - 4)))
       *

       16);

   uint32_t buff_size = NEXT_MULTIPLE(luma_size * 3 / 2, 16) + dmv_mem_size;
   uint32_t dir_mv_offset = NEXT_MULTIPLE(luma_size * 3 / 2, 16);
   buf->mwv207_offset_info.luma_size = luma_size;
   buf->mwv207_offset_info.dmv_mem_size = dmv_mem_size;

   
   dir_mv_offset += 32;
   buff_size += 32;
   buf->mwv207_offset_info.dir_mv_offset = dir_mv_offset;

   buff_size = buff_size + tbl_sizey + tbl_sizec;

#ifdef USE_FAKE_RFC_TABLE
   buf->mwv207_offset_info.fake_tbly_size = tbl_sizey;
   buf->mwv207_offset_info.fake_tblc_size = tbl_sizec;
#endif

   if (tbl_sizey) {
      buf->mwv207_offset_info.cbs_tbl_offsety =
         buff_size - tbl_sizey - tbl_sizec;
      buf->mwv207_offset_info.cbs_tbl_offsetc = buff_size - tbl_sizec;
   }
   buf->tiled_pic_size = buff_size;

   if (!buf->res) {
      mwv207_video_buffer_alloc_buffer(
         decoder->base.context, buf, buf->tiled_pic_size, PIPE_USAGE_STAGING);
   }
}

static void
dec_hevc_set_pp_reg(struct mwv207_video_decoder *decoder,
                    struct mwv207_h265_picture_desc *pic_param,
                    struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;

   mwv207_pp_unit_config ppu_ext_cfg[DEC_MAX_PPU_COUNT] = {0};
   if (dec->dec_param.pp_enabled) {
      mwv207_dec_pp_params_init(
         dec, ppu_ext_cfg, dec->dec_param.bit_depth, target);

      mwv207_pp_unit_set_int_config(
         dec->ppu_cfg, ppu_ext_cfg, dec->dec_param.bit_depth, 0, 0);

      uint32_t in_width = align(decoder->base.width, 16);
      uint32_t in_height = align(decoder->base.height, 16);

      for (uint32_t i = 0; i < DEC_MAX_PPU_COUNT; i++) {
         if ((dec->hw_feature.pp_up_lanczos[i]
              || dec->hw_feature.pp_down_lanczos[i])
             && (dec->ppu_cfg[i].lanczos_table == NULL)) {
            uint32_t size = LANCZOS_BUFFER_SIZE * sizeof(int32_t);

            dec->ppu_cfg[i].lanczos_table = mwv207_video_buffer_create_2(
               decoder->base.context, size, PIPE_USAGE_STAGING);
         }
      }

      if (mwv207_check_pp_unit_config(
             &dec->hw_feature, in_width, in_height, 0, dec->ppu_cfg)) {
         MWV207_LOG(MWV207_LEVEL_ERROR, "!!! pp set failed \n");
      }
      mwv207_calc_each_pp_buffer_size(dec->ppu_cfg, 0, 0);

      mwv207_pp_unit_int_config *ppu_cfg = &dec->ppu_cfg[0];

      mwv207_pp_set_regs(reg_buf, &dec->hw_feature, ppu_cfg, 0, 0);

      reg_buf->set_reg(reg_buf, JMDEC_PP_IN_FORMAT_U, 1);
   }
}

static void
dec_hevc_set_picparam_reg(struct mwv207_video_decoder *decoder,
                          struct mwv207_h265_picture_desc *pic_param)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;

   struct mwv207_video_dec_hw_features *hw_feature = &dec->hw_feature;

   uint8_t min_cb_size =
      (1 << (pic_param->pps.sps.log2_min_luma_coding_block_size_minus3 + 3));
   uint8_t ctb_size =
      (1 << (pic_param->pps.sps.log2_min_luma_coding_block_size_minus3 + 3
             + pic_param->pps.sps.log2_diff_max_min_luma_coding_block_size));

   uint32_t pic_width_in_pixels = pic_param->pps.sps.pic_width_in_luma_samples;
   uint32_t pic_height_in_pixels = pic_param->pps.sps.pic_height_in_luma_samples;
   uint32_t pic_width_align_by_ctbs = NEXT_MULTIPLE(pic_width_in_pixels, ctb_size);
   uint32_t pic_height_align_by_ctbs =
      NEXT_MULTIPLE(pic_height_in_pixels, ctb_size);
   uint32_t pic_width_in_cbs = pic_width_in_pixels
      >> (pic_param->pps.sps.log2_min_luma_coding_block_size_minus3 + 3);
   uint32_t pic_height_in_cbs = pic_height_in_pixels
      >> (pic_param->pps.sps.log2_min_luma_coding_block_size_minus3 + 3);

   uint32_t partial_ctb_h = pic_width_in_pixels != pic_width_align_by_ctbs ? 1 : 0;
   uint32_t partial_ctb_v =
      pic_height_in_pixels != pic_height_align_by_ctbs ? 1 : 0;

   reg_buf->set_reg(reg_buf, JMDEC_BUFFER_EMPTY_INT_E, 1);
   reg_buf->set_reg(reg_buf, JMDEC_BLOCK_BUFFER_MODE_E, 0);
   reg_buf->set_reg(reg_buf, JMDEC_PIC_WIDTH_IN_CBS, pic_width_in_cbs);
   reg_buf->set_reg(reg_buf, JMDEC_PIC_HEIGHT_IN_CBS, pic_height_in_cbs);
   reg_buf->set_reg(reg_buf, JMDEC_PARTIAL_CTB_X, partial_ctb_h);
   reg_buf->set_reg(reg_buf, JMDEC_PARTIAL_CTB_Y, partial_ctb_v);

   reg_buf->set_reg(
      reg_buf, JMDEC_PIC_WIDTH_4X4, (pic_width_in_cbs * min_cb_size) >> 2);
   reg_buf->set_reg(
      reg_buf, JMDEC_PIC_HEIGHT_4X4, (pic_height_in_cbs * min_cb_size) >> 2);

   reg_buf->set_reg(reg_buf, JMDEC_BLACKWHITE_E, 0);

   reg_buf->set_reg(reg_buf,
                    JMDEC_BIT_DEPTH_Y_MINUS8,
                    pic_param->pps.sps.bit_depth_luma_minus8);
   reg_buf->set_reg(reg_buf,
                    JMDEC_BIT_DEPTH_C_MINUS8,
                    pic_param->pps.sps.bit_depth_chroma_minus8);

   reg_buf->set_reg(
      reg_buf, JMDEC_PCM_E, pic_param->pps.sps.pcm_enabled_flag);
   reg_buf->set_reg(
      reg_buf,
      JMDEC_PCM_BITDEPTH_Y,
      (uint8_t)(pic_param->pps.sps.pcm_sample_bit_depth_luma_minus1 + 1));
   reg_buf->set_reg(
      reg_buf,
      JMDEC_PCM_BITDEPTH_C,
      (uint8_t)(pic_param->pps.sps.pcm_sample_bit_depth_chroma_minus1 + 1));

   reg_buf->set_reg(reg_buf,
                    JMDEC_REFPICLIST_MOD_E,
                    pic_param->pps.lists_modification_present_flag);

   reg_buf->set_reg(reg_buf,
                    JMDEC_MIN_CB_SIZE,
                    pic_param->pps.sps.log2_min_luma_coding_block_size_minus3
                       + 3);
   reg_buf->set_reg(
      reg_buf,
      JMDEC_MAX_CB_SIZE,
      pic_param->pps.sps.log2_min_luma_coding_block_size_minus3 + 3
         + pic_param->pps.sps.log2_diff_max_min_luma_coding_block_size);
   reg_buf->set_reg(reg_buf,
                    JMDEC_MIN_TRB_SIZE,
                    pic_param->pps.sps.log2_min_transform_block_size_minus2
                       + 2);
   reg_buf->set_reg(
      reg_buf,
      JMDEC_MAX_TRB_SIZE,
      pic_param->pps.sps.log2_min_transform_block_size_minus2 + 2
         + pic_param->pps.sps.log2_diff_max_min_transform_block_size);
   reg_buf->set_reg(
      reg_buf,
      JMDEC_MIN_PCM_SIZE,
      (char)(pic_param->pps.sps.log2_min_pcm_luma_coding_block_size_minus3
             + 3));
   reg_buf->set_reg(
      reg_buf,
      JMDEC_MAX_PCM_SIZE,
      (char)(pic_param->pps.sps.log2_min_pcm_luma_coding_block_size_minus3 + 3
             + pic_param->pps.sps
                  .log2_diff_max_min_pcm_luma_coding_block_size));

   reg_buf->set_reg(reg_buf,
                    JMDEC_MAX_INTER_HIERDEPTH,
                    pic_param->pps.sps.max_transform_hierarchy_depth_inter);
   reg_buf->set_reg(reg_buf,
                    JMDEC_MAX_INTRA_HIERDEPTH,
                    pic_param->pps.sps.max_transform_hierarchy_depth_intra);

   reg_buf->set_reg(
      reg_buf, JMDEC_ASYM_PRED_E, pic_param->pps.sps.amp_enabled_flag);
   reg_buf->set_reg(reg_buf,
                    JMDEC_SAO_E,
                    pic_param->pps.sps.sample_adaptive_offset_enabled_flag);

   reg_buf->set_reg(reg_buf,
                    JMDEC_PCM_FILT_DISABLE,
                    pic_param->pps.sps.pcm_loop_filter_disabled_flag);

   reg_buf->set_reg(reg_buf,
                    JMDEC_TEMPOR_MVP_E,
                    pic_param->pps.sps.sps_temporal_mvp_enabled_flag
                       && !pic_param->IDRPicFlag);

   reg_buf->set_reg(reg_buf,
                    JMDEC_SIGN_DATA_HIDE,
                    pic_param->pps.sign_data_hiding_enabled_flag);

   reg_buf->set_reg(reg_buf,
                    JMDEC_CABAC_INIT_PRESENT,
                    pic_param->pps.cabac_init_present_flag);

   reg_buf->set_reg(reg_buf,
                    JMDEC_REFIDX0_ACTIVE,
                    pic_param->pps.num_ref_idx_l0_default_active_minus1 + 1);
   reg_buf->set_reg(reg_buf,
                    JMDEC_REFIDX1_ACTIVE,
                    pic_param->pps.num_ref_idx_l1_default_active_minus1 + 1);

   reg_buf->set_reg(
      reg_buf, JMDEC_INIT_QP, pic_param->pps.init_qp_minus26 + 26);

   reg_buf->set_reg(reg_buf,
                    JMDEC_CONST_INTRA_E,
                    pic_param->pps.constrained_intra_pred_flag);

   reg_buf->set_reg(reg_buf,
                    JMDEC_TRANSFORM_SKIP_E,
                    pic_param->pps.transform_skip_enabled_flag);

   reg_buf->set_reg(
      reg_buf, JMDEC_CU_QPD_E, pic_param->pps.cu_qp_delta_enabled_flag);

   reg_buf->set_reg(
      reg_buf, JMDEC_MAX_CU_QPD_DEPTH, pic_param->pps.diff_cu_qp_delta_depth);

   reg_buf->set_reg(
      reg_buf, JMDEC_CH_QP_OFFSET, pic_param->pps.pps_cb_qp_offset);
   reg_buf->set_reg(
      reg_buf, JMDEC_CH_QP_OFFSET2, pic_param->pps.pps_cr_qp_offset);

   reg_buf->set_reg(reg_buf,
                    JMDEC_SLICE_CHQP_FLAG,
                    pic_param->pps.pps_slice_chroma_qp_offsets_present_flag);

   reg_buf->set_reg(
      reg_buf, JMDEC_WEIGHT_PRED_E, pic_param->pps.weighted_pred_flag);
   reg_buf->set_reg(
      reg_buf, JMDEC_WEIGHT_BIPR_IDC, pic_param->pps.weighted_bipred_flag);

   reg_buf->set_reg(reg_buf,
                    JMDEC_TRANSQ_BYPASS_E,
                    pic_param->pps.transquant_bypass_enabled_flag);

   reg_buf->set_reg(reg_buf,
                    JMDEC_DEPEND_SLICE_E,
                    pic_param->pps.dependent_slice_segments_enabled_flag);

   reg_buf->set_reg(reg_buf,
                    JMDEC_SLICE_HDR_EBITS,
                    pic_param->pps.num_extra_slice_header_bits); 

   reg_buf->set_reg(reg_buf,
                    JMDEC_STRONG_SMOOTH_E,
                    pic_param->pps.sps.strong_intra_smoothing_enabled_flag);

   reg_buf->set_reg(reg_buf,
                    JMDEC_TILE_ENABLE,
                    pic_param->pps.tiles_enabled_flag); 

   reg_buf->set_reg(reg_buf,
                    JMDEC_SCALING_LIST_E,
                    pic_param->pps.sps.scaling_list_enabled_flag);

   if (!hw_feature->pic_size_reg_unified) {
      reg_buf->set_reg(reg_buf,
                       JMDEC_NUM_TILE_COLS,
                       pic_param->pps.num_tile_columns_minus1 + 1);
      reg_buf->set_reg(reg_buf,
                       JMDEC_NUM_TILE_ROWS,
                       pic_param->pps.num_tile_rows_minus1 + 1);
   } else {
      reg_buf->set_reg(reg_buf,
                       JMDEC_NUM_TILE_COLS_8K,
                       pic_param->pps.num_tile_columns_minus1 + 1);
      reg_buf->set_reg(reg_buf,
                       JMDEC_NUM_TILE_ROWS_8K,
                       pic_param->pps.num_tile_rows_minus1 + 1);
   }

   reg_buf->set_reg(reg_buf,
                    JMDEC_ENTR_CODE_SYNCH_E,
                    pic_param->pps.entropy_coding_sync_enabled_flag);

   reg_buf->set_reg(
      reg_buf,
      JMDEC_FILT_SLICE_BORDER,
      pic_param->pps.pps_loop_filter_across_slices_enabled_flag);

   reg_buf->set_reg(reg_buf,
                    JMDEC_FILT_TILE_BORDER,
                    pic_param->pps.loop_filter_across_tiles_enabled_flag);

   reg_buf->set_reg(reg_buf,
                    JMDEC_FILT_CTRL_PRES,
                    pic_param->pps.deblocking_filter_override_enabled_flag
                       || pic_param->pps.pps_deblocking_filter_disabled_flag
                       || pic_param->pps.pps_beta_offset_div2
                       || pic_param->pps.pps_tc_offset_div2);

   reg_buf->set_reg(reg_buf,
                    JMDEC_FILT_OVERRIDE_E,
                    pic_param->pps.deblocking_filter_override_enabled_flag);

   reg_buf->set_reg(reg_buf,
                    JMDEC_FILTERING_DIS,
                    pic_param->pps.pps_deblocking_filter_disabled_flag);

   reg_buf->set_reg(
      reg_buf, JMDEC_FILT_OFFSET_BETA, pic_param->pps.pps_beta_offset_div2);
   reg_buf->set_reg(
      reg_buf, JMDEC_FILT_OFFSET_TC, pic_param->pps.pps_tc_offset_div2);

   reg_buf->set_reg(reg_buf,
                    JMDEC_PARALLEL_MERGE,
                    pic_param->pps.log2_parallel_merge_level_minus2 + 2);

   reg_buf->set_reg(reg_buf, JMDEC_IDR_PIC_E, pic_param->RAPPicFlag);
   reg_buf->set_reg(
      reg_buf,
      JMDEC_SLICE_HDR_EXT_E,
      pic_param->pps.slice_segment_header_extension_present_flag);
}

static void
dec_hevc_set_skip_length_reg(struct mwv207_video_decoder *decoder,
                             struct mwv207_h265_picture_desc *pic_param)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;

   uint8_t *slice_address = NULL;
   uint32_t strm_buff_size = 0;

   
   

   slice_address = decoder->streams[0];
   strm_buff_size =  decoder->streams_len[0]; 

   ASSERT(slice_address);
   struct mwv207_strm_data rb;
   struct mwv207_hevc_slice_header slice_header;
   uint32_t hw_skip_bits = 0;
   rb.strm_buff_start = slice_address;
   rb.strm_curr_pos = (uint8_t *)slice_address + 2;
   rb.bit_pos_in_word = 0;
   rb.strm_buff_size = strm_buff_size;
   rb.strm_data_size = decoder->bs_size - 3;
   rb.strm_buff_read_bits = 0;
   rb.remove_emul3_byte = 0;
   rb.emul_byte_count = 0;
   rb.is_rb = 0;
   struct mwv207_h265_picture_desc pic_param_tmp;
   struct mwv207_h265_picture_desc *pic_param_tmp_p = &pic_param_tmp;
   memcpy(&pic_param_tmp, pic_param, sizeof(struct mwv207_h265_picture_desc));

   pic_param_tmp_p->st_rps_bits = 0;
   mwv207_hevc_decode_slice_header(&rb,
                                  pic_param_tmp_p,
                                  &hw_skip_bits,
                                  &slice_header,
                                  pic_param->st_rps_bits);

   reg_buf->set_reg(reg_buf, JMDEC_HDR_SKIP_LENGTH, hw_skip_bits);
}

static void
dec_hevc_set_out_reg(struct mwv207_video_decoder *decoder,
                     struct mwv207_h265_picture_desc *pic_param,
                     struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;
   struct mwv207_video_dec_hw_features *hw_feature = &dec->hw_feature;
   struct mwv207_resource *buf_res_y;
   struct mwv207_resource *buf_res_uv;

   uint32_t pixel_width = (pic_param->pps.sps.bit_depth_luma_minus8 == 0
                      && pic_param->pps.sps.bit_depth_chroma_minus8 == 0)
      ? 8
      : 10;
   struct mwv207_video_buffer *buf = (struct mwv207_video_buffer *)target;
   buf_res_y = mwv207_resource(buf->resources[0]); 
   buf_res_uv = mwv207_resource(buf->resources[1]); 

   reg_buf->set_addr_reg_hi(reg_buf,
                            JMDEC_DEC_OUT_YBASE_MSB,
                            buf->res->bo,
                            0,
                            MWV207_BO_ACC_FLAG_WRITE);
   reg_buf->set_addr_reg(reg_buf,
                         JMDEC_DEC_OUT_YBASE_LSB,
                         buf->res->bo,
                         0,
                         MWV207_BO_ACC_FLAG_WRITE);

   reg_buf->set_addr_reg_hi(reg_buf,
                            JMDEC_DEC_OUT_CBASE_MSB,
                            buf->res->bo,
                            buf->mwv207_offset_info.luma_size,
                            MWV207_BO_ACC_FLAG_WRITE);
   reg_buf->set_addr_reg(reg_buf,
                         JMDEC_DEC_OUT_CBASE_LSB,
                         buf->res->bo,
                         buf->mwv207_offset_info.luma_size,
                         MWV207_BO_ACC_FLAG_WRITE);

   reg_buf->set_reg(reg_buf, JMDEC_WRITE_MVS_E, 1);

   reg_buf->set_addr_reg_hi(reg_buf,
                            JMDEC_DIR_MV_BASE_MSB,
                            buf->res->bo,
                            buf->mwv207_offset_info.dir_mv_offset,
                            MWV207_BO_ACC_FLAG_WRITE);
   reg_buf->set_addr_reg(reg_buf,
                         JMDEC_DIR_MV_BASE_LSB,
                         buf->res->bo,
                         buf->mwv207_offset_info.dir_mv_offset,
                         MWV207_BO_ACC_FLAG_WRITE);

   uint32_t pic_width_in_cbs = pic_param->pps.sps.pic_width_in_luma_samples
      >> (pic_param->pps.sps.log2_min_luma_coding_block_size_minus3 + 3);
   uint32_t pic_height_in_cbs = pic_param->pps.sps.pic_height_in_luma_samples
      >> (pic_param->pps.sps.log2_min_luma_coding_block_size_minus3 + 3);

   if (hw_feature->dec_stride_support) {

      uint32_t out_w = pic_width_in_cbs
         * (1 << (pic_param->pps.sps.log2_min_luma_coding_block_size_minus3
                  + 3));
      uint32_t out_w_y, out_w_c;
      if (!dec->dec_param.use_video_compressor) {
         out_w_y = out_w_c = out_w =
            NEXT_MULTIPLE(4 * out_w * pixel_width,
                          ALIGN(dec->dec_param.w_align) * 8)
            / 8;
      } else {
         if (hw_feature->rfc_stride_support) {
            out_w_y = NEXT_MULTIPLE(8 * out_w * pixel_width,
                                    ALIGN(dec->dec_param.w_align) * 8)
               >> 6;
            out_w_c = NEXT_MULTIPLE(4 * out_w * pixel_width,
                                    ALIGN(dec->dec_param.w_align) * 8)
               >> 6;
         } else {
            out_w_y = out_w_c = 4 * out_w * pixel_width / 8;
         }
      }
      reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_Y_STRIDE, out_w_y);
      reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_C_STRIDE, out_w_c);
   }

   if (IS_G2_LEGACY(dec->dec_param.asic_id)) {
      uint32_t rs_pixel_width = dec->dec_param.use_8bits_output
         ? 8
         : dec->dec_param.bit_depth == 10
            ? (dec->dec_param.use_p010_output ? 16 : 10)
            : 8;
      uint32_t rw = pic_width_in_cbs
         * (1 << (pic_param->pps.sps.log2_min_luma_coding_block_size_minus3
                  + 3));
      rw = NEXT_MULTIPLE(rw * rs_pixel_width, 16 * 8) / 8;
      uint32_t rh = pic_height_in_cbs
         * (1 << (pic_param->pps.sps.log2_min_luma_coding_block_size_minus3
                  + 3));

      reg_buf->set_addr_reg_hi(reg_buf,
                               JMDEC_DEC_RSY_BASE_MSB,
                               buf->res->bo,
                               0,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf,
                            JMDEC_DEC_RSY_BASE_LSB,
                            buf->res->bo,
                            0,
                            MWV207_BO_ACC_FLAG_WRITE);

      reg_buf->set_addr_reg_hi(reg_buf,
                               JMDEC_DEC_RSC_BASE_MSB,
                               buf->res->bo,
                               rw * rh,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf,
                            JMDEC_DEC_RSC_BASE_LSB,
                            buf->res->bo,
                            rw * rh,
                            MWV207_BO_ACC_FLAG_WRITE);
   }

   if (dec->dec_param.use_video_compressor) {

      reg_buf->set_addr_reg_hi(reg_buf,
                               JMDEC_DEC_OUT_TYBASE_MSB,
                               buf_res_y->bo,
                               0,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf,
                            JMDEC_DEC_OUT_TYBASE_LSB,
                            buf_res_y->bo,
                            0,
                            MWV207_BO_ACC_FLAG_WRITE);

      reg_buf->set_addr_reg_hi(reg_buf,
                               JMDEC_DEC_OUT_TCBASE_MSB,
                               buf_res_uv->bo,
                               0,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf,
                            JMDEC_DEC_OUT_TCBASE_LSB,
                            buf_res_uv->bo,
                            0,
                            MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_EC_BYPASS, 0);
   } else {
      
      
      reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_EC_BYPASS, 1);
   }
}

static void
dec_hevc_ref_list_init(struct mwv207_video_decoder *decoder,
                       struct mwv207_h265_picture_desc *pic_param,
                       struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;

   int i;
   dec->num_refs = 0;
   for (i = 0; i < 16; i++) {
      if (pic_param->ref_frames[i].picture) {
         dec->ref_frames_h265[i].picture = pic_param->ref_frames[i].picture;
         dec->ref_frames_h265[i].PicOrderCntVal =
            pic_param->ref_frames[i].PicOrderCntVal;
         dec->ref_frames_h265[i].is_long_term = pic_param->IsLongTerm[i];
         dec->num_refs++;
      } else {
         dec->ref_frames_h265[i].picture = target;
         dec->ref_frames_h265[i].PicOrderCntVal = 0;
      }
   }
}

static int32_t
CompareDeltaPoc(const void *p1, const void *p2)
{
   return (((struct RefPicDeltaPoc *)p1)->delta_poc
           - ((struct RefPicDeltaPoc *)p2)->delta_poc);
}

static void
dec_hevc_set_rpl_reg(struct mwv207_video_decoder *decoder,
                     struct mwv207_h265_picture_desc *pic_param,
                     struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;

   dec_hevc_ref_list_init(decoder, pic_param, target);

   int i, j;

   unsigned int long_term_flags = 0;
   uint32_t dpb_size = pic_param->pps.sps.sps_max_dec_pic_buffering_minus1 + 1;
   
   if (dpb_size > 15) {
      MWV207_LOG(
         MWV207_LEVEL_WARNING,
         "Sps assgin %d dpb, but va pic_param only have 15 reference info\n",
         dpb_size);
      dpb_size = 15;
   }

   for (i = 0; i < dpb_size; i++) {
      struct mwv207_picture_h265 ref_surface = dec->ref_frames_h265[i];
      struct mwv207_video_buffer *buf =
         (struct mwv207_video_buffer *)ref_surface.picture;

      reg_buf->set_addr_reg_hi(reg_buf,
                               ref_ybase_msb[i],
                               buf->res->bo,
                               0,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(
         reg_buf, ref_ybase[i], buf->res->bo, 0, MWV207_BO_ACC_FLAG_WRITE);

      reg_buf->set_addr_reg_hi(reg_buf,
                               ref_cbase_msb[i],
                               buf->res->bo,
                               buf->mwv207_offset_info.luma_size,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf,
                            ref_cbase[i],
                            buf->res->bo,
                            buf->mwv207_offset_info.luma_size,
                            MWV207_BO_ACC_FLAG_WRITE);

      long_term_flags |= ((ref_surface.is_long_term) != 0) << (15 - i);

      reg_buf->set_addr_reg_hi(reg_buf,
                               ref_dbase_msb[i],
                               buf->res->bo,
                               buf->mwv207_offset_info.dir_mv_offset,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf,
                            ref_dbase[i],
                            buf->res->bo,
                            buf->mwv207_offset_info.dir_mv_offset,
                            MWV207_BO_ACC_FLAG_WRITE);
   }

   reg_buf->set_reg(reg_buf, JMDEC_REFER_LTERM_E, long_term_flags);

   int8_t poc_diff;
   int32_t pic_order_cnt;
   for (i = 0; i < 15; i++) {
      pic_order_cnt = dec->ref_frames_h265[i].PicOrderCntVal;
      poc_diff =
         CLIP3(-128, 127, (int32_t)pic_param->CurrPicOrderCntVal - pic_order_cnt);
      reg_buf->set_reg(reg_buf, ref_poc_regs[i], poc_diff);
   }

   uint8_t list0[16];
   uint8_t list1[16];
   memset(list0, 0, sizeof(list0));
   memset(list1, 0, sizeof(list1));

   struct RefPicDeltaPoc poc[15];
   uint32_t num_st_before, num_st_after, num_lt;
   num_st_before = num_st_after = num_lt = 0;

   int num_refs = 0;
   int tot_refs = 0;
   for (i = 0; i < 15; i++) {
      poc[i].delta_poc = dec->ref_frames_h265[i].PicOrderCntVal
         - pic_param->CurrPicOrderCntVal;
      poc[i].index = i;
   }
   num_st_before = pic_param->NumPocStCurrBefore;
   num_st_after = pic_param->NumPocStCurrAfter;
   num_lt = pic_param->NumPocLtCurr;
   tot_refs = dec->num_refs;
   
   num_refs = pic_param->NumPocStCurrBefore + pic_param->NumPocStCurrAfter
      + pic_param->NumPocLtCurr;
   
   
   
   
   

   reg_buf->set_reg(reg_buf, JMDEC_REF_FRAMES, num_refs);

   qsort(poc, tot_refs, sizeof(struct RefPicDeltaPoc), CompareDeltaPoc);

   j = 0;

   for (i = pic_param->NumPocStCurrBefore - 1; i >= 0; i--) {
      if (j < 16)
         list0[j++] = poc[i].index;
   }
   for (i = 0 + pic_param->NumPocStCurrBefore;
        i < num_st_before + num_st_after;
        i++) {
      if (j < 16)
         list0[j++] = poc[i].index;
   }
   for (i = 0 + pic_param->NumPocStCurrBefore + pic_param->NumPocStCurrAfter;
        i < tot_refs;
        i++) {
      if (pic_param->RefPicSetLtCurr[i - 1] == i) {
         if (j < 16)
            list0[j++] = poc[i].index;
      }
   }

   j = 0;
   for (i = num_st_before; i < num_st_before + num_st_after; i++) {
      if (j < 16 && i < 16)
         list1[j++] = list0[i];
   }
   for (i = 0; i < num_st_before; i++) {
      if (j < 16 && i < 16)
         list1[j++] = list0[i];
   }
   for (i = num_st_before + num_st_after; i < num_refs; i++) {
      if (j < 16 && i < 16)
         list1[j++] = list0[i];
   }

   j = num_refs;
   i = 0;
   while (j < 16)
      list0[j++] = list0[i++];
   j = num_refs;
   i = 0;
   while (j < 16)
      list1[j++] = list1[i++];

   for (i = 0; i < 16; i++) {
      reg_buf->set_reg(reg_buf, ref_pic_list0[i], list0[i]);
      reg_buf->set_reg(reg_buf, ref_pic_list1[i], list1[i]);
   }
}

static void
dec_hevc_check_and_alloc_asic_buffer(
   struct mwv207_video_decoder *decoder,
   struct mwv207_h265_picture_desc *pic_param,
   struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_buffer *buf = (struct mwv207_video_buffer *)target;

   
   uint32_t pic_height = pic_param->pps.sps.bit_depth_chroma_minus8;
   uint32_t num_tile_cols = pic_param->pps.num_tile_columns_minus1 + 1;
   uint32_t pixel_width = (pic_param->pps.sps.bit_depth_luma_minus8 == 0
                      && pic_param->pps.sps.bit_depth_chroma_minus8 == 0)
      ? 8
      : 10;

   uint32_t height64 = (pic_height + 63) & ~63;
   uint32_t tile_buffer_size = ASIC_VERT_FILTER_RAM_SIZE * height64
         * (num_tile_cols - 1) * pixel_width / 8
      + ASIC_BSD_CTRL_RAM_SIZE * height64 * (num_tile_cols - 1)
      + ASIC_VERT_SAO_RAM_SIZE * height64 * (num_tile_cols - 1) * pixel_width
         / 8;

   if (num_tile_cols > 1) {
      dec->dec_param.filter_mem_offset = 0;
      dec->dec_param.bsd_control_mem_offset =
         dec->dec_param.filter_mem_offset
         +ASIC_VERT_FILTER_RAM_SIZE *height64 *(num_tile_cols - 1) * pixel_width/ 8;
      dec->dec_param.sao_mem_offset = dec->dec_param.bsd_control_mem_offset
         + ASIC_BSD_CTRL_RAM_SIZE * height64 * (num_tile_cols - 1);

      if (!dec->tile_edge) {
         dec->tile_edge = mwv207_video_buffer_create_2(
            decoder->base.context, tile_buffer_size, PIPE_USAGE_STAGING);
      }
   }

   uint32_t misc_buffer_size = 0;
   dec->dec_param.tile_info_offset = ASIC_SCALING_LIST_SIZE;
   misc_buffer_size = ASIC_SCALING_LIST_SIZE;
   misc_buffer_size +=
      (MAX_TILE_COLS_HEVC_DEC * MAX_TILE_ROWS_HEVC_DEC * 4 * sizeof(uint16_t) + 15
       + 16)
      & ~0xF;
   dec->dec_param.fake_table_constant_offset = misc_buffer_size;
   misc_buffer_size += buf->mwv207_offset_info.fake_tbly_size
      + buf->mwv207_offset_info.fake_tblc_size;
   if (!dec->misc_buffer) {
      dec->misc_buffer = mwv207_video_buffer_create_2(
         decoder->base.context, misc_buffer_size, PIPE_USAGE_STAGING);
      dec->misc_buf =
         decoder->ws->bo_map(decoder->ws, dec->misc_buffer->res->bo);
   }
}

static void
dec_hevc_set_misc_value_reg(struct mwv207_video_decoder *decoder,
                            struct mwv207_h265_picture_desc *pic_param,
                            struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   int i = 0, j = 0, k = 0;

   int16_t *p =
      (int16_t *)((uint8_t *)dec->misc_buf + dec->dec_param.tile_info_offset);
   if (pic_param->pps.tiles_enabled_flag) {
      
      
      
      if (pic_param->pps.num_tile_rows_minus1 > 20
          || pic_param->pps.num_tile_columns_minus1 > 18) {
         MWV207_LOG(MWV207_LEVEL_WARNING,
                   "passed pic_parms larger than array size 19 or 21, row "
                   "%d,columns %d\n",
                   pic_param->pps.num_tile_rows_minus1,
                   pic_param->pps.num_tile_columns_minus1);
      }
      for (i = 0; i <= pic_param->pps.num_tile_rows_minus1; i++)
         for (j = 0; j <= pic_param->pps.num_tile_columns_minus1; j++) {
            *p++ = pic_param->pps.column_width_minus1[j] + 1;
            *p++ = pic_param->pps.row_height_minus1[i] + 1;
         }
   } else {
      uint32_t ctb_size =
         (1
          << (pic_param->pps.sps.log2_min_luma_coding_block_size_minus3 + 3
              + pic_param->pps.sps.log2_diff_max_min_luma_coding_block_size));
      uint32_t pic_width_in_ctbs =
         NEXT_MULTIPLE(pic_param->pps.sps.pic_width_in_luma_samples, ctb_size)
         / ctb_size;
      uint32_t pic_height_in_ctbs =
         NEXT_MULTIPLE(pic_param->pps.sps.pic_height_in_luma_samples,
                       ctb_size)
         / ctb_size;
      p[0] = pic_width_in_ctbs;
      p[1] = pic_height_in_ctbs;
   }

   if (pic_param->pps.sps.scaling_list_enabled_flag) {
      uint8_t *p;

      p = (uint8_t *)(int16_t *)((uint8_t *)dec->misc_buf);

      /* dc coeffs of 16x16 and 32x32 lists, stored after 16 coeffs of first
       * 4x4 list */
      for (i = 0; i < 6; i++)
         *p++ = pic_param->pps.sps.ScalingListDCCoeff16x16[i];
      for (i = 0; i < 2; i++)
         *p++ = pic_param->pps.sps.ScalingListDCCoeff32x32[i];

      p += 8;

      for (i = 0; i < 6; i++) {
         for (j = 0; j < 4; j++) {
            for (k = 0; k < 4; k++)
               *p++ = pic_param->pps.sps.ScalingList4x4[i][4 * k + j];
         }
      }

      for (i = 0; i < 6; i++) {
         for (j = 0; j < 8; j++) {
            for (k = 0; k < 8; k++)
               *p++ = pic_param->pps.sps.ScalingList8x8[i][8 * k + j];
         }
      }

      for (i = 0; i < 6; i++) {
         for (j = 0; j < 8; j++) {
            for (k = 0; k < 8; k++)
               *p++ = pic_param->pps.sps.ScalingList16x16[i][8 * k + j];
         }
      }

      for (i = 0; i < 2; i++) {
         for (j = 0; j < 8; j++) {
            for (k = 0; k < 8; k++)
               *p++ = pic_param->pps.sps.ScalingList32x32[i][8 * k + j];
         }
      }
   }
}

static void
dec_hevc_set_stream_data_reg(struct mwv207_video_decoder *decoder,
                             struct mwv207_h265_picture_desc *pic_param,
                             struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;
   struct mwv207_video_buffer *stream_buf =
      decoder->bs_buffers[decoder->cur_buffer];
   struct mwv207_resource *stream_buf_res = stream_buf->res;

   reg_buf->set_reg(reg_buf, JMDEC_START_CODE_E, 1);
   uint32_t tmp = 0 /* (stream_buffer.bus_address & DEC_HW_ALIGN_MASK) * 8 */;
   reg_buf->set_reg(reg_buf, JMDEC_STRM_START_BIT, tmp);

   reg_buf->set_addr_reg_hi(reg_buf,
                            JMDEC_STREAM_BASE_MSB,
                            stream_buf_res->bo,
                            0,
                            MWV207_BO_ACC_FLAG_WRITE);
   reg_buf->set_addr_reg(reg_buf,
                         JMDEC_STREAM_BASE_LSB,
                         stream_buf_res->bo,
                         0,
                         MWV207_BO_ACC_FLAG_WRITE);

   reg_buf->set_reg(reg_buf, JMDEC_STRM_START_OFFSET, 0);
   reg_buf->set_reg(reg_buf, JMDEC_STREAM_LEN, decoder->bs_size);
   reg_buf->set_reg(reg_buf, JMDEC_STRM_BUFFER_LEN, stream_buf_res->size);
}

void
mwv207_dec_hevc_set_regs(struct mwv207_video_decoder *decoder,
                             struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;
   struct mwv207_h265_picture_desc *pic_param = &decoder->picture_desc.h265;
   mwv207_video_dec_get_hw_features_by_id(0x00001F76,
                                         &dec->hw_feature);

   dec->dec_param.pp_enabled = 1;
   dec->dec_param.w_align = 6;
   dec->dec_param.use_video_compressor = 0;
   dec->dec_param.allow_dpb_field_ordering = 1;
   pic_param->cur_pic = (struct mwv207_video_buffer *)target;
   dec->dec_param.bit_depth =
      (pic_param->pps.sps.bit_depth_luma_minus8 == 0
       && pic_param->pps.sps.bit_depth_chroma_minus8 == 0)
      ? 8
      : 10;
   if (dec->dec_param.bit_depth > 8)
      dec->dec_param.use_p010_output = 1;

   dec_hevc_calc_sur_offset(decoder, pic_param, target);
   dec_hevc_set_pp_reg(decoder, pic_param, target);
   reg_buf->set_reg(reg_buf, JMDEC_DEC_MODE, DEC_MODE_HEVC);

   mwv207_dec_set_common_config_regs(dec);
   dec_hevc_set_picparam_reg(decoder, pic_param);
   reg_buf->set_reg(reg_buf, JMDEC_HDR_SKIP_LENGTH, 0);
   reg_buf->set_reg(reg_buf, JMDEC_APF_ONE_PID, 0);
   reg_buf->set_reg(reg_buf, JMDEC_DEC_MULTICORE_E, 0);
   reg_buf->set_reg(reg_buf, JMDEC_DEC_WRITESTAT_E, 0);
   dec_hevc_set_skip_length_reg(decoder, pic_param);
   dec_hevc_set_out_reg(decoder, pic_param, target);
   dec_hevc_set_rpl_reg(decoder, pic_param, target);
   dec_hevc_check_and_alloc_asic_buffer(decoder, pic_param, target);

   dec_hevc_set_misc_value_reg(decoder, pic_param, target);
   dec_hevc_set_stream_data_reg(decoder, pic_param, target);

   reg_buf->set_addr_reg_hi(reg_buf,
                            JMDEC_TILE_BASE_MSB,
                            dec->misc_buffer->res->bo,
                            dec->dec_param.tile_info_offset,
                            MWV207_BO_ACC_FLAG_WRITE);
   reg_buf->set_addr_reg(reg_buf,
                         JMDEC_TILE_BASE_LSB,
                         dec->misc_buffer->res->bo,
                         dec->dec_param.tile_info_offset,
                         MWV207_BO_ACC_FLAG_WRITE);

   if (pic_param->pps.sps.scaling_list_enabled_flag) {

      reg_buf->set_addr_reg_hi(reg_buf,
                               JMDEC_SCALE_LIST_BASE_MSB,
                               dec->misc_buffer->res->bo,
                               0,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf,
                            JMDEC_SCALE_LIST_BASE_LSB,
                            dec->misc_buffer->res->bo,
                            0,
                            MWV207_BO_ACC_FLAG_WRITE);
   }

   if (dec->tile_edge && dec->tile_edge->res) {

      reg_buf->set_addr_reg_hi(reg_buf,
                               JMDEC_DEC_VERT_FILT_BASE_MSB,
                               dec->tile_edge->res->bo,
                               dec->dec_param.filter_mem_offset,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf,
                            JMDEC_DEC_VERT_FILT_BASE_LSB,
                            dec->tile_edge->res->bo,
                            dec->dec_param.filter_mem_offset,
                            MWV207_BO_ACC_FLAG_WRITE);

      reg_buf->set_addr_reg_hi(reg_buf,
                               JMDEC_DEC_VERT_SAO_BASE_MSB,
                               dec->tile_edge->res->bo,
                               dec->dec_param.sao_mem_offset,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf,
                            JMDEC_DEC_VERT_SAO_BASE_LSB,
                            dec->tile_edge->res->bo,
                            dec->dec_param.sao_mem_offset,
                            MWV207_BO_ACC_FLAG_WRITE);

      reg_buf->set_addr_reg_hi(reg_buf,
                               JMDEC_DEC_BSD_CTRL_BASE_MSB,
                               dec->tile_edge->res->bo,
                               dec->dec_param.bsd_control_mem_offset,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf,
                            JMDEC_DEC_BSD_CTRL_BASE_LSB,
                            dec->tile_edge->res->bo,
                            dec->dec_param.bsd_control_mem_offset,
                            MWV207_BO_ACC_FLAG_WRITE);
   }

   reg_buf->set_reg(
      reg_buf, JMDEC_TILE_ENABLE, pic_param->pps.tiles_enabled_flag);
   reg_buf->set_reg(reg_buf, JMDEC_DEC_E, 1);

   
}

