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

#ifndef MWV207_VIDEO_STATE_H
#define MWV207_VIDEO_STATE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "mwv207_video_enums.h"
#include <stdint.h>
#include <stdbool.h>

/*
 * see table 6-12 in the spec
 */
enum mwv207_mpeg12_picture_coding_type {
   MWV207_MPEG12_PICTURE_CODING_TYPE_I = 0x01,
   MWV207_MPEG12_PICTURE_CODING_TYPE_P = 0x02,
   MWV207_MPEG12_PICTURE_CODING_TYPE_B = 0x03,
   MWV207_MPEG12_PICTURE_CODING_TYPE_D = 0x04
};

/*
 * see table 6-14 in the spec
 */
enum mwv207_mpeg12_picture_structure {
   MWV207_MPEG12_PICTURE_STRUCTURE_RESERVED = 0x00,
   MWV207_MPEG12_PICTURE_STRUCTURE_FIELD_TOP = 0x01,
   MWV207_MPEG12_PICTURE_STRUCTURE_FIELD_BOTTOM = 0x02,
   MWV207_MPEG12_PICTURE_STRUCTURE_FRAME = 0x03
};

/*
 * flags for macroblock_type, see section 6.3.17.1 in the spec
 */
enum mwv207_mpeg12_macroblock_type {
   MWV207_MPEG12_MB_TYPE_QUANT = 0x01,
   MWV207_MPEG12_MB_TYPE_MOTION_FORWARD = 0x02,
   MWV207_MPEG12_MB_TYPE_MOTION_BACKWARD = 0x04,
   MWV207_MPEG12_MB_TYPE_PATTERN = 0x08,
   MWV207_MPEG12_MB_TYPE_INTRA = 0x10
};

/*
 * flags for motion_type, see table 6-17 and 6-18 in the spec
 */
enum mwv207_mpeg12_motion_type {
   MWV207_MPEG12_MO_TYPE_RESERVED = 0x00,
   MWV207_MPEG12_MO_TYPE_FIELD = 0x01,
   MWV207_MPEG12_MO_TYPE_FRAME = 0x02,
   MWV207_MPEG12_MO_TYPE_16x8 = 0x02,
   MWV207_MPEG12_MO_TYPE_DUAL_PRIME = 0x03
};

/*
 * see section 6.3.17.1 and table 6-19 in the spec
 */
enum mwv207_mpeg12_dct_type {
   MWV207_MPEG12_DCT_TYPE_FRAME = 0,
   MWV207_MPEG12_DCT_TYPE_FIELD = 1
};

enum mwv207_mpeg12_field_select {
   MWV207_MPEG12_FS_FIRST_FORWARD = 0x01,
   MWV207_MPEG12_FS_FIRST_BACKWARD = 0x02,
   MWV207_MPEG12_FS_SECOND_FORWARD = 0x04,
   MWV207_MPEG12_FS_SECOND_BACKWARD = 0x08
};

enum mwv207_h264_slice_type {
   MWV207_H264_SLICE_TYPE_P = 0x0,
   MWV207_H264_SLICE_TYPE_B = 0x1,
   MWV207_H264_SLICE_TYPE_I = 0x2,
   MWV207_H264_SLICE_TYPE_SP = 0x3,
   MWV207_H264_SLICE_TYPE_SI = 0x4
};

enum mwv207_h264_enc_picture_type {
   MWV207_H264_ENC_PICTURE_TYPE_P = 0x00,
   MWV207_H264_ENC_PICTURE_TYPE_B = 0x01,
   MWV207_H264_ENC_PICTURE_TYPE_I = 0x02,
   MWV207_H264_ENC_PICTURE_TYPE_IDR = 0x03,
   MWV207_H264_ENC_PICTURE_TYPE_SKIP = 0x04
};

enum mwv207_h265_enc_picture_type {
   MWV207_H265_ENC_PICTURE_TYPE_P = 0x00,
   MWV207_H265_ENC_PICTURE_TYPE_B = 0x01,
   MWV207_H265_ENC_PICTURE_TYPE_I = 0x02,
   MWV207_H265_ENC_PICTURE_TYPE_IDR = 0x03,
   MWV207_H265_ENC_PICTURE_TYPE_SKIP = 0x04
};

