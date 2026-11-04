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

#ifndef MWV207_9200_VDIEO_DEC_H
#define MWV207_9200_VDIEO_DEC_H
#include "mwv207_video_dec_reg.h"
#include "mwv207_video_dec_vpufeature.h"
#include <stdint.h>
#include "mwv207_video_reg_buf.h"
#include "pipe/p_video_codec.h"
#include "mwv207_dec_ppu.h"
#include "mwv207_decoder_h264_slice_header.h"

#define DEC_MODE_H264 0
#define DEC_MODE_HEVC 12
#define DEC_MODE_VP9 13

#define DEC_DPB_FRAME 0
#define DEC_DPB_INTERLACED_FIELD 1

#define DEC_MODE_H264_HIGH10P 15

#define EXT_REGION_USE (1)
#define INTER_REGION_USE (2)

#define TILED_REF_NONE (0)
#define TILED_REF_8x4 (1)
#define TILED_REF_8x4_INTERLACED (2)

#define IS_JMDEC(asic_id) (((asic_id) >> 16) == 0x8001)
#define IS_DEC_KMB_A0(asic_id) (((asic_id)&0xffff) == 0x6010)
#define IS_DEC_TBH(asic_id, hw_build_id)                                     \
   ((((hw_build_id)&0xffff) == 0x1F47) || (((hw_build_id)&0xffff) == 0x1F48) \
    || (((hw_build_id)&0xffff) == 0x1F49)                                    \
    || (((hw_build_id)&0xffff) == 0x1F4A))
#define IS_DEC_KMB_B0(asic_id, hw_build_id)                                  \
   ((((hw_build_id)&0xffff) == 0x1F51) || (((hw_build_id)&0xffff) == 0x1F50))
#define IS_DEC_KMB_B0_B(asic_id, hw_build_id)                                \
   (((hw_build_id)&0xffff) == 0x1F51)
#define IS_DEC_OYB(asic_id, hw_build_id) (((hw_build_id)&0xffff) == 0x1F66)

struct mwv207_video_dec_param {
   int tiled_stride_enable;
   int use_video_compressor;

   uint32_t high10p_mode;

   int w_align;
   int h_align;

   uint32_t rlc_mode;
   uint32_t n_cores;
   uint32_t bit_depth;
   uint32_t mc_enabled;
   uint32_t cr_first;
   uint32_t pp_enabled;

   uint32_t asic_id;
   uint32_t hw_build_id;

   uint32_t mono_chrome;
   int dpb_mode;
   uint32_t allow_dpb_field_ordering;

   uint32_t scaling_matrix_present_flag;

   uint32_t use_8bits_output;
   uint32_t use_p010_output;
   uint32_t use_bige_output;

   int tile_info_offset;
   int fake_table_constant_offset;
   int filter_mem_offset;
   int sao_mem_offset;
   int bsd_control_mem_offset;
};

typedef struct H264POCContext {
    int poc_lsb;
    int poc_msb;
    int delta_poc_bottom;
    int delta_poc[2];
    int frame_num;
    int prev_poc_msb;           
    int prev_poc_lsb;           
    int frame_num_offset;       
    int prev_frame_num_offset;  
    int prev_frame_num;         //< frame_num of the last pic for POC type 1/2
} H264POCContext;

#ifndef MAX_CABAC_BUF_COUNT
#define MAX_CABAC_BUF_COUNT 4
#endif

struct mwv207_video_dec {
   struct mwv207_video_dec_param dec_param;
   struct mwv207_video_dec_hw_features hw_feature;
   struct mwv207_video_reg_buf *reg_buf;

   mwv207_pp_unit_int_config ppu_cfg[DEC_MAX_PPU_COUNT];

   struct mwv207_video_buffer *cabac_buffer[MAX_CABAC_BUF_COUNT];
   int current_cabac_buf_index;
   void *cabac_buf;

   mwv207_h264_slice_header slice_header;

   uint32_t list0[34];
   uint32_t list1[34];
   uint32_t list_p[34];
   uint32_t num_refs;

   struct mwv207_picture_h264 ref_frames[16];

   struct mwv207_video_buffer *tile_edge;
   struct mwv207_video_buffer *misc_buffer;
   void *misc_buf;
   struct mwv207_picture_h265 ref_frames_h265[16];

   void *vp9_state;

   H264POCContext poc;

   struct mwv207_picture_h264 short_ref[32];
   struct mwv207_picture_h264 long_ref[32];  
   int long_ref_count;     
   int short_ref_count;    
};

struct mwv207_video_dec *
mwv207_video_dec_create(struct mwv207_winsys *ws);
void mwv207_video_dec_destroy_buffer(struct mwv207_video_dec *dec);
void mwv207_video_dec_destroy(struct mwv207_video_dec *dec);

void
mwv207_dec_set_common_config_regs(struct mwv207_video_dec *dec);
int mwv207_dec_pp_params_init(struct mwv207_video_dec *dec,
                             mwv207_pp_unit_config *ppu_ext_cfg,
                             uint32_t bit_depth,
                             struct pipe_video_buffer *target);

#endif

