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
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "mwv207_video_decoder.h"

#include "util/format/u_format.h"
#include "util/u_framebuffer.h"
#include "util/u_inlines.h"
#include "util/u_memory.h"
#include "util/u_video.h"

#include "vl/vl_defines.h"
#include "vl/vl_video_buffer.h"

#include "mwv207_context.h"
#include "mwv207_screen.h"
#include "mwv207_surface.h"
#include "mwv207/mwv207_dec_vp9.h"
#include <stdint.h>
#include <time.h>

#define NUM_BUFFERS 4

#define NUM_MPEG2_REFS 6
#define NUM_H264_REFS 17
#define NUM_VC1_REFS 5

#define FB_BUFFER_OFFSET 0x1000
#define FB_BUFFER_SIZE 2048
#define FB_BUFFER_SIZE_TONGA (2048 * 64)
#define IT_SCALING_TABLE_SIZE 992
#define MWV207_VDEC_SESSION_CONTEXT_SIZE (128 * 1024)

static void
get_h264_msg(struct mwv207_video_decoder *dec,
             struct pipe_h264_picture_desc *pic)
{
   int i = 0;
   dec->video_format = MWV207_VIDEO_FORMAT_MPEG4_AVC;

   struct mwv207_h264_picture_desc *h264_desc = &dec->picture_desc.h264;
   h264_desc->pps.sps.level_idc = pic->pps->sps->level_idc;
   h264_desc->pps.sps.chroma_format_idc = pic->pps->sps->chroma_format_idc;
   h264_desc->pps.sps.separate_colour_plane_flag =
      pic->pps->sps->separate_colour_plane_flag;
   h264_desc->pps.sps.bit_depth_luma_minus8 =
      pic->pps->sps->bit_depth_luma_minus8;
   h264_desc->pps.sps.bit_depth_chroma_minus8 =
      pic->pps->sps->bit_depth_chroma_minus8;
   h264_desc->pps.sps.seq_scaling_matrix_present_flag =
      pic->pps->sps->seq_scaling_matrix_present_flag;

   memcpy(h264_desc->pps.sps.ScalingList4x4,
          pic->pps->sps->ScalingList4x4,
          6 * 16);
   memcpy(h264_desc->pps.sps.ScalingList8x8,
          pic->pps->sps->ScalingList8x8,
          6 * 64);

   h264_desc->pps.sps.log2_max_frame_num_minus4 =
      pic->pps->sps->log2_max_frame_num_minus4;
   h264_desc->pps.sps.pic_order_cnt_type = pic->pps->sps->pic_order_cnt_type;
   h264_desc->pps.sps.log2_max_pic_order_cnt_lsb_minus4 =
      pic->pps->sps->log2_max_pic_order_cnt_lsb_minus4;
   h264_desc->pps.sps.delta_pic_order_always_zero_flag =
      pic->pps->sps->delta_pic_order_always_zero_flag;
   h264_desc->pps.sps.offset_for_non_ref_pic =
      pic->pps->sps->offset_for_non_ref_pic;
   h264_desc->pps.sps.offset_for_top_to_bottom_field =
      pic->pps->sps->offset_for_top_to_bottom_field;
   h264_desc->pps.sps.num_ref_frames_in_pic_order_cnt_cycle =
      pic->pps->sps->num_ref_frames_in_pic_order_cnt_cycle;

   memcpy(h264_desc->pps.sps.offset_for_ref_frame,
          pic->pps->sps->offset_for_ref_frame,
          4 * 256);

   h264_desc->pps.sps.max_num_ref_frames = pic->pps->sps->max_num_ref_frames;
   h264_desc->pps.sps.frame_mbs_only_flag =
      pic->pps->sps->frame_mbs_only_flag;
   h264_desc->pps.sps.mb_adaptive_frame_field_flag =
      pic->pps->sps->mb_adaptive_frame_field_flag;
   h264_desc->pps.sps.direct_8x8_inference_flag =
      pic->pps->sps->direct_8x8_inference_flag;

   h264_desc->pps.entropy_coding_mode_flag =
      pic->pps->entropy_coding_mode_flag;
   h264_desc->pps.bottom_field_pic_order_in_frame_present_flag =
      pic->pps->bottom_field_pic_order_in_frame_present_flag;
   h264_desc->pps.num_slice_groups_minus1 = pic->pps->num_slice_groups_minus1;
   h264_desc->pps.slice_group_map_type = pic->pps->slice_group_map_type;
   h264_desc->pps.slice_group_change_rate_minus1 =
      pic->pps->slice_group_change_rate_minus1;
   h264_desc->pps.num_ref_idx_l0_default_active_minus1 =
      pic->pps->num_ref_idx_l0_default_active_minus1;
   h264_desc->pps.num_ref_idx_l1_default_active_minus1 =
      pic->pps->num_ref_idx_l1_default_active_minus1;
   h264_desc->pps.weighted_pred_flag = pic->pps->weighted_pred_flag;
   h264_desc->pps.weighted_bipred_idc = pic->pps->weighted_bipred_idc;
   h264_desc->pps.pic_init_qp_minus26 = pic->pps->pic_init_qp_minus26;
   h264_desc->pps.chroma_qp_index_offset = pic->pps->chroma_qp_index_offset;
   h264_desc->pps.deblocking_filter_control_present_flag =
      pic->pps->deblocking_filter_control_present_flag;
   h264_desc->pps.constrained_intra_pred_flag =
      pic->pps->constrained_intra_pred_flag;
   h264_desc->pps.redundant_pic_cnt_present_flag =
      pic->pps->redundant_pic_cnt_present_flag;