enum mwv207_h264_enc_rate_control_method {
   MWV207_H264_ENC_RATE_CONTROL_METHOD_DISABLE = 0x00,
   MWV207_H264_ENC_RATE_CONTROL_METHOD_CONSTANT_SKIP = 0x01,
   MWV207_H264_ENC_RATE_CONTROL_METHOD_VARIABLE_SKIP = 0x02,
   MWV207_H264_ENC_RATE_CONTROL_METHOD_CONSTANT = 0x03,
   MWV207_H264_ENC_RATE_CONTROL_METHOD_VARIABLE = 0x04
};

enum mwv207_h265_enc_rate_control_method {
   MWV207_H265_ENC_RATE_CONTROL_METHOD_DISABLE = 0x00,
   MWV207_H265_ENC_RATE_CONTROL_METHOD_CONSTANT_SKIP = 0x01,
   MWV207_H265_ENC_RATE_CONTROL_METHOD_VARIABLE_SKIP = 0x02,
   MWV207_H265_ENC_RATE_CONTROL_METHOD_CONSTANT = 0x03,
   MWV207_H265_ENC_RATE_CONTROL_METHOD_VARIABLE = 0x04
};

struct mwv207_picture_desc {
   enum mwv207_video_profile profile;
   enum mwv207_video_entrypoint entry_point;
};

struct mwv207_quant_matrix {
   enum mwv207_video_format codec;
};

struct mwv207_macroblock {
   enum mwv207_video_format codec;
};

struct mwv207_mpeg12_picture_desc {
   struct mwv207_picture_desc base;

   unsigned picture_coding_type;
   unsigned picture_structure;
   unsigned frame_pred_frame_dct;
   unsigned q_scale_type;
   unsigned alternate_scan;
   unsigned intra_vlc_format;
   unsigned concealment_motion_vectors;
   unsigned intra_dc_precision;
   unsigned f_code[2][2];
   unsigned top_field_first;
   unsigned full_pel_forward_vector;
   unsigned full_pel_backward_vector;
   unsigned num_slices;

   const uint8_t *intra_matrix;
   const uint8_t *non_intra_matrix;

   struct mwv207_video_buffer *ref[2];
};

struct mwv207_mpeg12_macroblock {
   struct mwv207_macroblock base;

   unsigned short x, y;

   unsigned char macroblock_type;

   union {
      struct {

         unsigned int frame_motion_type : 2;

         unsigned int field_motion_type : 2;

         unsigned int dct_type : 1;
      } bits;
      unsigned int value;
   } macroblock_modes;

   unsigned char motion_vertical_field_select;

   short PMV[2][2][2];

   unsigned short coded_block_pattern;

   short *blocks;

   unsigned short num_skipped_macroblocks;
};

struct mwv207_mpeg4_picture_desc {
   struct mwv207_picture_desc base;

   int32_t trd[2];
   int32_t trb[2];
   uint16_t vop_time_increment_resolution;
   uint8_t vop_coding_type;
   uint8_t vop_fcode_forward;
   uint8_t vop_fcode_backward;
   uint8_t resync_marker_disable;
   uint8_t interlaced;
   uint8_t quant_type;
   uint8_t quarter_sample;
   uint8_t short_video_header;
   uint8_t rounding_control;
   uint8_t alternate_vertical_scan_flag;
   uint8_t top_field_first;

   const uint8_t *intra_matrix;
   const uint8_t *non_intra_matrix;

   struct mwv207_video_buffer *ref[2];
};

struct mwv207_vc1_picture_desc {
   struct mwv207_picture_desc base;

   uint32_t slice_count;
   uint8_t picture_type;
   uint8_t frame_coding_mode;
   uint8_t postprocflag;
   uint8_t pulldown;
   uint8_t interlace;
   uint8_t tfcntrflag;
   uint8_t finterpflag;
   uint8_t psf;
   uint8_t dquant;
   uint8_t panscan_flag;
   uint8_t refdist_flag;
   uint8_t quantizer;
   uint8_t extended_mv;
   uint8_t extended_dmv;
   uint8_t overlap;
   uint8_t vstransform;
   uint8_t loopfilter;
   uint8_t fastuvmc;
   uint8_t range_mapy_flag;
   uint8_t range_mapy;
   uint8_t range_mapuv_flag;
   uint8_t range_mapuv;
   uint8_t multires;
   uint8_t syncmarker;
   uint8_t rangered;
   uint8_t maxbframes;
   uint8_t deblockEnable;
   uint8_t pquant;

   struct mwv207_video_buffer *ref[2];
};

