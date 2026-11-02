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

#ifndef MWV207_VIDEO_INFO_H
#define MWV207_VIDEO_INFO_H
#include <stdint.h>

enum mwv207_video_format {
   MWV207_VIDEO_FORMAT_UNKNOWN = 0,
   MWV207_VIDEO_FORMAT_MPEG12, /**< MPEG1, MPEG2 */
   MWV207_VIDEO_FORMAT_MPEG4, /**< DIVX, XVID */
   MWV207_VIDEO_FORMAT_VC1, /**< WMV */
   MWV207_VIDEO_FORMAT_MPEG4_AVC, /**< H.264 */
   MWV207_VIDEO_FORMAT_HEVC, /**< H.265 */
   MWV207_VIDEO_FORMAT_JPEG, /**< JPEG */
   MWV207_VIDEO_FORMAT_VP9 /**< VP9 */
};

#define MWV207_VIDEO_BANK_WIDTH(x) ((x) << 0)
#define MWV207_VIDEO_BANK_HEIGHT(x) ((x) << 3)
#define MWV207_VIDEO_MACRO_TILE_ASPECT_RATIO(x) ((x) << 6)
#define MWV207_VIDEO_NUM_BANKS(x) ((x) << 9)

#define MWV207_VIDEO_H264_PROFILE_BASELINE 0x00000000
#define MWV207_VIDEO_H264_PROFILE_MAIN 0x00000001
#define MWV207_VIDEO_H264_PROFILE_HIGH 0x00000002
#define MWV207_VIDEO_H264_PROFILE_STEREO_HIGH 0x00000003
#define MWV207_VIDEO_H264_PROFILE_MVC 0x00000004

struct mwv207_video_mvc_element {
   uint16_t viewOrderIndex;
   uint16_t viewId;
   uint16_t numOfAnchorRefsInL0;
   uint16_t viewIdOfAnchorRefsInL0[15];
   uint16_t numOfAnchorRefsInL1;
   uint16_t viewIdOfAnchorRefsInL1[15];
   uint16_t numOfNonAnchorRefsInL0;
   uint16_t viewIdOfNonAnchorRefsInL0[15];
   uint16_t numOfNonAnchorRefsInL1;
   uint16_t viewIdOfNonAnchorRefsInL1[15];
};

struct mwv207_video_h264 {
   uint32_t profile;
   uint32_t level;

   uint32_t sps_info_flags;
   uint32_t pps_info_flags;
   uint8_t chroma_format;
   uint8_t bit_depth_luma_minus8;
   uint8_t bit_depth_chroma_minus8;
   uint8_t log2_max_frame_num_minus4;

   uint8_t pic_order_cnt_type;
   uint8_t log2_max_pic_order_cnt_lsb_minus4;
   uint8_t num_ref_frames;
   uint8_t reserved_8bit;

   int8_t pic_init_qp_minus26;
   int8_t pic_init_qs_minus26;
   int8_t chroma_qp_index_offset;
   int8_t second_chroma_qp_index_offset;

   uint8_t num_slice_groups_minus1;
   uint8_t slice_group_map_type;
   uint8_t num_ref_idx_l0_active_minus1;
   uint8_t num_ref_idx_l1_active_minus1;

   uint16_t slice_group_change_rate_minus1;
   uint16_t reserved_16bit_1;

   uint8_t scaling_list_4x4[6][16];
   uint8_t scaling_list_8x8[2][64];

   uint32_t frame_num;
   uint32_t frame_num_list[16];
   int32_t curr_field_order_cnt_list[2];
   int32_t field_order_cnt_list[16][2];

   uint32_t decoded_pic_idx;

   uint32_t curr_pic_ref_frame_num;

   uint8_t ref_frame_list[16];

   uint32_t reserved[122];

   struct {
      uint32_t numViews;
      uint32_t viewId0;
      struct mwv207_video_mvc_element mvcElements[1];
   } mvc;
};

struct mwv207_video_h265 {
   uint32_t sps_info_flags;
   uint32_t pps_info_flags;

   uint8_t chroma_format;
   uint8_t bit_depth_luma_minus8;
   uint8_t bit_depth_chroma_minus8;
   uint8_t log2_max_pic_order_cnt_lsb_minus4;

   uint8_t sps_max_dec_pic_buffering_minus1;
   uint8_t log2_min_luma_coding_block_size_minus3;
   uint8_t log2_diff_max_min_luma_coding_block_size;
   uint8_t log2_min_transform_block_size_minus2;