   memcpy(h264_desc->pps.ScalingList4x4, pic->pps->ScalingList4x4, 6 * 16);
   memcpy(h264_desc->pps.ScalingList8x8, pic->pps->ScalingList8x8, 6 * 64);

   h264_desc->pps.transform_8x8_mode_flag = pic->pps->transform_8x8_mode_flag;
   h264_desc->pps.second_chroma_qp_index_offset =
      pic->pps->second_chroma_qp_index_offset;

   h264_desc->frame_num = pic->frame_num;
   h264_desc->field_pic_flag = pic->field_pic_flag;
   h264_desc->bottom_field_flag = pic->bottom_field_flag;
   h264_desc->num_ref_idx_l0_active_minus1 =
      pic->num_ref_idx_l0_active_minus1;
   h264_desc->num_ref_idx_l1_active_minus1 =
      pic->num_ref_idx_l1_active_minus1;

   h264_desc->slice_count = pic->slice_count;
   h264_desc->field_order_cnt[0] = pic->field_order_cnt[0];
   h264_desc->field_order_cnt[1] = pic->field_order_cnt[1];
   h264_desc->is_reference = pic->is_reference;
   h264_desc->num_ref_frames = pic->num_ref_frames;

   memcpy(h264_desc->is_long_term, pic->is_long_term, 1 * 16);
   memcpy(h264_desc->top_is_reference, pic->top_is_reference, 1 * 16);
   memcpy(h264_desc->bottom_is_reference, pic->bottom_is_reference, 1 * 16);
   memcpy(
      h264_desc->field_order_cnt_list, pic->field_order_cnt_list, 4 * 16 * 2);
   memcpy(h264_desc->frame_num_list, pic->frame_num_list, 4 * 16);

   for (i = 0; i < 16; i++) {
      if (pic->ref[i]) {
         
         
         h264_desc->ref_frames[i].picture =
            (struct mwv207_video_buffer *)pic->ref[i];
         h264_desc->ref_frames[i].frame_num = h264_desc->frame_num_list[i];
         h264_desc->ref_frames[i].is_long_term = h264_desc->is_long_term[i];
         h264_desc->ref_frames[i].top_is_reference =
            h264_desc->top_is_reference[i];
         h264_desc->ref_frames[i].bottom_is_reference =
            h264_desc->bottom_is_reference[i];

         h264_desc->ref_frames[i].top_field_order_cnt =
            h264_desc->field_order_cnt_list[i][0];
         h264_desc->ref_frames[i].bottom_field_order_cnt =
            h264_desc->field_order_cnt_list[i][1];
      } else {
         h264_desc->ref_frames[i].picture = NULL;
         h264_desc->ref_frames[i].frame_num = 0;
         h264_desc->ref_frames[i].is_long_term = false;
         h264_desc->ref_frames[i].top_is_reference = false;
         h264_desc->ref_frames[i].bottom_is_reference = false;
         h264_desc->ref_frames[i].top_field_order_cnt = 0;
         h264_desc->ref_frames[i].bottom_field_order_cnt = 0;
      }
   }
}

static void
get_h265_msg(struct mwv207_video_decoder *dec,
             struct pipe_h265_picture_desc *pic)
{
   struct mwv207_h265_picture_desc *h265_desc = &dec->picture_desc.h265;
   dec->video_format = MWV207_VIDEO_FORMAT_HEVC;
   
   

   
   
   
   
   // *), 					sizeof(struct pipe_h265_pps) -
   