struct mwv207_h264_sps {
   uint8_t level_idc;
   uint8_t chroma_format_idc;
   uint8_t separate_colour_plane_flag;
   uint8_t bit_depth_luma_minus8;
   uint8_t bit_depth_chroma_minus8;
   uint8_t seq_scaling_matrix_present_flag;
   uint8_t ScalingList4x4[6][16];
   uint8_t ScalingList8x8[6][64];
   uint8_t log2_max_frame_num_minus4;
   uint8_t pic_order_cnt_type;
   uint8_t log2_max_pic_order_cnt_lsb_minus4;
   uint8_t delta_pic_order_always_zero_flag;
   int32_t offset_for_non_ref_pic;
   int32_t offset_for_top_to_bottom_field;
   uint8_t num_ref_frames_in_pic_order_cnt_cycle;
   int32_t offset_for_ref_frame[256];
   uint8_t max_num_ref_frames;
   uint8_t frame_mbs_only_flag;
   uint8_t mb_adaptive_frame_field_flag;
   uint8_t direct_8x8_inference_flag;
};

struct mwv207_h264_pps {
   struct mwv207_h264_sps sps;

   uint8_t entropy_coding_mode_flag;
   uint8_t bottom_field_pic_order_in_frame_present_flag;
   uint8_t num_slice_groups_minus1;
   uint8_t slice_group_map_type;
   uint8_t slice_group_change_rate_minus1;
   uint8_t num_ref_idx_l0_default_active_minus1;
   uint8_t num_ref_idx_l1_default_active_minus1;
   uint8_t weighted_pred_flag;
   uint8_t weighted_bipred_idc;
   int8_t pic_init_qp_minus26;
   int8_t chroma_qp_index_offset;
   uint8_t deblocking_filter_control_present_flag;
   uint8_t constrained_intra_pred_flag;
   uint8_t redundant_pic_cnt_present_flag;
   uint8_t ScalingList4x4[6][16];
   uint8_t ScalingList8x8[6][64];
   uint8_t transform_8x8_mode_flag;
   int8_t second_chroma_qp_index_offset;
};

struct mwv207_picture_h264 {
   void *picture;
   int32_t frame_num;
   bool is_long_term;
   bool top_is_reference;
   bool bottom_is_reference;
   int32_t top_field_order_cnt;
   int32_t bottom_field_order_cnt;
};

struct mwv207_h264_picture_desc {
   struct mwv207_picture_desc base;

   struct mwv207_h264_pps pps;

   uint32_t frame_num;
   uint8_t field_pic_flag;
   uint8_t bottom_field_flag;
   uint8_t num_ref_idx_l0_active_minus1;
   uint8_t num_ref_idx_l1_active_minus1;

   uint32_t slice_count;
   int32_t field_order_cnt[2];
   bool is_reference;
   uint8_t num_ref_frames;

   bool is_long_term[16];
   bool top_is_reference[16];
   bool bottom_is_reference[16];
   uint32_t field_order_cnt_list[16][2];
   uint32_t frame_num_list[16];

   struct mwv207_picture_h264 ref_frames[16];
   struct mwv207_video_buffer *cur_pic;

   /*add by zhihong.deng, 2024-07-16, since openmax will not pass ref_frames to us,
   and our hardware need this, so create it myself*/
   struct mwv207_picture_h264 my_ref_frames[16];
   uint8_t num_my_ref_frames;
   uint8_t is_idr;
   int32_t my_field_order_cnt[2];   
};

struct mwv207_h264_enc_rate_control {
   enum mwv207_h264_enc_rate_control_method rate_ctrl_method;
   unsigned target_bitrate;
   unsigned peak_bitrate;
   unsigned frame_rate_num;
   unsigned frame_rate_den;
   unsigned vbv_buffer_size;
   unsigned vbv_buf_lv;
   unsigned target_bits_picture;
   unsigned peak_bits_picture_integer;
   unsigned peak_bits_picture_fraction;
   unsigned fill_data_enable;
   unsigned enforce_hrd;
};

struct mwv207_h264_enc_motion_estimation {
   unsigned motion_est_quarter_pixel;
   unsigned enc_disable_sub_mode;
   unsigned lsmvert;
   unsigned enc_en_ime_overw_dis_subm;
   unsigned enc_ime_overw_dis_subm_no;
   unsigned enc_ime2_search_range_x;
   unsigned enc_ime2_search_range_y;
};

