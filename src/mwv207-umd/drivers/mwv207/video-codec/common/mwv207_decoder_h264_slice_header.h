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

#ifndef H264HWD_SLICE_HEADER_H
#define H264HWD_SLICE_HEADER_H

/*------------------------------------------------------------------------------
         1. Include headers
------------------------------------------------------------------------------*/

#include <stdint.h>
#include "mwv207_decoder_h264_nal_unit.h"
#include "mwv207_decoder_sw_stream.h"
#include "mwv207_video_state.h"

/*------------------------------------------------------------------------------
         2. Module defines
------------------------------------------------------------------------------*/

enum { P_SLICE = 0, B_SLICE = 1, I_SLICE = 2, SP_SLICE = 3, SI_SLICE = 4 };

enum { NO_LONG_TERM_FRAME_INDICES = 0xFFFF };

#define IS_P_SLICE(slice_type)                                               \
   (((slice_type) == P_SLICE) || ((slice_type) == P_SLICE + 5))

#define IS_SP_SLICE(slice_type)                                              \
   (((slice_type) == SP_SLICE) || ((slice_type) == SP_SLICE + 5))

#define IS_I_SLICE(slice_type)                                               \
   (((slice_type) == I_SLICE) || ((slice_type) == I_SLICE + 5))

#define IS_B_SLICE(slice_type)                                               \
   (((slice_type) == B_SLICE) || ((slice_type) == B_SLICE + 5))

#define MAX_NUM_REF_PICS 16
#define MAX_NUM_SLICE_GROUPS 8
#define MAX_NUM_SEQ_PARAM_SETS 32
#define MAX_NUM_PIC_PARAM_SETS 256

#define MAX_NUM_VIEWS 2

#define PICT_TOP_FIELD     1
#define PICT_BOTTOM_FIELD  2
#define PICT_FRAME         3

/*------------------------------------------------------------------------------
         3. Data types
------------------------------------------------------------------------------*/

/* structure to store data of one reference picture list reordering operation
 */
typedef struct {
   uint32_t reordering_of_pic_nums_idc;
   uint32_t abs_diff_pic_num;
   uint32_t long_term_pic_num;
   uint32_t abs_diff_view_idx;
} mwv207_ref_pic_list_reordering_operation;

typedef struct {
   uint32_t ref_pic_list_reordering_flag_l0;
   mwv207_ref_pic_list_reordering_operation command[MAX_NUM_REF_PICS + 1];
} mwv207_ref_pic_list_reordering;

typedef struct {
   uint32_t memory_management_control_operation;
   uint32_t difference_of_pic_nums;
   uint32_t long_term_pic_num;
   uint32_t long_term_frame_idx;
   uint32_t max_long_term_frame_idx;
} mwv207_memory_management_operation;

/* worst case scenario: all MAX_NUM_REF_PICS pictures in the buffer are
 * short term pictures, each one of them is first marked as long term
 * reference picture which is then marked as unused for reference.
 * Additionally, max long-term frame index is set and current picture is
 * marked as long term reference picture. Last position reserved for
 * end memory_management_control_operation command */
#define MAX_NUM_MMC_OPERATIONS (2 * MAX_NUM_REF_PICS + 2 + 1)

typedef struct {
   uint32_t strm_len;
   uint32_t no_output_of_prior_pics_flag;
   uint32_t long_term_reference_flag;
   uint32_t adaptive_ref_pic_marking_mode_flag;
   mwv207_memory_management_operation operation[MAX_NUM_MMC_OPERATIONS];
} mwv207_dec_ref_pic_marking;

/**
 * Memory management control operation opcode.
 */
typedef enum MMCOOpcode {
    MMCO_END = 0,
    MMCO_SHORT2UNUSED,
    MMCO_LONG2UNUSED,
    MMCO_SHORT2LONG,
    MMCO_SET_MAX_LONG,
    MMCO_RESET,
    MMCO_LONG,
} MMCOOpcode;

/**
 * Memory management control operation.
 */
typedef struct MMCO {
    MMCOOpcode opcode;
    int short_pic_num;  
    int long_arg;       
} MMCO;
#define MAX_MMCO_COUNT         66

typedef struct {
   uint32_t first_mb_in_slice;
   uint32_t slice_type;
   uint32_t pic_parameter_set_id;
   uint32_t frame_num;
   uint32_t idr_pic_id;
   uint32_t poc_length;
   uint32_t poc_length_hw;
   uint32_t pic_order_cnt_lsb;
   int32_t delta_pic_order_cnt_bottom;
   int32_t delta_pic_order_cnt[2];
   uint32_t redundant_pic_cnt;
   uint32_t num_ref_idx_active_override_flag;
   uint32_t num_ref_idx_l0_active;
   uint32_t num_ref_idx_l1_active;
   int32_t slice_qp_delta;
   uint32_t disable_deblocking_filter_idc;
   int32_t slice_alpha_c0_offset;
   int32_t slice_beta_offset;
   uint32_t slice_group_change_cycle;
   mwv207_ref_pic_list_reordering ref_pic_list_reordering;
   mwv207_ref_pic_list_reordering ref_pic_list_reordering_l1;
   mwv207_dec_ref_pic_marking dec_ref_pic_marking;
   uint32_t cabac_init_idc;
   uint32_t field_pic_flag;
   uint32_t bottom_field_flag;
   uint32_t direct_spatial_mv_pred_flag;

 /**
     * memory management control operations buffer.
     */
    MMCO mmco[MAX_MMCO_COUNT];
    int  nb_mmco;
    int mmco_reset;
    int explicit_ref_marking;  

    int curr_pic_num;
    int max_pic_num;
    int picture_structure;      
} mwv207_h264_slice_header;

/*------------------------------------------------------------------------------
         4. Function prototypes
------------------------------------------------------------------------------*/

uint32_t 
mwv207_h264_bsd_decode_slice_header(
   struct mwv207_strm_data *stream,
   struct mwv207_h264_picture_desc *pic_param,
   //  VAPictureParameterBufferH264 *pic_param,
   //  VASliceParameterBufferH264 *slice_param,
   mwv207_h264_slice_header *p_slice_header,
   mwv207_h264_nal_unit *p_nal_unit);

uint32_t
mwv207_h264_bsd_Ref_idx_override(struct mwv207_strm_data *stream,
                                //   VAPictureParameterBufferH264 *pic_param,
                                struct mwv207_h264_picture_desc *pic_param,
                                uint32_t *num_ref_idx_active_override_flag);

#endif