   // *));
   h265_desc->pps.sps.chroma_format_idc = pic->pps->sps->chroma_format_idc;
   h265_desc->pps.sps.separate_colour_plane_flag =
      pic->pps->sps->separate_colour_plane_flag;
   h265_desc->pps.sps.pic_width_in_luma_samples =
      pic->pps->sps->pic_width_in_luma_samples;
   h265_desc->pps.sps.pic_height_in_luma_samples =
      pic->pps->sps->pic_height_in_luma_samples;
   h265_desc->pps.sps.bit_depth_luma_minus8 =
      pic->pps->sps->bit_depth_luma_minus8;
   h265_desc->pps.sps.bit_depth_chroma_minus8 =
      pic->pps->sps->bit_depth_chroma_minus8;
   h265_desc->pps.sps.log2_max_pic_order_cnt_lsb_minus4 =
      pic->pps->sps->log2_max_pic_order_cnt_lsb_minus4;
   h265_desc->pps.sps.sps_max_dec_pic_buffering_minus1 =
      pic->pps->sps->sps_max_dec_pic_buffering_minus1;
   h265_desc->pps.sps.log2_min_luma_coding_block_size_minus3 =
      pic->pps->sps->log2_min_luma_coding_block_size_minus3;
   h265_desc->pps.sps.log2_diff_max_min_luma_coding_block_size =
      pic->pps->sps->log2_diff_max_min_luma_coding_block_size;
   h265_desc->pps.sps.log2_min_transform_block_size_minus2 =
      pic->pps->sps->log2_min_transform_block_size_minus2;
   h265_desc->pps.sps.log2_diff_max_min_transform_block_size =
      pic->pps->sps->log2_diff_max_min_transform_block_size;
   h265_desc->pps.sps.max_transform_hierarchy_depth_inter =
      pic->pps->sps->max_transform_hierarchy_depth_inter;
   h265_desc->pps.sps.max_transform_hierarchy_depth_intra =
      pic->pps->sps->max_transform_hierarchy_depth_intra;
   h265_desc->pps.sps.scaling_list_enabled_flag =
      pic->pps->sps->scaling_list_enabled_flag;
   memcpy(h265_desc->pps.sps.ScalingList4x4,
          pic->pps->sps->ScalingList4x4,
          sizeof(uint8_t) * 6 * 16);
   memcpy(h265_desc->pps.sps.ScalingList8x8,
          pic->pps->sps->ScalingList8x8,
          sizeof(uint8_t) * 6 * 64);
   memcpy(h265_desc->pps.sps.ScalingList16x16,
          pic->pps->sps->ScalingList16x16,
          sizeof(uint8_t) * 6 * 64);
   memcpy(h265_desc->pps.sps.ScalingList32x32,
          pic->pps->sps->ScalingList32x32,
          sizeof(uint8_t) * 2 * 64);
   memcpy(h265_desc->pps.sps.ScalingListDCCoeff16x16,
          pic->pps->sps->ScalingListDCCoeff16x16,
          sizeof(uint8_t) * 6);
   memcpy(h265_desc->pps.sps.ScalingListDCCoeff32x32,
          pic->pps->sps->ScalingListDCCoeff32x32,
          sizeof(uint8_t) * 2);
   h265_desc->pps.sps.amp_enabled_flag = pic->pps->sps->amp_enabled_flag;
   h265_desc->pps.sps.sample_adaptive_offset_enabled_flag =
      pic->pps->sps->sample_adaptive_offset_enabled_flag;
   h265_desc->pps.sps.pcm_enabled_flag = pic->pps->sps->pcm_enabled_flag;
   h265_desc->pps.sps.pcm_sample_bit_depth_luma_minus1 =
      pic->pps->sps->pcm_sample_bit_depth_luma_minus1;
   h265_desc->pps.sps.pcm_sample_bit_depth_chroma_minus1 =
      pic->pps->sps->pcm_sample_bit_depth_chroma_minus1;
   h265_desc->pps.sps.log2_min_pcm_luma_coding_block_size_minus3 =
      pic->pps->sps->log2_min_pcm_luma_coding_block_size_minus3;
   h265_desc->pps.sps.log2_diff_max_min_pcm_luma_coding_block_size =
      pic->pps->sps->log2_diff_max_min_pcm_luma_coding_block_size;
   h265_desc->pps.sps.pcm_loop_filter_disabled_flag =
      pic->pps->sps->pcm_loop_filter_disabled_flag;
   h265_desc->pps.sps.num_short_term_ref_pic_sets =
      pic->pps->sps->num_short_term_ref_pic_sets;
   h265_desc->pps.sps.long_term_ref_pics_present_flag =
      pic->pps->sps->long_term_ref_pics_present_flag;
   h265_desc->pps.sps.num_long_term_ref_pics_sps =
      pic->pps->sps->num_long_term_ref_pics_sps;
   h265_desc->pps.sps.sps_temporal_mvp_enabled_flag =
      pic->pps->sps->sps_temporal_mvp_enabled_flag;
   h265_desc->pps.sps.strong_intra_smoothing_enabled_flag =
      pic->pps->sps->strong_intra_smoothing_enabled_flag;

   h265_desc->pps.dependent_slice_segments_enabled_flag =
      pic->pps->dependent_slice_segments_enabled_flag;
   h265_desc->pps.output_flag_present_flag =
      pic->pps->output_flag_present_flag;
   h265_desc->pps.num_extra_slice_header_bits =
      pic->pps->num_extra_slice_header_bits;
   h265_desc->pps.sign_data_hiding_enabled_flag =
      pic->pps->sign_data_hiding_enabled_flag;
   h265_desc->pps.cabac_init_present_flag = pic->pps->cabac_init_present_flag;
   h265_desc->pps.num_ref_idx_l0_default_active_minus1 =
      pic->pps->num_ref_idx_l0_default_active_minus1;
   h265_desc->pps.num_ref_idx_l1_default_active_minus1 =
      pic->pps->num_ref_idx_l1_default_active_minus1;
   h265_desc->pps.init_qp_minus26 = pic->pps->init_qp_minus26;
   h265_desc->pps.constrained_intra_pred_flag =
      pic->pps->constrained_intra_pred_flag;
   h265_desc->pps.transform_skip_enabled_flag =
      pic->pps->transform_skip_enabled_flag;
   h265_desc->pps.cu_qp_delta_enabled_flag =
      pic->pps->cu_qp_delta_enabled_flag;
   h265_desc->pps.diff_cu_qp_delta_depth = pic->pps->diff_cu_qp_delta_depth;
   h265_desc->pps.pps_cb_qp_offset = pic->pps->pps_cb_qp_offset;
   h265_desc->pps.pps_cr_qp_offset = pic->pps->pps_cr_qp_offset;
   h265_desc->pps.pps_slice_chroma_qp_offsets_present_flag =
      pic->pps->pps_slice_chroma_qp_offsets_present_flag;
   h265_desc->pps.weighted_pred_flag = pic->pps->weighted_pred_flag;
   h265_desc->pps.weighted_bipred_flag = pic->pps->weighted_bipred_flag;
   h265_desc->pps.transquant_bypass_enabled_flag =
      pic->pps->transquant_bypass_enabled_flag;
   h265_desc->pps.tiles_enabled_flag = pic->pps->tiles_enabled_flag;
   h265_desc->pps.entropy_coding_sync_enabled_flag =
      pic->pps->entropy_coding_sync_enabled_flag;
   h265_desc->pps.num_tile_columns_minus1 = pic->pps->num_tile_columns_minus1;
   h265_desc->pps.num_tile_rows_minus1 = pic->pps->num_tile_rows_minus1;
   h265_desc->pps.uniform_spacing_flag = pic->pps->uniform_spacing_flag;
   memcpy(h265_desc->pps.column_width_minus1,
          pic->pps->column_width_minus1,
          sizeof(uint16_t) * 20);
   memcpy(h265_desc->pps.row_height_minus1,
          pic->pps->row_height_minus1,
          sizeof(uint16_t) * 22);
   h265_desc->pps.loop_filter_across_tiles_enabled_flag =
      pic->pps->loop_filter_across_tiles_enabled_flag;
   h265_desc->pps.pps_loop_filter_across_slices_enabled_flag =
      pic->pps->pps_loop_filter_across_slices_enabled_flag;
   h265_desc->pps.deblocking_filter_control_present_flag =
      pic->pps->deblocking_filter_control_present_flag;
   h265_desc->pps.deblocking_filter_override_enabled_flag =
      pic->pps->deblocking_filter_override_enabled_flag;
   h265_desc->pps.pps_deblocking_filter_disabled_flag =
      pic->pps->pps_deblocking_filter_disabled_flag;
   h265_desc->pps.pps_beta_offset_div2 = pic->pps->pps_beta_offset_div2;
   h265_desc->pps.pps_tc_offset_div2 = pic->pps->pps_tc_offset_div2;
   h265_desc->pps.lists_modification_present_flag =
      pic->pps->lists_modification_present_flag;
   h265_desc->pps.log2_parallel_merge_level_minus2 =
      pic->pps->log2_parallel_merge_level_minus2;
   h265_desc->pps.slice_segment_header_extension_present_flag =
      pic->pps->slice_segment_header_extension_present_flag;