   uint8_t log2_diff_max_min_transform_block_size;
   uint8_t max_transform_hierarchy_depth_inter;
   uint8_t max_transform_hierarchy_depth_intra;
   uint8_t pcm_sample_bit_depth_luma_minus1;

   uint8_t pcm_sample_bit_depth_chroma_minus1;
   uint8_t log2_min_pcm_luma_coding_block_size_minus3;
   uint8_t log2_diff_max_min_pcm_luma_coding_block_size;
   uint8_t num_extra_slice_header_bits;

   uint8_t num_short_term_ref_pic_sets;
   uint8_t num_long_term_ref_pic_sps;
   uint8_t num_ref_idx_l0_default_active_minus1;
   uint8_t num_ref_idx_l1_default_active_minus1;

   int8_t pps_cb_qp_offset;
   int8_t pps_cr_qp_offset;
   int8_t pps_beta_offset_div2;
   int8_t pps_tc_offset_div2;

   uint8_t diff_cu_qp_delta_depth;
   uint8_t num_tile_columns_minus1;
   uint8_t num_tile_rows_minus1;
   uint8_t log2_parallel_merge_level_minus2;

   uint16_t column_width_minus1[19];
   uint16_t row_height_minus1[21];

   int8_t init_qp_minus26;
   uint8_t num_delta_pocs_ref_rps_idx;
   uint8_t curr_idx;
   uint8_t reserved1;
   int32_t curr_poc;
   uint8_t ref_pic_list[16];
   int32_t poc_list[16];
   uint8_t ref_pic_set_st_curr_before[8];
   uint8_t ref_pic_set_st_curr_after[8];
   uint8_t ref_pic_set_lt_curr[8];

   uint8_t ucScalingListDCCoefSizeID2[6];
   uint8_t ucScalingListDCCoefSizeID3[2];

   uint8_t highestTid;
   uint8_t isNonRef;

   uint8_t p010_mode;
   uint8_t msb_mode;
   uint8_t luma_10to8;
   uint8_t chroma_10to8;
   uint8_t sclr_luma10to8;
   uint8_t sclr_chroma10to8;

   uint8_t direct_reflist[2][15];
};

struct mwv207_video_vc1 {
   uint32_t profile;
   uint32_t level;
   uint32_t sps_info_flags;
   uint32_t pps_info_flags;
   uint32_t pic_structure;
   uint32_t chroma_format;
};

struct mwv207_video_mpeg2 {
   uint32_t decoded_pic_idx;
   uint32_t ref_pic_idx[2];

   uint8_t load_intra_quantiser_matrix;
   uint8_t load_nonintra_quantiser_matrix;
   uint8_t reserved_quantiser_alignement[2];
   uint8_t intra_quantiser_matrix[64];
   uint8_t nonintra_quantiser_matrix[64];

   uint8_t profile_and_level_indication;
   uint8_t chroma_format;

   uint8_t picture_coding_type;

   uint8_t reserved_1;

   uint8_t f_code[2][2];
   uint8_t intra_dc_precision;
   uint8_t pic_structure;
   uint8_t top_field_first;
   uint8_t frame_pred_frame_dct;
   uint8_t concealment_motion_vectors;
   uint8_t q_scale_type;
   uint8_t intra_vlc_format;
   uint8_t alternate_scan;
};

struct mwv207_video_mpeg4 {
   uint32_t decoded_pic_idx;
   uint32_t ref_pic_idx[2];

   uint32_t variant_type;
   uint8_t profile_and_level_indication;

   uint8_t video_object_layer_verid;
   uint8_t video_object_layer_shape;

   uint8_t reserved_1;

   uint16_t video_object_layer_width;
   uint16_t video_object_layer_height;

   uint16_t vop_time_increment_resolution;

   uint16_t reserved_2;

   uint32_t flags;

   uint8_t quant_type;

   uint8_t reserved_3[3];

   uint8_t intra_quant_mat[64];
   uint8_t nonintra_quant_mat[64];

   struct {
      uint8_t sprite_enable;

      uint8_t reserved_4[3];

      uint16_t sprite_width;
      uint16_t sprite_height;
      int16_t sprite_left_coordinate;
      int16_t sprite_top_coordinate;

      uint8_t no_of_sprite_warping_points;
      uint8_t sprite_warping_accuracy;
      uint8_t sprite_brightness_change;
      uint8_t low_latency_sprite_enable;
   } sprite_config;

   struct {
      uint32_t flags;
      uint8_t vol_mode;
      uint8_t reserved_5[3];
   } divx_311_config;
};

#endif