struct mwv207_h264_enc_pic_control {
   unsigned enc_cabac_enable;
   unsigned enc_constraint_set_flags;
   unsigned enc_frame_cropping_flag;
   unsigned enc_frame_crop_left_offset;
   unsigned enc_frame_crop_right_offset;
   unsigned enc_frame_crop_top_offset;
   unsigned enc_frame_crop_bottom_offset;
};

struct mwv207_h264_enc_picture_desc {
   struct mwv207_picture_desc base;

   struct mwv207_h264_enc_rate_control rate_ctrl;

   struct mwv207_h264_enc_motion_estimation motion_est;
   struct mwv207_h264_enc_pic_control pic_ctrl;

   unsigned quant_i_frames;
   unsigned quant_p_frames;
   unsigned quant_b_frames;

   enum mwv207_h264_enc_picture_type picture_type;
   unsigned frame_num;
   unsigned frame_num_cnt;
   unsigned p_remain;
   unsigned i_remain;
   unsigned idr_pic_id;
   unsigned gop_cnt;
   unsigned pic_order_cnt;
   unsigned pic_order_cnt_type;
   unsigned ref_idx_l0;
   unsigned ref_idx_l1;
   unsigned gop_size;
   unsigned ref_pic_mode;

   bool not_referenced;
   bool enable_vui;
   struct util_hash_table *frame_idx;
};

struct mwv207_h265_enc_seq_param {
   uint8_t general_profile_idc;
   uint8_t general_level_idc;
   uint8_t general_tier_flag;
   uint32_t intra_period;
   uint16_t pic_width_in_luma_samples;
   uint16_t pic_height_in_luma_samples;
   uint32_t chroma_format_idc;
   uint32_t bit_depth_luma_minus8;
   uint32_t bit_depth_chroma_minus8;
   bool strong_intra_smoothing_enabled_flag;
   bool amp_enabled_flag;
   bool sample_adaptive_offset_enabled_flag;
   bool pcm_enabled_flag;
   bool sps_temporal_mvp_enabled_flag;
   uint8_t log2_min_luma_coding_block_size_minus3;
   uint8_t log2_diff_max_min_luma_coding_block_size;
   uint8_t log2_min_transform_block_size_minus2;
   uint8_t log2_diff_max_min_transform_block_size;
   uint8_t max_transform_hierarchy_depth_inter;
   uint8_t max_transform_hierarchy_depth_intra;
};

struct mwv207_h265_enc_pic_param {
   uint8_t log2_parallel_merge_level_minus2;
   uint8_t nal_unit_type;
   bool constrained_intra_pred_flag;
};

struct mwv207_h265_enc_slice_param {
   uint8_t max_num_merge_cand;
   int8_t slice_cb_qp_offset;
   int8_t slice_cr_qp_offset;
   int8_t slice_beta_offset_div2;
   int8_t slice_tc_offset_div2;
   bool cabac_init_flag;
   uint32_t slice_deblocking_filter_disabled_flag;
   bool slice_loop_filter_across_slices_enabled_flag;
};

struct mwv207_h265_enc_rate_control {
   enum mwv207_h265_enc_rate_control_method rate_ctrl_method;
   unsigned target_bitrate;
   unsigned peak_bitrate;
   unsigned frame_rate_num;
   unsigned frame_rate_den;
   unsigned quant_i_frames;
   unsigned vbv_buffer_size;
   unsigned vbv_buf_lv;
   unsigned target_bits_picture;
   unsigned peak_bits_picture_integer;
   unsigned peak_bits_picture_fraction;
   unsigned fill_data_enable;
   unsigned enforce_hrd;
};

struct mwv207_h265_enc_picture_desc {
   struct mwv207_picture_desc base;

   struct mwv207_h265_enc_seq_param seq;
   struct mwv207_h265_enc_pic_param pic;
   struct mwv207_h265_enc_slice_param slice;
   struct mwv207_h265_enc_rate_control rc;

   enum mwv207_h265_enc_picture_type picture_type;
   unsigned decoded_curr_pic;
   unsigned reference_frames[16];
   unsigned frame_num;
   unsigned pic_order_cnt;
   unsigned pic_order_cnt_type;
   unsigned ref_idx_l0;
   unsigned ref_idx_l1;
   bool not_referenced;
   struct util_hash_table *frame_idx;
};