   h265_desc->IDRPicFlag = pic->IDRPicFlag;
   h265_desc->RAPPicFlag = pic->RAPPicFlag;
   h265_desc->CurrRpsIdx = pic->CurrRpsIdx;
   h265_desc->NumPocTotalCurr = pic->NumPocTotalCurr;
   h265_desc->NumDeltaPocsOfRefRpsIdx = pic->NumDeltaPocsOfRefRpsIdx;
   h265_desc->NumShortTermPictureSliceHeaderBits =
      pic->NumShortTermPictureSliceHeaderBits;
   h265_desc->NumLongTermPictureSliceHeaderBits =
      pic->NumLongTermPictureSliceHeaderBits;
   h265_desc->CurrPicOrderCntVal = pic->CurrPicOrderCntVal;
   memcpy(
      h265_desc->PicOrderCntVal, pic->PicOrderCntVal, sizeof(int32_t) * 16);
   memcpy(h265_desc->IsLongTerm, pic->IsLongTerm, sizeof(uint8_t) * 16);
   h265_desc->NumPocStCurrBefore = pic->NumPocStCurrBefore;
   h265_desc->NumPocStCurrAfter = pic->NumPocStCurrAfter;
   h265_desc->NumPocLtCurr = pic->NumPocLtCurr;
   memcpy(h265_desc->RefPicSetStCurrBefore,
          pic->RefPicSetStCurrBefore,
          sizeof(uint8_t) * 8);
   memcpy(h265_desc->RefPicSetStCurrAfter,
          pic->RefPicSetStCurrAfter,
          sizeof(uint8_t) * 8);
   memcpy(
      h265_desc->RefPicSetLtCurr, pic->RefPicSetLtCurr, sizeof(uint8_t) * 8);
   memcpy(h265_desc->RefPicList, pic->RefPicList, sizeof(uint8_t) * 2 * 15);
   h265_desc->UseRefPicList = pic->UseRefPicList;
   

   int i = 0;
   for (i = 0; i < 16; i++) {
      if (pic->ref[i]) {
         h265_desc->ref_frames[i].picture =
            (struct mwv207_video_buffer *)pic->ref[i];
         h265_desc->ref_frames[i].PicOrderCntVal =
            h265_desc->PicOrderCntVal[i];
         h265_desc->ref_frames[i].is_long_term = h265_desc->IsLongTerm[i];
      } else {
         h265_desc->ref_frames[i].picture = NULL;
         h265_desc->ref_frames[i].PicOrderCntVal = 0;
         h265_desc->ref_frames[i].is_long_term = 0;
      }
   }
}

/**
 * destroy this video decoder
 */
static void
mwv207_video_decoder_destroy(struct pipe_video_codec *decoder)
{
   MWV207_LOG(MWV207_LEVEL_LOG, "\n");
   struct mwv207_video_decoder *dec = mwv207_video_dec(decoder);
   mwv207_video_cs_release(&dec->cs);
   mwv207_video_dec_destroy(dec->decoder);
   dec->decoder = NULL;
   int i = 0;
   for (i = 0; i < NUM_BUFFERS; ++i) {
      if (dec->bs_buffers[i]) {
         mwv207_video_buffer_destroy(
            (struct pipe_video_buffer *)dec->bs_buffers[i]);
      }
   }

   FREE(dec);
}

/**
 * start decoding of a new frame
 */
