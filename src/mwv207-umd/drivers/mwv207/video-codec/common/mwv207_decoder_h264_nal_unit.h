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

#ifndef H264HWD_NAL_UNIT_H
#define H264HWD_NAL_UNIT_H

/*------------------------------------------------------------------------------
         1. Include headers
------------------------------------------------------------------------------*/

#include <stdint.h>
#include "mwv207_decoder_sw_stream.h"

/*------------------------------------------------------------------------------
         2. Module defines
------------------------------------------------------------------------------*/

/* macro to determine if NAL unit pointed by p_nal_unit contains an IDR slice
 */
#define IS_IDR_NAL_UNIT(p_nal_unit)                                          \
   ((p_nal_unit)->nal_unit_type == NAL_CODED_SLICE_IDR                       \
    || ((p_nal_unit)->nal_unit_type == NAL_CODED_SLICE_EXT                   \
        && (p_nal_unit)->non_idr_flag == 0))

/*------------------------------------------------------------------------------
         3. Data types
------------------------------------------------------------------------------*/

typedef enum {
   NAL_UNSPECIFIED = 0,
   NAL_CODED_SLICE = 1,
   NAL_CODED_SLICE_DP_A = 2,
   NAL_CODED_SLICE_DP_B = 3,
   NAL_CODED_SLICE_DP_C = 4,
   NAL_CODED_SLICE_IDR = 5,
   NAL_SEI = 6,
   NAL_SEQ_PARAM_SET = 7,
   NAL_PIC_PARAM_SET = 8,
   NAL_ACCESS_UNIT_DELIMITER = 9,
   NAL_END_OF_SEQUENCE = 10,
   NAL_END_OF_STREAM = 11,
   NAL_FILLER_DATA = 12,
   NAL_SPS_EXT = 13,
   NAL_PREFIX = 14,
   NAL_SUBSET_SEQ_PARAM_SET = 15,
   NAL_CODED_SLICE_AUX = 19,
   NAL_CODED_SLICE_EXT = 20,
   NAL_MAX_TYPE_VALUE = 31
} mwv207_h264_nal_unit_type;

typedef struct {
   mwv207_h264_nal_unit_type nal_unit_type;
   uint32_t nal_ref_idc;
   uint32_t svc_extension_flag;
   uint32_t non_idr_flag;
   uint32_t priority_id;
   uint32_t view_id;
   uint32_t temporal_id;
   uint32_t anchor_pic_flag;
   uint32_t inter_view_flag;
} mwv207_h264_nal_unit;

/*------------------------------------------------------------------------------
         4. Function prototypes
------------------------------------------------------------------------------*/

uint32_t 
mwv207_h264_bsd_decode_nal_unit(struct mwv207_strm_data *stream,
                                   mwv207_h264_nal_unit *p_nal_unit);

#endif