struct mwv207_h265_sps {
   uint8_t chroma_format_idc;
   uint8_t separate_colour_plane_flag;
   uint32_t pic_width_in_luma_samples;
   uint32_t pic_height_in_luma_samples;
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
   uint8_t scaling_list_enabled_flag;
   uint8_t ScalingList4x4[6][16];
   uint8_t ScalingList8x8[6][64];
   uint8_t ScalingList16x16[6][64];
   uint8_t ScalingList32x32[2][64];
   uint8_t ScalingListDCCoeff16x16[6];
   uint8_t ScalingListDCCoeff32x32[2];
   uint8_t amp_enabled_flag;
   uint8_t sample_adaptive_offset_enabled_flag;
   uint8_t pcm_enabled_flag;
   uint8_t pcm_sample_bit_depth_luma_minus1;
   uint8_t pcm_sample_bit_depth_chroma_minus1;
   uint8_t log2_min_pcm_luma_coding_block_size_minus3;
   uint8_t log2_diff_max_min_pcm_luma_coding_block_size;
   uint8_t pcm_loop_filter_disabled_flag;
   uint8_t num_short_term_ref_pic_sets;
   uint8_t long_term_ref_pics_present_flag;
   uint8_t num_long_term_ref_pics_sps;
   uint8_t sps_temporal_mvp_enabled_flag;
   uint8_t strong_intra_smoothing_enabled_flag;
};

struct mwv207_h265_pps {
   struct mwv207_h265_sps sps;

   uint8_t dependent_slice_segments_enabled_flag;
   uint8_t output_flag_present_flag;
   uint8_t num_extra_slice_header_bits;
   uint8_t sign_data_hiding_enabled_flag;
   uint8_t cabac_init_present_flag;
   uint8_t num_ref_idx_l0_default_active_minus1;
   uint8_t num_ref_idx_l1_default_active_minus1;
   int8_t init_qp_minus26;
   uint8_t constrained_intra_pred_flag;
   uint8_t transform_skip_enabled_flag;
   uint8_t cu_qp_delta_enabled_flag;
   uint8_t diff_cu_qp_delta_depth;
   int8_t pps_cb_qp_offset;
   int8_t pps_cr_qp_offset;
   uint8_t pps_slice_chroma_qp_offsets_present_flag;
   uint8_t weighted_pred_flag;
   uint8_t weighted_bipred_flag;
   uint8_t transquant_bypass_enabled_flag;
   uint8_t tiles_enabled_flag;
   uint8_t entropy_coding_sync_enabled_flag;
   uint8_t num_tile_columns_minus1;
   uint8_t num_tile_rows_minus1;
   uint8_t uniform_spacing_flag;
   uint16_t column_width_minus1[20];
   uint16_t row_height_minus1[22];
   uint8_t loop_filter_across_tiles_enabled_flag;
   uint8_t pps_loop_filter_across_slices_enabled_flag;
   uint8_t deblocking_filter_control_present_flag;
   uint8_t deblocking_filter_override_enabled_flag;
   uint8_t pps_deblocking_filter_disabled_flag;
   int8_t pps_beta_offset_div2;
   int8_t pps_tc_offset_div2;
   uint8_t lists_modification_present_flag;
   uint8_t log2_parallel_merge_level_minus2;
   uint8_t slice_segment_header_extension_present_flag;
};

struct mwv207_picture_h265 {
   void *picture;
   int32_t frame_idx;
   bool is_long_term;
   bool top_is_reference;
   bool bottom_is_reference;
   int32_t top_field_order_cnt;
   int32_t bottom_field_order_cnt;

   int32_t PicOrderCntVal;
};

struct mwv207_h265_picture_desc {
   struct mwv207_picture_desc base;

   struct mwv207_h265_pps pps;

   uint8_t IDRPicFlag;
   uint8_t RAPPicFlag;
   uint8_t CurrRpsIdx;
   uint32_t NumPocTotalCurr;
   uint32_t NumDeltaPocsOfRefRpsIdx;
   uint32_t NumShortTermPictureSliceHeaderBits;
   uint32_t NumLongTermPictureSliceHeaderBits;

   int32_t CurrPicOrderCntVal;
   int32_t PicOrderCntVal[16];
   uint8_t IsLongTerm[16];
   uint8_t NumPocStCurrBefore;
   uint8_t NumPocStCurrAfter;
   uint8_t NumPocLtCurr;
   uint8_t RefPicSetStCurrBefore[8];
   uint8_t RefPicSetStCurrAfter[8];
   uint8_t RefPicSetLtCurr[8];
   uint8_t RefPicList[2][15];
   bool UseRefPicList;