static void
mwv207_video_decoder_begin_frame(struct pipe_video_codec *decoder,
                                 struct pipe_video_buffer *target,
                                 struct pipe_picture_desc *picture)
{
   struct mwv207_video_decoder *dec = mwv207_video_dec(decoder);
   struct mwv207_video_buffer *target_buf =
      (struct mwv207_video_buffer *)target;

   assert(decoder);

   mwv207_codec_time_record_start(&dec->codec_time);

   dec->ws->bo_wait(dec->ws,
                    dec->bs_buffers[dec->cur_buffer]->res->bo,
                    OS_TIMEOUT_INFINITE);
   dec->bs_size = 0;
   dec->bs_ptr =
      dec->ws->bo_map(dec->ws, dec->bs_buffers[dec->cur_buffer]->res->bo);

   if (target_buf && target_buf->res) {
      dec->ws->bo_wait(dec->ws, target_buf->res->bo, OS_TIMEOUT_INFINITE);
   }
}

/**
 * decode a macroblock
 */
static void
mwv207_video_decoder_decode_macroblock(
   struct pipe_video_codec *decoder,
   struct pipe_video_buffer *target,
   struct pipe_picture_desc *picture,
   const struct pipe_macroblock *macroblocks,
   unsigned num_macroblocks)
{

   assert(0);
}

/**
 * decode a bitstream
 */
static void
mwv207_video_decoder_decode_bitstream(struct pipe_video_codec *decoder,
                                      struct pipe_video_buffer *target,
                                      struct pipe_picture_desc *picture,
                                      unsigned num_buffers,
                                      const void *const *buffers,
                                      const unsigned *sizes)
{
   struct mwv207_video_decoder *dec = mwv207_video_dec(decoder);
   int i;

   assert(decoder);

   if (!dec->bs_ptr)
      return;

   for (i = 0; i < num_buffers; ++i) {
      if ((i == 0)
          && num_buffers == 3) { 
         continue;
      }

      if (num_buffers == 1) {
         dec->streams_num = 1;
         uint8_t *tmp = (uint8_t *)buffers[i];
         if (sizes[i] >= 4 && (*(tmp + 0) == 0x0) && (*(tmp + 1) == 0x0)
             && (*(tmp + 2) == 0x1)) {
            
            
            memcpy(dec->streams[0] + dec->streams_len[0],
                   buffers[i] + 3,
                   sizes[i] - 3);

            dec->streams_len[0] += sizes[i] - 3;

            memcpy(dec->bs_ptr, buffers[i], sizes[i]);
            dec->bs_size += sizes[i];
            dec->bs_ptr += sizes[i];
         } else {
            if (sizes[i] != 1) {
               
               
               memcpy(dec->streams[0] + dec->streams_len[0],
                      buffers[i],
                      sizes[i]);

               dec->streams_len[0] += sizes[i];
               memcpy(dec->bs_ptr, buffers[i], sizes[i]);
               dec->bs_size += sizes[i];
               dec->bs_ptr += sizes[i];
            } else {
               // printf("buf value: %d \n", *tmp);
            }
         }

      } else {
         if ((sizes[i] != 3) && (sizes[i] != 4)) {
            memcpy(dec->streams[dec->streams_num], buffers[i], sizes[i]);
            dec->streams_len[dec->streams_num] = sizes[i];
            dec->streams_num++;
         }

         memcpy(dec->bs_ptr, buffers[i], sizes[i]);
         dec->bs_size += sizes[i];
         dec->bs_ptr += sizes[i];
      }
   }
}

static void
get_vp9_msg(struct mwv207_video_decoder *dec,
            struct pipe_vp9_picture_desc *pic)
{
   struct mwv207_vp9_picture_desc *vp9 = &dec->picture_desc.vp9;
   struct mwv207_vp9_picture_desc *d = vp9;

   dec->video_format = MWV207_VIDEO_FORMAT_VP9;

   d->picture_parameter.frame_width = pic->picture_parameter.frame_width;
   d->picture_parameter.frame_height = pic->picture_parameter.frame_height;
   d->picture_parameter.pic_fields.subsampling_x = pic->picture_parameter.pic_fields.subsampling_x;
   d->picture_parameter.pic_fields.subsampling_y = pic->picture_parameter.pic_fields.subsampling_y;
   d->picture_parameter.pic_fields.frame_type = pic->picture_parameter.pic_fields.frame_type;
   d->picture_parameter.pic_fields.show_frame = pic->picture_parameter.pic_fields.show_frame;
   d->picture_parameter.pic_fields.error_resilient_mode = pic->picture_parameter.pic_fields.error_resilient_mode;
   d->picture_parameter.pic_fields.intra_only = pic->picture_parameter.pic_fields.intra_only;
   d->picture_parameter.pic_fields.allow_high_precision_mv = pic->picture_parameter.pic_fields.allow_high_precision_mv;
   d->picture_parameter.pic_fields.mcomp_filter_type = pic->picture_parameter.pic_fields.mcomp_filter_type;
   d->picture_parameter.pic_fields.frame_parallel_decoding_mode = pic->picture_parameter.pic_fields.frame_parallel_decoding_mode;
   d->picture_parameter.pic_fields.reset_frame_context = pic->picture_parameter.pic_fields.reset_frame_context;
   d->picture_parameter.pic_fields.refresh_frame_context = pic->picture_parameter.pic_fields.refresh_frame_context;
   d->picture_parameter.pic_fields.frame_context_idx = pic->picture_parameter.pic_fields.frame_context_idx;
   d->picture_parameter.pic_fields.segmentation_enabled = pic->picture_parameter.pic_fields.segmentation_enabled;
   d->picture_parameter.pic_fields.segmentation_temporal_update = pic->picture_parameter.pic_fields.segmentation_temporal_update;
   d->picture_parameter.pic_fields.segmentation_update_map = pic->picture_parameter.pic_fields.segmentation_update_map;
   d->picture_parameter.pic_fields.last_ref_frame = pic->picture_parameter.pic_fields.last_ref_frame;
   d->picture_parameter.pic_fields.last_ref_frame_sign_bias = pic->picture_parameter.pic_fields.last_ref_frame_sign_bias;
   d->picture_parameter.pic_fields.golden_ref_frame = pic->picture_parameter.pic_fields.golden_ref_frame;
   d->picture_parameter.pic_fields.golden_ref_frame_sign_bias = pic->picture_parameter.pic_fields.golden_ref_frame_sign_bias;
   d->picture_parameter.pic_fields.alt_ref_frame = pic->picture_parameter.pic_fields.alt_ref_frame;
   d->picture_parameter.pic_fields.alt_ref_frame_sign_bias = pic->picture_parameter.pic_fields.alt_ref_frame_sign_bias;
   d->picture_parameter.pic_fields.lossless_flag = pic->picture_parameter.pic_fields.lossless_flag;

