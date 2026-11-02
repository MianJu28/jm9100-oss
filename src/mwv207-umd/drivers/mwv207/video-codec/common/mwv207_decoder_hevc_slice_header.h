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

#ifndef HEVC_SLICE_HEADER_H_
#define HEVC_SLICE_HEADER_H_

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// #include <va/va.h>
// #include <va/va_dec_hevc.h>

#include <stdint.h>
#include "mwv207_decoder_sw_stream.h"
#include "mwv207_video_state.h"

enum {
   HEVC_B_SLICE = 0,
   HEVC_P_SLICE = 1,
   HEVC_I_SLICE = 2,
};

#define MAX_NUM_LT_REF_PICS_SPS 32
#define MAX_NUM_ST_REF_PIC_SETS 64
#define MAX_DPB_SIZE 16
struct mwv207_st_ref_pic_elem {
   int32_t delta_poc;
   uint32_t used_by_curr_pic;
};
struct mwv207_st_ref_pic_set {
   uint32_t num_negative_pics;
   uint32_t num_positive_pics;
   struct mwv207_st_ref_pic_elem elem[MAX_DPB_SIZE];
};
struct mwv207_hevc_slice_header {
   uint32_t no_output_of_prior_pics_flag;
   uint32_t pic_parameter_set_id;
   uint32_t slice_address;
   uint32_t dependent_slice_flag;
   uint32_t slice_type;
   uint32_t pic_output_flag;

   uint32_t pic_order_cnt_lsb;
   uint32_t short_term_ref_pic_set_sps_flag;
   uint32_t short_term_ref_pic_set_idx;
   struct mwv207_st_ref_pic_set st_ref_pic_set[MAX_NUM_ST_REF_PIC_SETS + 1];
   uint32_t num_long_term_sps;
   uint32_t num_long_term_pics;
   uint32_t lt_idx_sps[MAX_NUM_LT_REF_PICS_SPS];
   uint32_t poc_lsb_lt[MAX_NUM_LT_REF_PICS_SPS];
   uint32_t used_by_curr_pic_lt[MAX_NUM_LT_REF_PICS_SPS];
   uint32_t delta_poc_msb_present_flag[MAX_NUM_LT_REF_PICS_SPS];
   uint32_t delta_poc_msb_cycle_lt[MAX_NUM_LT_REF_PICS_SPS];

   uint32_t temporal_mvp_enable_flag;
   uint32_t num_ref_idx_l0_active;
   uint32_t num_ref_idx_l1_active;
   uint32_t hw_skip_bits;
   uint32_t rpl_modification_flag[2];
   uint32_t list_entry_l0[16];
   uint32_t list_entry_l1[16];
};

#define IS_HEVC_P_SLICE(slice_type)                                          \
   (((slice_type) == HEVC_P_SLICE) || ((slice_type) == HEVC_P_SLICE + 5))

#define IS_HEVC_I_SLICE(slice_type)                                          \
   (((slice_type) == HEVC_I_SLICE) || ((slice_type) == HEVC_I_SLICE + 5))

#define IS_HEVC_B_SLICE(slice_type)                                          \
   (((slice_type) == HEVC_B_SLICE) || ((slice_type) == HEVC_B_SLICE + 5))

uint32_t 
mwv207_hevc_decode_slice_header(
   struct mwv207_strm_data *stream,
   struct mwv207_h265_picture_desc *pic_param,
   uint32_t *hw_skip_bits,
   struct mwv207_hevc_slice_header *slice_header,
   uint32_t st_rps_bits /* , VASliceParameterBufferHEVC *slice_param */);

#endif