   uint32_t st_rps_bits;

   struct mwv207_picture_h265 ref_frames[16];
   struct mwv207_video_buffer *cur_pic;
};

struct mwv207_mjpeg_picture_desc {
   struct mwv207_picture_desc base;

   struct {
      uint16_t picture_width;
      uint16_t picture_height;

      struct {
         uint8_t component_id;
         uint8_t h_sampling_factor;
         uint8_t v_sampling_factor;
         uint8_t quantiser_table_selector;
      } components[255];

      uint8_t num_components;
   } picture_parameter;

   struct {
      uint8_t load_quantiser_table[4];
      uint8_t quantiser_table[4][64];
   } quantization_table;

   struct {
      uint8_t load_huffman_table[2];

      struct {
         uint8_t num_dc_codes[16];
         uint8_t dc_values[12];
         uint8_t num_ac_codes[16];
         uint8_t ac_values[162];
         uint8_t pad[2];
      } table[2];
   } huffman_table;

   struct {
      unsigned slice_data_size;
      unsigned slice_data_offset;
      unsigned slice_data_flag;
      unsigned slice_horizontal_position;
      unsigned slice_vertical_position;

      struct {
         uint8_t component_selector;
         uint8_t dc_table_selector;
         uint8_t ac_table_selector;
      } components[4];

      uint8_t num_components;

      uint16_t restart_interval;
      unsigned num_mcus;
   } slice_parameter;
};

struct mwv207_vp9_segment_parameter {
   struct {
      uint16_t segment_reference_enabled : 1;
      uint16_t segment_reference : 2;
      uint16_t segment_reference_skipped : 1;
   } segment_flags;

   bool alt_quant_enabled;
   int16_t alt_quant;

   bool alt_lf_enabled;
   int16_t alt_lf;

   uint8_t filter_level[4][2];

   int16_t luma_ac_quant_scale;
   int16_t luma_dc_quant_scale;

   int16_t chroma_ac_quant_scale;
   int16_t chroma_dc_quant_scale;
};

struct mwv207_vp9_picture_desc {
   struct mwv207_picture_desc base;

   struct mwv207_video_buffer *ref[16];

   struct {
      uint16_t frame_width;
      uint16_t frame_height;

      struct {
         uint32_t subsampling_x : 1;
         uint32_t subsampling_y : 1;
         uint32_t frame_type : 1;
         uint32_t show_frame : 1;
         uint32_t error_resilient_mode : 1;
         uint32_t intra_only : 1;
         uint32_t allow_high_precision_mv : 1;
         uint32_t mcomp_filter_type : 3;
         uint32_t frame_parallel_decoding_mode : 1;
         uint32_t reset_frame_context : 2;
         uint32_t refresh_frame_context : 1;
         uint32_t frame_context_idx : 2;
         uint32_t segmentation_enabled : 1;
         uint32_t segmentation_temporal_update : 1;
         uint32_t segmentation_update_map : 1;
         uint32_t last_ref_frame : 3;
         uint32_t last_ref_frame_sign_bias : 1;
         uint32_t golden_ref_frame : 3;
         uint32_t golden_ref_frame_sign_bias : 1;
         uint32_t alt_ref_frame : 3;
         uint32_t alt_ref_frame_sign_bias : 1;
         uint32_t lossless_flag : 1;
      } pic_fields;

      uint8_t filter_level;
      uint8_t sharpness_level;

      uint8_t log2_tile_rows;
      uint8_t log2_tile_columns;

      uint8_t frame_header_length_in_bytes;

      uint16_t first_partition_size;

      uint8_t mb_segment_tree_probs[7];
      uint8_t segment_pred_probs[3];

      uint8_t profile;

      uint8_t bit_depth;

      bool mode_ref_delta_enabled;
      bool mode_ref_delta_update;

      uint8_t base_qindex;
      int8_t y_dc_delta_q;
      int8_t uv_ac_delta_q;
      int8_t uv_dc_delta_q;
   } picture_parameter;

   struct {
      uint32_t slice_data_size;
      uint32_t slice_data_offset;

      uint32_t slice_data_flag;

      struct mwv207_vp9_segment_parameter seg_param[8];
   } slice_parameter;
};

#ifdef __cplusplus
}
#endif

#endif