   d->picture_parameter.filter_level = pic->picture_parameter.filter_level;
   d->picture_parameter.sharpness_level = pic->picture_parameter.sharpness_level;
   d->picture_parameter.log2_tile_rows = pic->picture_parameter.log2_tile_rows;
   d->picture_parameter.log2_tile_columns = pic->picture_parameter.log2_tile_columns;
   d->picture_parameter.frame_header_length_in_bytes = pic->picture_parameter.frame_header_length_in_bytes;
   d->picture_parameter.first_partition_size = pic->picture_parameter.first_partition_size;

   memcpy(d->picture_parameter.mb_segment_tree_probs, pic->picture_parameter.mb_segment_tree_probs, 7);
   memcpy(d->picture_parameter.segment_pred_probs, pic->picture_parameter.segment_pred_probs, 3);

   d->picture_parameter.profile = pic->picture_parameter.profile;
   d->picture_parameter.bit_depth = pic->picture_parameter.bit_depth;
   d->picture_parameter.mode_ref_delta_enabled = pic->picture_parameter.mode_ref_delta_enabled;
   d->picture_parameter.mode_ref_delta_update = pic->picture_parameter.mode_ref_delta_update;
   d->picture_parameter.base_qindex = pic->picture_parameter.base_qindex;
   d->picture_parameter.y_dc_delta_q = pic->picture_parameter.y_dc_delta_q;
   d->picture_parameter.uv_ac_delta_q = pic->picture_parameter.uv_ac_delta_q;
   d->picture_parameter.uv_dc_delta_q = pic->picture_parameter.uv_dc_delta_q;

   for (int i = 0; i < 16; i++)
      d->ref[i] = (struct mwv207_video_buffer *)pic->ref[i];

   if (pic->slice_parameter.slice_count) {
      d->slice_parameter.slice_data_size = pic->slice_parameter.slice_data_size[0];
      d->slice_parameter.slice_data_offset = pic->slice_parameter.slice_data_offset[0];
      d->slice_parameter.slice_data_flag = pic->slice_parameter.slice_data_flag[0];
      for (int i = 0; i < 8; i++) {
         d->slice_parameter.seg_param[i].segment_flags.segment_reference_enabled =
            pic->slice_parameter.seg_param[i].segment_flags.segment_reference_enabled;
         d->slice_parameter.seg_param[i].segment_flags.segment_reference =
            pic->slice_parameter.seg_param[i].segment_flags.segment_reference;
         d->slice_parameter.seg_param[i].segment_flags.segment_reference_skipped =
            pic->slice_parameter.seg_param[i].segment_flags.segment_reference_skipped;
         d->slice_parameter.seg_param[i].alt_quant_enabled = pic->slice_parameter.seg_param[i].alt_quant_enabled;
         d->slice_parameter.seg_param[i].alt_quant = pic->slice_parameter.seg_param[i].alt_quant;
         d->slice_parameter.seg_param[i].alt_lf_enabled = pic->slice_parameter.seg_param[i].alt_lf_enabled;
         d->slice_parameter.seg_param[i].alt_lf = pic->slice_parameter.seg_param[i].alt_lf;
         memcpy(d->slice_parameter.seg_param[i].filter_level, pic->slice_parameter.seg_param[i].filter_level, sizeof(d->slice_parameter.seg_param[i].filter_level));
         d->slice_parameter.seg_param[i].luma_ac_quant_scale = pic->slice_parameter.seg_param[i].luma_ac_quant_scale;
         d->slice_parameter.seg_param[i].luma_dc_quant_scale = pic->slice_parameter.seg_param[i].luma_dc_quant_scale;
         d->slice_parameter.seg_param[i].chroma_ac_quant_scale = pic->slice_parameter.seg_param[i].chroma_ac_quant_scale;
         d->slice_parameter.seg_param[i].chroma_dc_quant_scale = pic->slice_parameter.seg_param[i].chroma_dc_quant_scale;
      }
   }
}

/**
 * 把"本帧解码失败"告知上层（I-6）。
 *
 * Mesa 的 va 前端只有在 *picture->fence 非空时才会调 get_decoder_fence()，
 * 而 vaSyncSurface() 又只把该函数的返回值当作成功/超时来翻译（返回 0 →
 * VA_STATUS_ERROR_TIMEDOUT）。所以这里给一个"永远就绪"的桩 fence
 * （fd = -1，见 mwv207_drm_cs.c 的 fence_create/fence_wait），真正的失败
 * 信号由 get_decoder_fence() 的返回值传出。
 *
 * 在此之前的行为是：跳过解码但照常返回，VAAPI 侧一无所知，应用拿到的是
 * 目标 surface 里的旧内容（B2 的 resize_inter 向量、I-3/I-4 的拒绝路径
 * 都是如此）。
 */
static void
mwv207_video_decoder_mark_frame_failed(struct mwv207_video_decoder *dec,
                                       struct pipe_picture_desc *picture)
{
   struct pipe_fence_handle *stub;

   dec->frame_failed = true;

   if (!picture || !picture->fence)
      return;

   stub = dec->ws->fence_create(dec->ws, -1);
   if (!stub)
      return;

   dec->ws->fence_reference(picture->fence, stub);
   dec->ws->fence_reference(&stub, NULL);
}

/**
 * vaSyncSurface() 的实现入口：等待上一帧的 fence，并把"这一帧失败了"
 * 通过返回值 0 传给 Mesa（→ VA_STATUS_ERROR_TIMEDOUT）。
 */
static int
mwv207_video_decoder_get_decoder_fence(struct pipe_video_codec *decoder,
                                       struct pipe_fence_handle *fence,
                                       uint64_t timeout)
{
   struct mwv207_video_decoder *dec = mwv207_video_dec(decoder);

   if (dec->frame_failed) {
      dec->frame_failed = false;
      return 0;
   }

   if (!fence)
      return 1;

   return dec->ws->fence_wait(dec->ws, fence, timeout);
}

/**
 * end decoding of the current frame
 */
static void
mwv207_video_decoder_end_frame(struct pipe_video_codec *decoder,
                               struct pipe_video_buffer *target,
                               struct pipe_picture_desc *picture)
{
   struct mwv207_video_decoder *dec = mwv207_video_dec(decoder);

   assert(decoder);

   switch (u_reduce_video_profile(picture->profile)) {
   case PIPE_VIDEO_FORMAT_MPEG4_AVC: {
      /* FIX(jm9100-oss): I-3 —— 硬件解码器只有 4:0:0 / 4:2:0 两种色度采样
       * （寄存器表无采样格式选择字段，见 mwv207_video_codec.c 里
       * mwv207_profile_supported 上方注释）。VAAPI 侧已收回 422/444 声明，
       * 正常客户端不会走到这里；万一有客户端绕过能力表硬塞 4:2:2/4:4:4
       * 码流，这里显式拒绝并清掉本帧流数据，避免把错色度的画面当成功
       * 解出来（end_frame 返回 void，无法向上报错）。 */
      struct pipe_h264_picture_desc *h264_pic =
         (struct pipe_h264_picture_desc *)picture;
      if (h264_pic->pps && h264_pic->pps->sps &&
          h264_pic->pps->sps->chroma_format_idc > 1) {
         static bool warned;
         if (!warned) {
            warned = true;
            MWV207_LOG(MWV207_LEVEL_ERROR,
                       "H.264 chroma_format_idc=%u (4:2:2/4:4:4) 不被硬件支持，"
                       "已跳过解码\n",
                       h264_pic->pps->sps->chroma_format_idc);
         }
         memset(dec->streams_len, 0, sizeof(dec->streams_len));
         dec->streams_num = 0;
         dec->decoder->reg_buf->bos_clear(dec->decoder->reg_buf);
         mwv207_video_decoder_mark_frame_failed(dec, picture);
         return;
      }
      /* FIX(jm9100-oss): I-4 —— 高位深（High10）H.264 同样跳过：按当前寄存器
       * 配置提交会让 DEC 管线挂死（实测一个 10 帧样片触发 6 次 TDR，
       * `job from 'av:h264:df0' to 'mwv207_dec0', timed out!`，输出面全 0）。
       * Hantro 系的 10bit H.264 需要 high10p 位流预转换，本驱动未实现，
       * 能力表里也已收回 High10 声明。客户端若绕过能力表硬塞 10bit 码流，
       * 这里直接拒绝，不把 DEC 管线拖进超时循环。 */
      if (h264_pic->pps && h264_pic->pps->sps &&
          h264_pic->pps->sps->bit_depth_luma_minus8 > 0) {
         static bool warned;
         if (!warned) {
            warned = true;
            MWV207_LOG(MWV207_LEVEL_ERROR,
                       "H.264 bit_depth=%u (High10) 不被硬件支持，已跳过解码\n",
                       h264_pic->pps->sps->bit_depth_luma_minus8 + 8);
         }
         memset(dec->streams_len, 0, sizeof(dec->streams_len));
         dec->streams_num = 0;
         dec->decoder->reg_buf->bos_clear(dec->decoder->reg_buf);
         mwv207_video_decoder_mark_frame_failed(dec, picture);
         return;
      }
      get_h264_msg(dec, h264_pic);
      break;
   }

   case PIPE_VIDEO_FORMAT_HEVC:
      get_h265_msg(dec, (struct pipe_h265_picture_desc *)picture);
      break;

   case PIPE_VIDEO_FORMAT_VP9:
      get_vp9_msg(dec, (struct pipe_vp9_picture_desc *)picture);
      break;
   case PIPE_VIDEO_FORMAT_VC1:
      break;

   case PIPE_VIDEO_FORMAT_MPEG12:
      break;

   case PIPE_VIDEO_FORMAT_MPEG4:
      break;

   case PIPE_VIDEO_FORMAT_JPEG:
      break;

   default:
      mwv207_video_decoder_mark_frame_failed(dec, picture);
      return;
   }

   mwv207_9200_video_dec_picture(dec, target);

   /* I-6：codec 层在 set_regs() 里发现本帧不能解码（参数不支持、参考缺失、
    * 尺寸不一致……）时只把 JMDEC_DEC_E 清掉并返回，PPU 输出面根本没编程。
    * 以前这里照常提交，应用于是拿到目标 surface 的旧内容；现在改成不提交、
    * 并把失败传给 vaSyncSurface()。 */
   if (dec->frame_skipped) {
      dec->frame_skipped = false;
      mwv207_video_decoder_mark_frame_failed(dec, picture);
      dec->streams_num = 0;
      memset(dec->streams_len, 0, sizeof(dec->streams_len));
      dec->decoder->reg_buf->bos_clear(dec->decoder->reg_buf);
      mwv207_video_dec_destroy_buffer(dec->decoder);
      ++dec->cur_buffer;
      dec->cur_buffer %= MWV207_NUM_BUFFERS;
      return;
   }

   // struct pipe_h264_picture_desc *pic_param = &decoder->picture_desc.h264;
   // struct mwv207_h264_picture_desc *pic_param = &dec->picture_desc.h264;
   // struct pipe_h264_picture_desc *picture2 =
   //    (struct pipe_h264_picture_desc *)picture;
   

   /* VP9 copies the freshly decoded picture into a reference slot right
    * below, so the job must have completed before we touch the staging
    * buffer. */
   if (dec->video_format == MWV207_VIDEO_FORMAT_VP9)
      mwv207_video_cs_submit_wait(&dec->cs, dec->decoder->reg_buf);
   else
      mwv207_video_cs_submit(&dec->cs, dec->decoder->reg_buf);
   mwv207_dec_vp9_submit_done(dec);


   dec->streams_num = 0;
   memset(dec->streams_len, 0, sizeof(dec->streams_len));
   dec->decoder->reg_buf->bos_clear(dec->decoder->reg_buf);

   dec->codec_time.codec_finish_time = os_time_get();
   mwv207_codec_time_compute(&dec->codec_time);

   mwv207_video_dec_destroy_buffer(dec->decoder);

   dec->decoder->poc.prev_poc_msb = dec->decoder->poc.poc_msb;
   dec->decoder->poc.prev_poc_lsb = dec->decoder->poc.poc_lsb;
   dec->decoder->poc.prev_frame_num_offset =
      dec->decoder->poc.frame_num_offset;
   dec->decoder->poc.prev_frame_num = dec->decoder->poc.frame_num;

   ++dec->cur_buffer;
   dec->cur_buffer %= MWV207_NUM_BUFFERS;
}

/**
 * flush any outstanding command buffers to the hardware
 */
static void
mwv207_video_decoder_flush(struct pipe_video_codec *decoder)
{
}

struct pipe_video_codec *
mwv207_create_decoder(struct pipe_context *context,
                      const struct pipe_video_codec *decoder)
{
   MWV207_LOG(MWV207_LEVEL_DEBUG,
              " width : %d, height : %d \n",
              decoder->width,
              decoder->height);
   struct mwv207_context *ctx = mwv207_context(context);
   struct mwv207_winsys *ws = ctx->ws;

   unsigned width = decoder->width;
   unsigned height = decoder->height;
   unsigned bs_buf_size;
   int i;

   struct mwv207_video_decoder *dec = CALLOC_STRUCT(mwv207_video_decoder);
   if (!dec)
      return NULL;

   dec->frame_number = 0;
   dec->base = *decoder;
   dec->base.context = context;
   dec->base.width = width;
   dec->base.height = height;

   dec->base.destroy = mwv207_video_decoder_destroy;
   dec->base.begin_frame = mwv207_video_decoder_begin_frame;
   dec->base.decode_macroblock = mwv207_video_decoder_decode_macroblock;
   dec->base.decode_bitstream = mwv207_video_decoder_decode_bitstream;
   dec->base.end_frame = mwv207_video_decoder_end_frame;
   dec->base.get_decoder_fence = mwv207_video_decoder_get_decoder_fence;
   dec->base.flush = mwv207_video_decoder_flush;

   dec->screen = context->screen;
   dec->ws = ws;
   dec->cur_buffer = 0;

   dec->decoder = mwv207_video_dec_create(ws);
   mwv207_video_cs_init(&dec->cs, ws);
   dec->cs.codec_time = &dec->codec_time;

   bs_buf_size = width * height * 3;
   for (i = 0; i < MWV207_NUM_BUFFERS; ++i) {
      dec->streams[i] = MALLOC(bs_buf_size);
      dec->bs_buffers[i] = mwv207_video_buffer_create_2(
         dec->base.context, bs_buf_size, PIPE_USAGE_STAGING);
      if (!dec->bs_buffers[i]) {
         MWV207_LOG(MWV207_LEVEL_ERROR,
                    "Can't allocated bitstream buffers.\n");
         goto error;
      }
   }

   return &dec->base;
error:
   mwv207_video_decoder_destroy(&dec->base);
   return NULL;
}

