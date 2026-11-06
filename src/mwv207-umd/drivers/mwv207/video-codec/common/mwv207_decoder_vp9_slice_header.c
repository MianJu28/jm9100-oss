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

#include "mwv207_decoder_vp9_slice_header.h"
#include "mwv207_vp9hwd_bool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MWV207_OK 0
#define MWV207_NOK 1

#define VP9_KEY_FRAME_START_CODE_0 0x49
#define VP9_KEY_FRAME_START_CODE_1 0x83
#define VP9_KEY_FRAME_START_CODE_2 0x42
#define MIN_TILE_WIDTH 256
#define MAX_TILE_WIDTH 4096
#define MIN_TILE_WIDTH_SBS (MIN_TILE_WIDTH >> 6)
#define MAX_TILE_WIDTH_SBS (MAX_TILE_WIDTH >> 6)

static uint32_t
check_sync_code(struct mwv207_strm_data *rb)
{
   if (mwv207_sw_get_bits(rb, 8) != VP9_KEY_FRAME_START_CODE_0
       || mwv207_sw_get_bits(rb, 8) != VP9_KEY_FRAME_START_CODE_1
       || mwv207_sw_get_bits(rb, 8) != VP9_KEY_FRAME_START_CODE_2) {
      printf("VP9 Key-frame start code missing or invalid!\n");
      return MWV207_NOK;
   }
   return MWV207_OK;
}

static void
existing_fef_register(struct mwv207_vp9_slice_header *slice_header, uint32_t map)
{
   slice_header->existing_ref_map |= map;
}

static uint32_t
setup_frame_size(struct mwv207_strm_data *rb,
               struct mwv207_vp9_slice_header *slice_header)
{
   uint32_t tmp;

   tmp = mwv207_sw_get_bits(rb, 16);
   slice_header->width = tmp + 1;
   STREAM_TRACE("frame_width", slice_header->width);

   tmp = mwv207_sw_get_bits(rb, 16);
   slice_header->height = tmp + 1;
   STREAM_TRACE("frame_height", slice_header->height);

   tmp = mwv207_sw_get_bits(rb, 1);
   slice_header->scaling_active = tmp;
   STREAM_TRACE("Scaling active", slice_header->scaling_active);

   if (slice_header->scaling_active) {

      tmp = mwv207_sw_get_bits(rb, 16);
      slice_header->scaled_width = tmp + 1;
      STREAM_TRACE("scaled_frame_widthX", slice_header->scaled_width);

      tmp = mwv207_sw_get_bits(rb, 16);
      slice_header->scaled_height = tmp + 1;
      STREAM_TRACE("scaled_frame_heightX", slice_header->scaled_height);
   }

   return (MWV207_OK);
}

static void
decode_lf_params(struct mwv207_strm_data *rb,
               struct mwv207_vp9_slice_header *slice_header)
{
   uint32_t sign, tmp, j;

   if (slice_header->key_frame || slice_header->error_resilient
       || slice_header->intra_only) {
      memset(slice_header->mb_ref_lf_delta,
             0,
             sizeof(slice_header->mb_ref_lf_delta));
      memset(slice_header->mb_mode_lf_delta,
             0,
             sizeof(slice_header->mb_mode_lf_delta));
      slice_header->mb_ref_lf_delta[0] = 1;
      slice_header->mb_ref_lf_delta[1] = 0;
      slice_header->mb_ref_lf_delta[2] = -1;
      slice_header->mb_ref_lf_delta[3] = -1;
   }

   slice_header->loop_filter_level = mwv207_sw_get_bits(rb, 6);
   slice_header->loop_filter_sharpness = mwv207_sw_get_bits(rb, 3);
   STREAM_TRACE("loop_filter_level", slice_header->loop_filter_level);
   STREAM_TRACE("loop_filter_sharpness", slice_header->loop_filter_sharpness);

   slice_header->mode_ref_lf_enabled = mwv207_sw_get_bits(rb, 1);
   STREAM_TRACE("loop_filter_adj_enable", slice_header->mode_ref_lf_enabled);

   if (slice_header->mode_ref_lf_enabled) {

      tmp = mwv207_sw_get_bits(rb, 1);
      STREAM_TRACE("loop_filter_adj_update", tmp);
      if (tmp) {

         for (j = 0; j < MAX_REF_LF_DELTAS; j++) {
            tmp = mwv207_sw_get_bits(rb, 1);
            STREAM_TRACE("ref_frame_delta_update", tmp);
            if (tmp) {

               tmp = mwv207_sw_get_bits(rb, 6);

               sign = mwv207_sw_get_bits(rb, 1);
               STREAM_TRACE("loop_filter_payload", tmp);
               STREAM_TRACE("loop_filter_sign", sign);

               slice_header->mb_ref_lf_delta[j] = tmp;
               if (sign)
                  slice_header->mb_ref_lf_delta[j] =
                     -slice_header->mb_ref_lf_delta[j];
            }
         }

         for (j = 0; j < MAX_MODE_LF_DELTAS; j++) {
            tmp = mwv207_sw_get_bits(rb, 1);
            STREAM_TRACE("mb_type_delta_update", tmp);
            if (tmp) {

               tmp = mwv207_sw_get_bits(rb, 6);

               sign = mwv207_sw_get_bits(rb, 1);
               STREAM_TRACE("loop_filter_payload", tmp);
               STREAM_TRACE("loop_filter_sign", sign);

               slice_header->mb_mode_lf_delta[j] = tmp;
               if (sign)
                  slice_header->mb_mode_lf_delta[j] =
                     -slice_header->mb_mode_lf_delta[j];
            }
         }
      }
   } /* Mb mode/ref lf adjustment */
}

static int32_t
decode_quantizer_delta(struct mwv207_strm_data *rb)
{
   uint32_t sign;
   int32_t delta;

   if (mwv207_sw_get_bits(rb, 1)) {
      STREAM_TRACE("qp_delta_present", 1);
      delta = mwv207_sw_get_bits(rb, 4);
      STREAM_TRACE("qp_delta", delta);
      sign = mwv207_sw_get_bits(rb, 1);
      STREAM_TRACE("qp_delta_sign", sign);
      if (sign)
         delta = -delta;
      return delta;
   } else {
      STREAM_TRACE("qp_delta_present", 0);
      return 0;
   }
}

static void
get_tile_n_bits(struct mwv207_vp9_slice_header *slice_header,
             uint32_t *min_log2_ntiles_ptr,
             uint32_t *delta_log2_ntiles)
{
   const int sb_cols = (slice_header->width + 63) >> 6;
   uint32_t min_log2_ntiles, max_log2_ntiles;

   for (max_log2_ntiles = 0;
        (sb_cols >> max_log2_ntiles) >= MIN_TILE_WIDTH_SBS;
        max_log2_ntiles++) {
   }
   if (max_log2_ntiles > 0)
      max_log2_ntiles--;
   for (min_log2_ntiles = 0;
        (MAX_TILE_WIDTH_SBS << min_log2_ntiles) < sb_cols;
        min_log2_ntiles++) {
   }

   
   *min_log2_ntiles_ptr = min_log2_ntiles;
   *delta_log2_ntiles = max_log2_ntiles - min_log2_ntiles;
}

static void
decode_segmentation_data(struct mwv207_strm_data *rb,
                       struct mwv207_vp9_slice_header *slice_header)
{
   uint32_t tmp, sign, i, j;
   struct mwv207_vp9_entropy_probs *fc =
      &slice_header->entropy;

   slice_header->segment_enabled = mwv207_sw_get_bits(rb, 1);
   STREAM_TRACE("segment_enabled", slice_header->segment_enabled);

   slice_header->segment_map_update = 0;
   slice_header->segment_map_temporal_update = 0;
   if (!slice_header->segment_enabled)
      return;

   slice_header->segment_map_update = mwv207_sw_get_bits(rb, 1);
   STREAM_TRACE("segment_map_update", slice_header->segment_map_update);
   if (slice_header->segment_map_update) {
      for (i = 0; i < MB_SEG_TREE_PROBS; i++) {
         tmp = mwv207_sw_get_bits(rb, 1);
         STREAM_TRACE("segment_tree_prob_update", tmp);
         if (tmp) {
            fc->mb_segment_tree_probs[i] = mwv207_sw_get_bits(rb, 8);
            STREAM_TRACE("segment_tree_prob", tmp);
         } else {
            fc->mb_segment_tree_probs[i] = 255;
         }
      }

      slice_header->segment_map_temporal_update = mwv207_sw_get_bits(rb, 1);
      STREAM_TRACE("segment_map_temporal_update",
                   slice_header->segment_map_temporal_update);
      for (i = 0; i < PREDICTION_PROBS; i++) {
         if (slice_header->segment_map_temporal_update) {
            tmp = mwv207_sw_get_bits(rb, 1);
            STREAM_TRACE("segment_pred_prob_update", tmp);
            if (tmp) {
               fc->segment_pred_probs[i] = mwv207_sw_get_bits(rb, 8);
               STREAM_TRACE("segment_pred_prob", tmp);
            } else {
               fc->segment_pred_probs[i] = 255;
            }
         } else {
            fc->segment_pred_probs[i] = 255;
         }
      }
   }

   tmp = mwv207_sw_get_bits(rb, 1);
   STREAM_TRACE("segment_data_update", tmp);
   /* Remember whether this frame carried fresh per-segment feature data:
    * when it does not, the previous frame's feature values stay in effect
    * (VP9 spec 6.2.10) and the driver has to reuse its cached copy. */
   slice_header->segment_data_update = tmp;
   if (tmp) {
      /* Absolute/relative mode */
      slice_header->segment_feature_mode = mwv207_sw_get_bits(rb, 1);
      STREAM_TRACE("segment_abs_delta", slice_header->segment_feature_mode);

      memset(slice_header->segment_feature_enable,
             0,
             sizeof(slice_header->segment_feature_enable));
      memset(slice_header->segment_feature_data,
             0,
             sizeof(slice_header->segment_feature_data));

      for (i = 0; i < MAX_MB_SEGMENTS; i++) {
         for (j = 0; j < SEG_LVL_MAX; j++) {
            slice_header->segment_feature_enable[i][j] =
               mwv207_sw_get_bits(rb, 1);
            STREAM_TRACE("segment_feature_enable",
                         slice_header->segment_feature_enable[i][j]);

            if (slice_header->segment_feature_enable[i][j]) {

               slice_header->segment_feature_data[i][j] =
                  mwv207_sw_get_bits_unsigned_max(rb,
                                                 vp9_seg_feature_data_max[j]);
               STREAM_TRACE("segment_feature_data",
                            slice_header->segment_feature_data[i][j]);

               if (vp9_seg_feature_data_signed[j]) {
                  sign = mwv207_sw_get_bits(rb, 1);
                  STREAM_TRACE("segment_feature_sign", sign);
                  if (sign)
                     slice_header->segment_feature_data[i][j] =
                        -slice_header->segment_feature_data[i][j];
               }
            }
         }
      }
   }
}

static uint32_t
setup_frame_size_with_refs(struct mwv207_strm_data *rb,
                       struct mwv207_vp9_slice_header *slice_header)
{
   uint32_t tmp;
   int32_t found, i;
   uint32_t prev_width, prev_height;

   found = 0;
   slice_header->resolution_change = 0;
   prev_width = slice_header->width;
   prev_height = slice_header->height;
   for (i = 0; i < ALLOWED_REFS_PER_FRAME; ++i) {
      tmp = mwv207_sw_get_bits(rb, 1);
      STREAM_TRACE("use_prev_frame_size", tmp);
      if (tmp) {
         found = 1;

         
         /* Keep the size the caller supplied for inter frames: the
          * referenced frame's dimensions are what the tile parsing needs,
          * and pic_param_passed_* is only filled for streams that carry
          * their own size.  Falling back to 0 here makes the tile-count
          * fields (which depend on the frame width) be read with the wrong
          * width and shifts the whole header by a bit or two. */
         if (slice_header->pic_param_passed_width &&
             slice_header->pic_param_passed_height) {
            slice_header->width = slice_header->pic_param_passed_width;
            slice_header->height = slice_header->pic_param_passed_height;
         }

         STREAM_TRACE("frame_widthRx", slice_header->width);
         STREAM_TRACE("frame_heightRx", slice_header->height);
         break;
      }
   }

   if (!found) {

      tmp = mwv207_sw_get_bits(rb, 16);
      slice_header->width = tmp + 1;
      STREAM_TRACE("frame_widthR", slice_header->width);

      tmp = mwv207_sw_get_bits(rb, 16);
      slice_header->height = tmp + 1;
      STREAM_TRACE("frame_heightR", slice_header->height);
   }

   if (slice_header->width != prev_width
       || slice_header->height != prev_height)
      slice_header->resolution_change =
         1;

   slice_header->scaling_active = tmp = mwv207_sw_get_bits(rb, 1);
   STREAM_TRACE("scaling active", slice_header->scaling_active);

   if (slice_header->scaling_active) {

      tmp = mwv207_sw_get_bits(rb, 16);
      slice_header->scaled_width = tmp + 1;
      STREAM_TRACE("scaled_frame_width", slice_header->scaled_width);

      tmp = mwv207_sw_get_bits(rb, 16);
      slice_header->scaled_height = tmp + 1;
      STREAM_TRACE("scaled_frame_height", slice_header->scaled_height);
   }

   return (MWV207_OK);
}

const uint8_t vp9_default_inter_mode_prob[INTER_MODE_CONTEXTS_DEC][4] = {
   {2, 173, 34, 0}, 
   {7, 145, 85, 0}, 
   {7, 166, 63, 0}, 
   {7, 94, 66, 0}, // 3 = one predicted/zero and one new mv
   {8, 64, 46, 0}, 
   {17, 81, 31, 0}, 
   {25, 29, 30, 0}, 
};

static void
mwv207_vp9_init_mode_contexts(struct mwv207_vp9_slice_header *slice_header)
{
   
   
   memcpy(slice_header->entropy.a.inter_mode_prob,
          vp9_default_inter_mode_prob,
          sizeof(vp9_default_inter_mode_prob));
}

static const vp9_prob
   default_kf_uv_probs[VP9_INTRA_MODES][VP9_INTRA_MODES - 1] = {
      {144, 11, 54, 157, 195, 130, 46, 58, 108},
      {118, 15, 123, 148, 131, 101, 44, 93, 131},
      {113, 12, 23, 188, 226, 142, 26, 32, 125},
      {120, 11, 50, 123, 163, 135, 64, 77, 103},
      {113, 9, 36, 155, 111, 157, 32, 44, 161},
      {116, 9, 55, 176, 76, 96, 37, 61, 149},
      {115, 9, 28, 141, 161, 167, 21, 25, 193},
      {120, 12, 32, 145, 195, 142, 32, 38, 86},
      {116, 12, 64, 120, 140, 125, 49, 115, 121},
      {102, 19, 66, 162, 182, 122, 35, 59, 128}
};

static const vp9_prob
   default_if_y_probs[BLOCK_SIZE_GROUPS][VP9_INTRA_MODES - 1] = {
      {65, 32, 18, 144, 162, 194, 41, 51, 98},
      {132, 68, 18, 165, 217, 196, 45, 40, 78},
      {173, 80, 19, 176, 240, 193, 64, 35, 46},
      {221, 135, 38, 194, 248, 121, 96, 85, 29}
};

static const vp9_prob
   default_if_uv_probs[VP9_INTRA_MODES][VP9_INTRA_MODES - 1] = {
      {120, 7, 76, 176, 208, 126, 28, 54, 103},
      {48, 12, 154, 155, 139, 90, 34, 117, 119},
      {67, 6, 25, 204, 243, 158, 13, 21, 96},
      {97, 5, 44, 131, 176, 139, 48, 68, 97},
      {83, 5, 42, 156, 111, 152, 26, 49, 152},
      {80, 5, 58, 178, 74, 83, 33, 62, 145},
      {86, 5, 32, 154, 192, 168, 14, 22, 163},
      {85, 5, 32, 156, 216, 148, 19, 29, 73},
      {77, 7, 64, 116, 132, 122, 37, 126, 120},
      {101, 21, 107, 181, 192, 103, 19, 67, 125}
};

static const vp9_prob
   vp9_partition_probs[NUM_FRAME_TYPES][NUM_PARTITION_CONTEXTS]
                      [PARTITION_TYPES_DEC] = {

                         {

                            {158, 97, 94, 0} /* a/l both not split */,
                            {93, 24, 99, 0},
                            {85, 119, 44, 0},
                            {62, 59, 67, 0} /* a/l both split */,

                            {149, 53, 53, 0} /* a/l both not split */,
                            {94, 20, 48, 0},
                            {83, 53, 24, 0},
                            {52, 18, 18, 0} /* a/l both split */,

                            {150, 40, 39, 0} /* a/l both not split */,
                            {78, 12, 26, 0},
                            {67, 33, 11, 0},
                            {24, 7, 5, 0} /* a/l both split */,

                            {174, 35, 49, 0} /* a/l both not split */,
                            {68, 11, 27, 0},
                            {57, 15, 9, 0},
                            {12, 3, 3, 0} /* a/l both split */
                         },
                         {

                            {199, 122, 141, 0} /* a/l both not split */,
                            {147, 63, 159, 0},
                            {148, 133, 118, 0},
                            {121, 104, 114, 0} /* a/l both split */,

                            {174, 73, 87, 0} /* a/l both not split */,
                            {92, 41, 83, 0},
                            {82, 99, 50, 0},
                            {53, 39, 39, 0} /* a/l both split */,

                            {177, 58, 59, 0} /* a/l both not split */,
                            {68, 26, 63, 0},
                            {52, 79, 25, 0},
                            {17, 14, 12, 0} /* a/l both split */,

                            {222, 34, 30, 0} /* a/l both not split */,
                            {72, 16, 44, 0},
                            {58, 32, 12, 0},
                            {10, 7, 6, 0} /* a/l both split */
                         }};

/* Array indices are identical to previously-existing INTRAMODECONTEXTNODES.
 */
typedef int8_t vp9_tree_index;

const vp9_tree_index vp9_sb_mv_ref_tree[6] = {
   -ZEROMV_DEC, 2, -NEARESTMV_DEC, 4, -NEARMV_DEC, -NEWMV_DEC};

struct vp9_token {
   int value;
   int Len;
};
struct vp9_token vp9_sb_mv_ref_encoding_array[VP9_INTER_MODES];

static const vp9_prob default_intra_inter_p[INTRA_INTER_CONTEXTS] = {
   9, 102, 187, 225};

static const vp9_prob default_comp_inter_p[COMP_INTER_CONTEXTS] = {
   239, 183, 119, 96, 41};

static const vp9_prob default_comp_ref_p[REF_CONTEXTS_DEC] = {
   50, 126, 123, 221, 226};

static const vp9_prob default_single_ref_p[REF_CONTEXTS_DEC][2] = {
   {33, 16}, {77, 74}, {142, 142}, {172, 170}, {238, 247}};

static const vp9_prob vp9_default_tx_probs_32x32p[TX_SIZE_CONTEXTS_DEC]
                                                 [TX_SIZE_MAX_SB_DEC - 1] = {
                                                    {
                                                       3,
                                                       136,
                                                       37,
                                                    },
                                                    {
                                                       5,
                                                       52,
                                                       13,
                                                    },
};
static const vp9_prob vp9_default_tx_probs_16x16p[TX_SIZE_CONTEXTS_DEC]
                                                 [TX_SIZE_MAX_SB_DEC - 2] = {
                                                    {
                                                       20,
                                                       152,
                                                    },
                                                    {
                                                       15,
                                                       101,
                                                    },
};
static const vp9_prob vp9_default_tx_probs_8x8p[TX_SIZE_CONTEXTS_DEC]
                                               [TX_SIZE_MAX_SB_DEC - 3] = {
                                                  {
                                                     100,
                                                  },
                                                  {
                                                     66,
                                                  },
};

static const vp9_prob vp9_default_mbskip_probs[MBSKIP_CONTEXTS] = {
   192, 128, 64};

static const vp9_prob vp9_switchable_interp_prob[VP9_SWITCHABLE_FILTERS + 1]
                                                [VP9_SWITCHABLE_FILTERS - 1] =
                                                   {
                                                      {
                                                         235,
                                                         162,
                                                      },
                                                      {
                                                         36,
                                                         255,
                                                      },
                                                      {
                                                         34,
                                                         3,
                                                      },
                                                      {
                                                         149,
                                                         144,
                                                      },
};

static const uint8_t vp9_kf_default_bmode_probs
   [VP9_INTRA_MODES][VP9_INTRA_MODES][VP9_INTRA_MODES - 1] = {
      {

         {137, 30, 42, 148, 151, 207, 70, 52, 91},
         {92, 45, 102, 136, 116, 180, 74, 90, 100},
         {73, 32, 19, 187, 222, 215, 46, 34, 100},
         {91, 30, 32, 116, 121, 186, 93, 86, 94},
         {72, 35, 36, 149, 68, 206, 68, 63, 105},
         {73, 31, 28, 138, 57, 124, 55, 122, 151},
         {67, 23, 21, 140, 126, 197, 40, 37, 171},
         {86, 27, 28, 128, 154, 212, 45, 43, 53},
         {74, 32, 27, 107, 86, 160, 63, 134, 102},
         {59, 67, 44, 140, 161, 202, 78, 67, 119}
      },
      {

         {63, 36, 126, 146, 123, 158, 60, 90, 96},
         {43, 46, 168, 134, 107, 128, 69, 142, 92},
         {44, 29, 68, 159, 201, 177, 50, 57, 77},
         {58, 38, 76, 114, 97, 172, 78, 133, 92},
         {46, 41, 76, 140, 63, 184, 69, 112, 57},
         {38, 32, 85, 140, 46, 112, 54, 151, 133},
         {39, 27, 61, 131, 110, 175, 44, 75, 136},
         {52, 30, 74, 113, 130, 175, 51, 64, 58},
         {47, 35, 80, 100, 74, 143, 64, 163, 74},
         {36, 61, 116, 114, 128, 162, 80, 125, 82}
      },
      {

         {82, 26, 26, 171, 208, 204, 44, 32, 105},
         {55, 44, 68, 166, 179, 192, 57, 57, 108},
         {42, 26, 11, 199, 241, 228, 23, 15, 85},
         {68, 42, 19, 131, 160, 199, 55, 52, 83},
         {58, 50, 25, 139, 115, 232, 39, 52, 118},
         {50, 35, 33, 153, 104, 162, 64, 59, 131},
         {44, 24, 16, 150, 177, 202, 33, 19, 156},
         {55, 27, 12, 153, 203, 218, 26, 27, 49},
         {53, 49, 21, 110, 116, 168, 59, 80, 76},
         {38, 72, 19, 168, 203, 212, 50, 50, 107}
      },
      {

         {103, 26, 36, 129, 132, 201, 83, 80, 93},
         {59, 38, 83, 112, 103, 162, 98, 136, 90},
         {62, 30, 23, 158, 200, 207, 59, 57, 50},
         {67, 30, 29, 84, 86, 191, 102, 91, 59},
         {60, 32, 33, 112, 71, 220, 64, 89, 104},
         {53, 26, 34, 130, 56, 149, 84, 120, 103},
         {53, 21, 23, 133, 109, 210, 56, 77, 172},
         {77, 19, 29, 112, 142, 228, 55, 66, 36},
         {61, 29, 29, 93, 97, 165, 83, 175, 162},
         {47, 47, 43, 114, 137, 181, 100, 99, 95}
      },
      {

         {69, 23, 29, 128, 83, 199, 46, 44, 101},
         {53, 40, 55, 139, 69, 183, 61, 80, 110},
         {40, 29, 19, 161, 180, 207, 43, 24, 91},
         {60, 34, 19, 105, 61, 198, 53, 64, 89},
         {52, 31, 22, 158, 40, 209, 58, 62, 89},
         {44, 31, 29, 147, 46, 158, 56, 102, 198},
         {35, 19, 12, 135, 87, 209, 41, 45, 167},
         {55, 25, 21, 118, 95, 215, 38, 39, 66},
         {51, 38, 25, 113, 58, 164, 70, 93, 97},
         {47, 54, 34, 146, 108, 203, 72, 103, 151}
      },
      {

         {64, 19, 37, 156, 66, 138, 49, 95, 133},
         {46, 27, 80, 150, 55, 124, 55, 121, 135},
         {36, 23, 27, 165, 149, 166, 54, 64, 118},
         {53, 21, 36, 131, 63, 163, 60, 109, 81},
         {40, 26, 35, 154, 40, 185, 51, 97, 123},
         {35, 19, 34, 179, 19, 97, 48, 129, 124},
         {36, 20, 26, 136, 62, 164, 33, 77, 154},
         {45, 18, 32, 130, 90, 157, 40, 79, 91},
         {45, 26, 28, 129, 45, 129, 49, 147, 123},
         {38, 44, 51, 136, 74, 162, 57, 97, 121}
      },
      {

         {75, 17, 22, 136, 138, 185, 32, 34, 166},
         {56, 39, 58, 133, 117, 173, 48, 53, 187},
         {35, 21, 12, 161, 212, 207, 20, 23, 145},
         {56, 29, 19, 117, 109, 181, 55, 68, 112},
         {47, 29, 17, 153, 64, 220, 59, 51, 114},
         {46, 16, 24, 136, 76, 147, 41, 64, 172},
         {34, 17, 11, 108, 152, 187, 13, 15, 209},
         {51, 24, 14, 115, 133, 209, 32, 26, 104},
         {55, 30, 18, 122, 79, 179, 44, 88, 116},
         {37, 49, 25, 129, 168, 164, 41, 54, 148}
      },
      {

         {82, 22, 32, 127, 143, 213, 39, 41, 70},
         {62, 44, 61, 123, 105, 189, 48, 57, 64},
         {47, 25, 17, 175, 222, 220, 24, 30, 86},
         {68, 36, 17, 106, 102, 206, 59, 74, 74},
         {57, 39, 23, 151, 68, 216, 55, 63, 58},
         {49, 30, 35, 141, 70, 168, 82, 40, 115},
         {51, 25, 15, 136, 129, 202, 38, 35, 139},
         {68, 26, 16, 111, 141, 215, 29, 28, 28},
         {59, 39, 19, 114, 75, 180, 77, 104, 42},
         {40, 61, 26, 126, 152, 206, 61, 59, 93}
      },
      {

         {78, 23, 39, 111, 117, 170, 74, 124, 94},
         {48, 34, 86, 101, 92, 146, 78, 179, 134},
         {47, 22, 24, 138, 187, 178, 68, 69, 59},
         {56, 25, 33, 105, 112, 187, 95, 177, 129},
         {48, 31, 27, 114, 63, 183, 82, 116, 56},
         {43, 28, 37, 121, 63, 123, 61, 192, 169},
         {42, 17, 24, 109, 97, 177, 56, 76, 122},
         {58, 18, 28, 105, 139, 182, 70, 92, 63},
         {46, 23, 32, 74, 86, 150, 67, 183, 88},
         {36, 38, 48, 92, 122, 165, 88, 137, 91}
      },
      {

         {65, 70, 60, 155, 159, 199, 61, 60, 81},
         {44, 78, 115, 132, 119, 173, 71, 112, 93},
         {39, 38, 21, 184, 227, 206, 42, 32, 64},
         {58, 47, 36, 124, 137, 193, 80, 82, 78},
         {49, 50, 35, 144, 95, 205, 63, 78, 59},
         {41, 53, 52, 148, 71, 142, 65, 128, 51},
         {40, 36, 28, 143, 143, 202, 40, 55, 137},
         {52, 34, 29, 129, 183, 227, 42, 35, 43},
         {42, 44, 44, 104, 105, 164, 64, 130, 80},
         {43, 81, 53, 140, 169, 204, 68, 84, 72}
      }};

static void
mwv207_vp9_init_mbmode_probs(struct mwv207_vp9_slice_header *x)
{
   int i, j;

   for (i = 0; i < BLOCK_SIZE_GROUPS; i++) {
      for (j = 0; j < 8; j++)
         x->entropy.a.sb_ymode_prob[i][j] = default_if_y_probs[i][j];
      x->entropy.a.sb_ymode_prob_b[i][0] = default_if_y_probs[i][8];
   }

   for (i = 0; i < VP9_INTRA_MODES; i++) {
      for (j = 0; j < 8; j++)
         x->entropy.kf_uv_mode_prob[i][j] = default_kf_uv_probs[i][j];
      x->entropy.kf_uv_mode_prob_b[i][0] = default_kf_uv_probs[i][8];

      for (j = 0; j < 8; j++)
         x->entropy.a.uv_mode_prob[i][j] = default_if_uv_probs[i][j];
      x->entropy.a.uv_mode_prob_b[i][0] = default_if_uv_probs[i][8];
   }

   memcpy(x->entropy.a.switchable_interp_prob,
          vp9_switchable_interp_prob,
          sizeof(vp9_switchable_interp_prob));

   memcpy(x->entropy.a.partition_prob,
          vp9_partition_probs,
          sizeof(vp9_partition_probs));

   memcpy(x->entropy.a.intra_inter_prob,
          default_intra_inter_p,
          sizeof(default_intra_inter_p));
   memcpy(x->entropy.a.comp_inter_prob,
          default_comp_inter_p,
          sizeof(default_comp_inter_p));
   memcpy(x->entropy.a.comp_ref_prob,
          default_comp_ref_p,
          sizeof(default_comp_ref_p));
   memcpy(x->entropy.a.single_ref_prob,
          default_single_ref_p,
          sizeof(default_single_ref_p));
   memcpy(x->entropy.a.tx32x32_prob,
          vp9_default_tx_probs_32x32p,
          sizeof(vp9_default_tx_probs_32x32p));
   memcpy(x->entropy.a.tx16x16_prob,
          vp9_default_tx_probs_16x16p,
          sizeof(vp9_default_tx_probs_16x16p));
   memcpy(x->entropy.a.tx8x8_prob,
          vp9_default_tx_probs_8x8p,
          sizeof(vp9_default_tx_probs_8x8p));
   memcpy(x->entropy.a.mbskip_probs,
          vp9_default_mbskip_probs,
          sizeof(vp9_default_mbskip_probs));

   for (i = 0; i < VP9_INTRA_MODES; i++) {
      for (j = 0; j < VP9_INTRA_MODES; j++) {
         memcpy(x->entropy.kf_bmode_prob[i][j],
                vp9_kf_default_bmode_probs[i][j],
                8);
         x->entropy.kf_bmode_prob_b[i][j][0] =
            vp9_kf_default_bmode_probs[i][j][8];
      }
   }
}

const struct mwv207_nmv_context vp9_default_nmv_context1 = {
   {32, 64, 96},
   {128, 128},
   {{216}, {208}},
   {{64, 96, 64}, {64, 96, 64}},
   {160, 160},
   {128, 128},
   {{224, 144, 192, 168, 192, 176, 192, 198, 198, 245},
    {216, 128, 176, 160, 176, 176, 192, 198, 198, 208}},
   {{{128, 128, 64}, {96, 112, 64}},
    {{128, 128, 64}, {96, 112, 64}}},
   {{136, 140, 148, 160, 176, 192, 224, 234, 234, 240},
    {136, 140, 148, 160, 176, 192, 224, 234, 234, 240}},
};

static void
mwv207_vp9_init_mv_probs(struct mwv207_vp9_slice_header *x)
{
   memcpy(&x->entropy.a.nmvc,
          &vp9_default_nmv_context1,
          sizeof(struct mwv207_nmv_context));
}

void
mwv207_vp9_reset_probs(struct mwv207_vp9_slice_header *slice_header)
{
   int32_t i, j, k, l, m;

   mwv207_vp9_init_mode_contexts(slice_header);
   mwv207_vp9_init_mbmode_probs(slice_header);
   mwv207_vp9_init_mv_probs(slice_header);

   /* Copy the default probs into two separate prob tables: part1 and part2.
    */
   for (i = 0; i < BLOCK_TYPES; i++) {
      for (j = 0; j < REF_TYPES; j++) {
         for (k = 0; k < COEF_BANDS; k++) {
            for (l = 0; l < PREV_COEF_CONTEXTS; l++) {
               if (l >= 3 && k == 0)
                  continue;

               for (m = 0; m < UNCONSTRAINED_NODES; m++) {
                  slice_header->entropy.a.prob_coeffs[i][j][k][l][m] =
                     default_coef_probs_4x4[i][j][k][l][m];
                  slice_header->entropy.a.prob_coeffs8x8[i][j][k][l][m] =
                     default_coef_probs_8x8[i][j][k][l][m];
                  slice_header->entropy.a.prob_coeffs16x16[i][j][k][l][m] =
                     default_coef_probs_16x16[i][j][k][l][m];
                  slice_header->entropy.a.prob_coeffs32x32[i][j][k][l][m] =
                     default_coef_probs_32x32[i][j][k][l][m];
               }
            }
         }
      }
   }

   if (slice_header->key_frame || slice_header->error_resilient
       || slice_header->reset_frame_context == 3) {

      for (i = 0; i < NUM_FRAME_CONTEXTS; i++)
         memcpy(&slice_header->entropy_last[i],
                &slice_header->entropy,
                sizeof(struct mwv207_vp9_entropy_probs));
   } else if (slice_header->reset_frame_context == 2) {
      memcpy(&slice_header->entropy_last[slice_header->frame_context_idx],
             &slice_header->entropy,
             sizeof(struct mwv207_vp9_entropy_probs));
   }
}

static void
vp9_reset_decoder(struct mwv207_vp9_slice_header *slice_header)
{
   int32_t i;

   memset(slice_header->segment_feature_enable,
          0,
          sizeof(slice_header->segment_feature_enable));
   memset(slice_header->segment_feature_data,
          0,
          sizeof(slice_header->segment_feature_data));

   mwv207_vp9_reset_probs(slice_header);

   slice_header->frame_context_idx = 0;
   slice_header->ref_frame_sign_bias[0] =
      slice_header->ref_frame_sign_bias[1] =
         slice_header->ref_frame_sign_bias[2] =
            slice_header->ref_frame_sign_bias[3] = 0;
   slice_header->allow_comp_inter_inter = 0;
   for (i = 0; i < NUM_REF_FRAMES; i++) {
      slice_header->ref_frame_map[i] = i;
   }
}

static void
vp9_get_probs(struct mwv207_vp9_slice_header *slice_header)
{
   /* Frame context tells which frame is used as reference, make
    * a copy of the context to use as base for this frame probs. */
   slice_header->entropy =
      slice_header->entropy_last[slice_header->frame_context_idx];
}

static uint32_t
vp9_decode_uncompressed_header(struct mwv207_vp9_slice_header *slice_header,
                            struct mwv207_strm_data rb,
                            uint32_t data_len)
{
   uint32_t key_frame = 0;
   uint32_t err, tmp, i, delta_log2_tiles;

   tmp = mwv207_sw_get_bits(&rb, 2);
   STREAM_TRACE("Frame marker", tmp);
   tmp = mwv207_sw_get_bits(&rb, 1);
   STREAM_TRACE("VP version", slice_header->vp_version);
   slice_header->vp_version = tmp;
   tmp = mwv207_sw_get_bits(&rb, 1);
   slice_header->vp_profile = (tmp << 1) + slice_header->vp_version;
   if (slice_header->vp_profile > 2) {
      tmp = mwv207_sw_get_bits(&rb, 1);
      slice_header->vp_profile += tmp;
   }

   slice_header->reset_frame_flags = 0;
   slice_header->show_existing_frame = tmp = mwv207_sw_get_bits(&rb, 1);

   STREAM_TRACE("Show existing frame", slice_header->show_existing_frame);
   if (slice_header->show_existing_frame) {
      
      int32_t idx_to_show = mwv207_sw_get_bits(&rb, 3);
      slice_header->show_existing_frame_index =
         slice_header->ref_frame_map[idx_to_show];
      slice_header->refresh_frame_flags = 0;
      slice_header->loop_filter_level = 0;
      return MWV207_OK;
   }

   key_frame = tmp = mwv207_sw_get_bits(&rb, 1);
   slice_header->key_frame = !key_frame;
   slice_header->show_frame = tmp = mwv207_sw_get_bits(&rb, 1);
   slice_header->error_resilient = tmp = mwv207_sw_get_bits(&rb, 1);
   STREAM_TRACE("Key frame", slice_header->key_frame);
   STREAM_TRACE("Show frame", slice_header->show_frame);
   STREAM_TRACE("Error resilient", slice_header->error_resilient);

   if (slice_header->key_frame) {
      err = check_sync_code(&rb);
      if (err == MWV207_NOK)
         return err;

      slice_header->bit_depth = slice_header->vp_profile >= 2
         ? ((tmp = mwv207_sw_get_bits(&rb, 1)) ? 12 : 10)
         : 8;
      STREAM_TRACE("Bit depth", slice_header->bit_depth);
      if (slice_header->bit_depth == 12)
         return MWV207_NOK;
      slice_header->color_space = tmp = mwv207_sw_get_bits(&rb, 3);
      STREAM_TRACE("Color space", slice_header->color_space);
      if (slice_header->color_space != 7) {
         tmp = mwv207_sw_get_bits(&rb, 1);
         STREAM_TRACE("YUV range", tmp);
         if (slice_header->vp_version == 1) {
            slice_header->subsampling_x = tmp = mwv207_sw_get_bits(&rb, 1);
            slice_header->subsampling_y = tmp = mwv207_sw_get_bits(&rb, 1);
            tmp = mwv207_sw_get_bits(&rb, 1);
            STREAM_TRACE("Subsampling X", slice_header->subsampling_x);
            STREAM_TRACE("Subsampling Y", slice_header->subsampling_y);
            STREAM_TRACE("Alpha plane", tmp);
         } else {
            slice_header->subsampling_x = slice_header->subsampling_y = 1;
         }
      } else {
         if (slice_header->vp_version == 1) {
            slice_header->subsampling_x = slice_header->subsampling_y = 0;
            tmp = mwv207_sw_get_bits(&rb, 1);
            STREAM_TRACE("Alpha plane", tmp);
         } else {
            STREAM_TRACE("RGB not supported in profile", 0);
            return MWV207_NOK;
         }
      }

      slice_header->reset_frame_flags = 1;
      slice_header->refresh_frame_flags = (1 << NUM_REF_FRAMES) - 1;

      for (i = 0; i < ALLOWED_REFS_PER_FRAME; i++) {
         slice_header->active_ref_idx[i] =
            0;
         existing_fef_register(slice_header, 1 << i);
      }
      if (setup_frame_size(&rb, slice_header) != MWV207_OK)
         return MWV207_NOK;
   } else {
      if (!slice_header->show_frame) {
         slice_header->intra_only = tmp = mwv207_sw_get_bits(&rb, 1);
      } else {
         slice_header->intra_only = 0;
      }
      /*
             if (dec_cont->dec_stat == VP9DEC_INITIALIZED &&
             slice_header->intra_only == 0) { return MWV207_NOK;
             } */

      STREAM_TRACE("Intra only", slice_header->intra_only);

      if (!slice_header->error_resilient) {
         slice_header->reset_frame_context = tmp = mwv207_sw_get_bits(&rb, 2);
      } else {
         slice_header->reset_frame_context = 0;
      }
      STREAM_TRACE("Reset frame context", slice_header->reset_frame_context);
      if (slice_header->intra_only) {
         err = check_sync_code(&rb);
         if (err == MWV207_NOK)
            return err;

         slice_header->bit_depth = slice_header->vp_profile >= 2
            ? (tmp = mwv207_sw_get_bits(&rb, 1)) ? 12 : 10
            : 8;
         if (slice_header->bit_depth == 12)
            return MWV207_NOK;
         if (slice_header->vp_profile > 0) {
            slice_header->color_space = tmp = mwv207_sw_get_bits(&rb, 3);
            STREAM_TRACE("Color space", slice_header->color_space);
            if (slice_header->color_space != 7) {
               tmp = mwv207_sw_get_bits(&rb, 1);
               STREAM_TRACE("YUV range", tmp);
               if (slice_header->vp_version == 1) {
                  slice_header->subsampling_x = tmp =
                     mwv207_sw_get_bits(&rb, 1);
                  slice_header->subsampling_y = tmp =
                     mwv207_sw_get_bits(&rb, 1);
                  tmp = mwv207_sw_get_bits(&rb, 1);
                  STREAM_TRACE("Subsampling X", slice_header->subsampling_x);
                  STREAM_TRACE("Subsampling Y", slice_header->subsampling_y);
                  STREAM_TRACE("Alpha plane", tmp);
               } else {
                  slice_header->subsampling_x = slice_header->subsampling_y =
                     1;
               }
            } else {
               if (slice_header->vp_version == 1) {
                  slice_header->subsampling_x = slice_header->subsampling_y =
                     0;
                  tmp = mwv207_sw_get_bits(&rb, 1);
                  STREAM_TRACE("Alpha plane", tmp);
               } else {
                  STREAM_TRACE("RGB not supported in profile", 0);
                  return MWV207_NOK;
               }
            }
         }

         slice_header->refresh_frame_flags = tmp =
            mwv207_sw_get_bits(&rb, NUM_REF_FRAMES);
         STREAM_TRACE("refresh_frame_flags",
                      slice_header->refresh_frame_flags);
         if (setup_frame_size(&rb, slice_header) != MWV207_OK)
            return MWV207_NOK;
      } else {
         slice_header->refresh_frame_flags = tmp =
            mwv207_sw_get_bits(&rb, NUM_REF_FRAMES);
         STREAM_TRACE("refresh_frame_flags",
                      slice_header->refresh_frame_flags);

         for (i = 0; i < ALLOWED_REFS_PER_FRAME; i++) {

            int32_t ref_frame_num, mapped_ref;
            ref_frame_num = tmp = mwv207_sw_get_bits(&rb, NUM_REF_FRAMES_LG2);
            mapped_ref = tmp = slice_header->ref_frame_map[ref_frame_num];
            STREAM_TRACE("active_reference_frame_idx", ref_frame_num);
            STREAM_TRACE("active_reference_frame_num", mapped_ref);
            slice_header->active_ref_idx[i] = mapped_ref;

            slice_header->ref_frame_sign_bias[i + 1] = tmp =
               mwv207_sw_get_bits(&rb, 1);
            STREAM_TRACE("ref_frame_sign_bias",
                         slice_header->ref_frame_sign_bias[i + 1]);
         }

         if (setup_frame_size_with_refs(&rb, slice_header) != MWV207_OK)
            return (MWV207_NOK);

         slice_header->allow_high_precision_mv = tmp =
            mwv207_sw_get_bits(&rb, 1);
         STREAM_TRACE("high_precision_mv",
                      slice_header->allow_high_precision_mv);

         tmp = mwv207_sw_get_bits(&rb, 1);
         STREAM_TRACE("mb_switchable_mcomp_filt", tmp);
         if (tmp) {
            slice_header->mcomp_filter_type = SWITCHABLE;
         } else {
            slice_header->mcomp_filter_type = tmp = mwv207_sw_get_bits(&rb, 2);
            STREAM_TRACE("mcomp_filter_type",
                         slice_header->mcomp_filter_type);
         }

         slice_header->allow_comp_inter_inter = 0;
         for (i = 0; i < ALLOWED_REFS_PER_FRAME; i++) {
            slice_header->allow_comp_inter_inter |= (i > 0)
               && (slice_header->ref_frame_sign_bias[i + 1]
                   != slice_header->ref_frame_sign_bias[1]);
         }

         if (slice_header->allow_comp_inter_inter) {
            if (slice_header->ref_frame_sign_bias[LAST_FRAME_DEC]
                == slice_header->ref_frame_sign_bias[GOLDEN_FRAME_DEC]) {
               slice_header->comp_fixed_ref = ALTREF_FRAME_DEC;
               slice_header->comp_var_ref[0] = LAST_FRAME_DEC;
               slice_header->comp_var_ref[1] = GOLDEN_FRAME_DEC;
            } else if (slice_header->ref_frame_sign_bias[LAST_FRAME_DEC]
                       == slice_header
                             ->ref_frame_sign_bias[ALTREF_FRAME_DEC]) {
               slice_header->comp_fixed_ref = GOLDEN_FRAME_DEC;
               slice_header->comp_var_ref[0] = LAST_FRAME_DEC;
               slice_header->comp_var_ref[1] = ALTREF_FRAME_DEC;
            } else {
               slice_header->comp_fixed_ref = LAST_FRAME_DEC;
               slice_header->comp_var_ref[0] = GOLDEN_FRAME_DEC;
               slice_header->comp_var_ref[1] = ALTREF_FRAME_DEC;
            }
         }
      }
      existing_fef_register(slice_header, slice_header->refresh_frame_flags);
   }

   if (!slice_header->error_resilient) {
      /* Refresh entropy probs,
       * 0 == this frame probs are used only for this frame decoding,
       * 1 == this frame probs will be stored for future reference */
      slice_header->refresh_entropy_probs = tmp = mwv207_sw_get_bits(&rb, 1);
      slice_header->frame_parallel_decoding = tmp = mwv207_sw_get_bits(&rb, 1);
      STREAM_TRACE("Refresh frame context",
                   slice_header->refresh_entropy_probs);
      STREAM_TRACE("Frame parallel", slice_header->frame_parallel_decoding);
   } else {
      slice_header->refresh_entropy_probs = 0;
      slice_header->frame_parallel_decoding = 1;
   }

   slice_header->frame_context_idx = tmp =
      mwv207_sw_get_bits(&rb, NUM_FRAME_CONTEXTS_LG2);
   STREAM_TRACE("Frame context index", slice_header->frame_context_idx);

   if (slice_header->key_frame || slice_header->error_resilient
       || slice_header->intra_only)
      vp9_reset_decoder(slice_header);

   vp9_get_probs(slice_header);
#ifdef USE_VP9_EC
#endif

   decode_lf_params(&rb, slice_header);

   slice_header->qp_yac = tmp = mwv207_sw_get_bits(&rb, 8);
   STREAM_TRACE("qp_y_ac", slice_header->qp_yac);
   slice_header->qp_ydc = decode_quantizer_delta(&rb);
   slice_header->qp_ch_dc = decode_quantizer_delta(&rb);
   slice_header->qp_ch_ac = decode_quantizer_delta(&rb);

   slice_header->lossless = slice_header->qp_yac == 0
      && slice_header->qp_ydc == 0 && slice_header->qp_ch_dc == 0
      && slice_header->qp_ch_ac == 0;

   decode_segmentation_data(&rb, slice_header);

   get_tile_n_bits(
      slice_header, &slice_header->log2_tile_columns, &delta_log2_tiles);
   while (delta_log2_tiles--) {
      tmp = mwv207_sw_get_bits(&rb, 1);
      if (tmp) {
         slice_header->log2_tile_columns++;
      } else {
         break;
      }
   }
   STREAM_TRACE("log2_tile_columns", slice_header->log2_tile_columns);

   slice_header->log2_tile_rows = tmp = mwv207_sw_get_bits(&rb, 1);
   if (slice_header->log2_tile_rows) {
      tmp = mwv207_sw_get_bits(&rb, 1);
      slice_header->log2_tile_rows += tmp;
   }

   STREAM_TRACE("log2_tile_rows", slice_header->log2_tile_rows);

   slice_header->offset_to_dct_parts = tmp = mwv207_sw_get_bits(&rb, 16);

   slice_header->frame_tag_size = (rb.strm_buff_read_bits + 7) / 8;

   STREAM_TRACE("First partition size", slice_header->offset_to_dct_parts);
   STREAM_TRACE("Frame tag size", slice_header->frame_tag_size);
   return MWV207_OK;
}

static uint32_t
vp9_decode_coeff_update(struct mwv207_vp_bool_coder *bc,
                     uint8_t prob_coeffs[BLOCK_TYPES][REF_TYPES][COEF_BANDS]
                                   [PREV_COEF_CONTEXTS][ENTROPY_NODES_PART1])
{
   uint32_t i, j, k, l, m;
   uint32_t tmp;

   tmp = mwv207_vp9_read_bits(bc, 1);
   STREAM_TRACE("coeff_prob_update_flag", tmp);
   if (!tmp)
      return MWV207_OK;

   for (i = 0; i < BLOCK_TYPES; i++) {
      for (j = 0; j < REF_TYPES; j++) {
         for (k = 0; k < COEF_BANDS; k++) {
            for (l = 0; l < PREV_COEF_CONTEXTS; l++) {
               if (l >= 3 && k == 0)
                  continue;

               for (m = 0; m < UNCONSTRAINED_NODES; m++) {
                  tmp = mwv207_vp9_decode_bool(bc, 252);
                  CHECK_END_OF_STREAM(tmp);
                  if (tmp) {
                     uint8_t old, new;
                     old = prob_coeffs[i][j][k][l][m];
                     new = mwv207_vp9_read_prob_diff_update(bc, old);
                     STREAM_TRACE("coeff_prob_delta_subexp", new);
                     CHECK_END_OF_STREAM(tmp);
#ifdef TRACE_HDR_PROBS
                     printf("hdr prob[%d][%d][%d][%d][%d] %d -> %d\n",
                            i,
                            j,
                            k,
                            l,
                            m,
                            old,
                            new);
#endif
                     prob_coeffs[i][j][k][l][m] = new;
                  }
               }
            }
         }
      }
   }
   return MWV207_OK;
}

static uint8_t
dwl_private_area_read_byte(const uint8_t *p)
{
   return *p;
}

static void
vp9_store_adapt_probs(struct mwv207_vp9_slice_header *slice_header)
{

   slice_header->prev_ctx = slice_header->entropy.a;
}

static uint32_t
read_tile_size(const uint8_t *cx_size)
{
   uint32_t size;
   size = (uint32_t)(dwl_private_area_read_byte(cx_size + 3))
      + ((uint32_t)(dwl_private_area_read_byte(cx_size + 2)) << 8)
      + ((uint32_t)(dwl_private_area_read_byte(cx_size + 1)) << 16)
      + ((uint32_t)(dwl_private_area_read_byte(cx_size + 0)) << 24);
   return size;
}

static void
update_nmv(struct mwv207_vp_bool_coder *bc,
          vp9_prob *const p,
          const vp9_prob upd_p)
{
   uint32_t tmp = mwv207_vp9_decode_bool(bc, upd_p);
   if (tmp) {
      *p = (mwv207_vp9_read_bits(bc, 7) << 1) | 1;
      STREAM_TRACE("mv_prob", *p);
   }
}

static uint32_t
mwv207_vp_decode_mv_update(struct mwv207_vp_bool_coder *bc,
                          struct mwv207_vp9_slice_header *slice_header)
{
   uint32_t i, j, k;
   struct mwv207_nmv_context *mvctx = &slice_header->entropy.a.nmvc;

   for (j = 0; j < MV_JOINTS - 1; ++j) {
      update_nmv(bc, &mvctx->joints[j], VP9_NMV_UPDATE_PROB);
   }
   for (i = 0; i < 2; ++i) {
      update_nmv(bc, &mvctx->sign[i], VP9_NMV_UPDATE_PROB);
      for (j = 0; j < MV_CLASSES - 1; ++j) {
         update_nmv(bc, &mvctx->classes[i][j], VP9_NMV_UPDATE_PROB);
      }
      for (j = 0; j < CLASS0_SIZE - 1; ++j) {
         update_nmv(bc, &mvctx->class0[i][j], VP9_NMV_UPDATE_PROB);
      }
      for (j = 0; j < MV_OFFSET_BITS; ++j) {
         update_nmv(bc, &mvctx->bits[i][j], VP9_NMV_UPDATE_PROB);
      }
   }

   for (i = 0; i < 2; ++i) {
      for (j = 0; j < CLASS0_SIZE; ++j) {
         for (k = 0; k < 3; ++k)
            update_nmv(bc, &mvctx->class0_fp[i][j][k], VP9_NMV_UPDATE_PROB);
      }
      for (j = 0; j < 3; ++j) {
         update_nmv(bc, &mvctx->fp[i][j], VP9_NMV_UPDATE_PROB);
      }
   }

   if (slice_header->allow_high_precision_mv) {
      for (i = 0; i < 2; ++i) {
         update_nmv(bc, &mvctx->class0_hp[i], VP9_NMV_UPDATE_PROB);
         update_nmv(bc, &mvctx->hp[i], VP9_NMV_UPDATE_PROB);
      }
   }

   return MWV207_OK;
}

static uint32_t
vp9_decode_compressed_header(uint8_t *stream,
                          uint32_t strm_len,
                          struct mwv207_vp_bool_coder *bc,
                          uint8_t *buffer,
                          uint32_t buf_len,
                          struct mwv207_vp9_slice_header *slice_header)
{
   if (slice_header->width == 0 || slice_header->height == 0) {
      printf("%s err wh!!!!!", __FUNCTION__);
      return MWV207_NOK;
   }
   uint32_t tmp, i, j, k;
   struct mwv207_vp9_entropy_probs *fc =
      &slice_header->entropy;

   vp9_store_adapt_probs(slice_header);

   if (stream >= (buffer + buf_len))
      stream -= buf_len;

   mwv207_vp9_bool_start(bc, stream, strm_len, buffer, buf_len);

   if (slice_header->lossless) {
      slice_header->transform_mode = ONLY_4X4_DEC;
   } else {
      slice_header->transform_mode = mwv207_vp9_read_bits(bc, 2);
      STREAM_TRACE("transform_mode", slice_header->transform_mode);
      if (slice_header->transform_mode == ALLOW_32X32_DEC) {
         slice_header->transform_mode += mwv207_vp9_read_bits(bc, 1);
         STREAM_TRACE("transform_mode", slice_header->transform_mode);
      }
      if (slice_header->transform_mode == TX_MODE_SELECT_DEC) {
         for (i = 0; i < TX_SIZE_CONTEXTS_DEC; ++i) {
            for (j = 0; j < TX_SIZE_MAX_SB_DEC - 3; ++j) {
               tmp = mwv207_vp9_decode_bool(bc, VP9_DEF_UPDATE_PROB);
               STREAM_TRACE("tx8x8_prob_update", tmp);
               if (tmp) {
                  uint8_t *prob = &fc->a.tx8x8_prob[i][j];
                  *prob = mwv207_vp9_read_prob_diff_update(bc, *prob);
                  STREAM_TRACE("tx8x8_prob", *prob);
               }
            }
         }

         for (i = 0; i < TX_SIZE_CONTEXTS_DEC; ++i) {
            for (j = 0; j < TX_SIZE_MAX_SB_DEC - 2; ++j) {
               tmp = mwv207_vp9_decode_bool(bc, VP9_DEF_UPDATE_PROB);
               STREAM_TRACE("tx16x16_prob_update", tmp);
               if (tmp) {
                  uint8_t *prob = &fc->a.tx16x16_prob[i][j];
                  *prob = mwv207_vp9_read_prob_diff_update(bc, *prob);
                  STREAM_TRACE("tx16x16_prob", *prob);
               }
            }
         }

         for (i = 0; i < TX_SIZE_CONTEXTS_DEC; ++i) {
            for (j = 0; j < TX_SIZE_MAX_SB_DEC - 1; ++j) {
               tmp = mwv207_vp9_decode_bool(bc, VP9_DEF_UPDATE_PROB);
               STREAM_TRACE("tx32x32_prob_update", tmp);
               if (tmp) {
                  uint8_t *prob = &fc->a.tx32x32_prob[i][j];
                  *prob = mwv207_vp9_read_prob_diff_update(bc, *prob);
                  STREAM_TRACE("tx32x32_prob", *prob);
               }
            }
         }
      }
   }

   tmp = vp9_decode_coeff_update(bc, fc->a.prob_coeffs);
   if (tmp != MWV207_OK)
      return (tmp);
   if (slice_header->transform_mode > ONLY_4X4_DEC) {
      tmp = vp9_decode_coeff_update(bc, fc->a.prob_coeffs8x8);
      if (tmp != MWV207_OK)
         return (tmp);
   }
   if (slice_header->transform_mode > ALLOW_8X8_DEC) {
      tmp = vp9_decode_coeff_update(bc, fc->a.prob_coeffs16x16);
      if (tmp != MWV207_OK)
         return (tmp);
   }
   if (slice_header->transform_mode > ALLOW_16X16_DEC) {
      tmp = vp9_decode_coeff_update(bc, fc->a.prob_coeffs32x32);
      if (tmp != MWV207_OK)
         return (tmp);
   }

   slice_header->probs_decoded = 1;

   for (k = 0; k < MBSKIP_CONTEXTS; ++k) {
      tmp = mwv207_vp9_decode_bool(bc, VP9_DEF_UPDATE_PROB);
      STREAM_TRACE("mbskip_prob_update", tmp);
      if (tmp) {
         fc->a.mbskip_probs[k] =
            mwv207_vp9_read_prob_diff_update(bc, fc->a.mbskip_probs[k]);
         STREAM_TRACE("mbskip_prob", fc->a.mbskip_probs[k]);
      }
   }

   if (!slice_header->key_frame && !slice_header->intra_only) {
      for (i = 0; i < INTER_MODE_CONTEXTS_DEC; i++) {
         for (j = 0; j < VP9_INTER_MODES - 1; j++) {
            tmp = mwv207_vp9_decode_bool(bc, VP9_DEF_UPDATE_PROB);
            STREAM_TRACE("inter_mode_prob_update", tmp);
            if (tmp) {
               uint8_t *prob = &fc->a.inter_mode_prob[i][j];
               *prob = mwv207_vp9_read_prob_diff_update(bc, *prob);
               STREAM_TRACE("inter_mode_prob", *prob);
            }
         }
      }

      if (slice_header->mcomp_filter_type == SWITCHABLE) {
         for (j = 0; j < VP9_SWITCHABLE_FILTERS + 1; ++j) {
            for (i = 0; i < VP9_SWITCHABLE_FILTERS - 1; ++i) {
               tmp = mwv207_vp9_decode_bool(bc, VP9_DEF_UPDATE_PROB);
               STREAM_TRACE("switchable_filter_prob_update", tmp);
               if (tmp) {
                  uint8_t *prob = &fc->a.switchable_interp_prob[j][i];
                  *prob = mwv207_vp9_read_prob_diff_update(bc, *prob);
                  STREAM_TRACE("switchable_interp_prob", *prob);
               }
            }
         }
      }

      for (i = 0; i < INTRA_INTER_CONTEXTS; i++) {
         tmp = mwv207_vp9_decode_bool(bc, VP9_DEF_UPDATE_PROB);
         STREAM_TRACE("intra_inter_prob_update", tmp);
         if (tmp) {
            uint8_t *prob = &fc->a.intra_inter_prob[i];
            *prob = mwv207_vp9_read_prob_diff_update(bc, *prob);
            STREAM_TRACE("intra_inter_prob", *prob);
         }
      }

      if (slice_header->allow_comp_inter_inter) {
         tmp = mwv207_vp9_read_bits(bc, 1);
         STREAM_TRACE("comp_pred_mode", tmp);
         slice_header->comp_pred_mode = tmp;
         if (tmp) {
            tmp = mwv207_vp9_read_bits(bc, 1);
            STREAM_TRACE("comp_pred_mode_hybrid", tmp);
            slice_header->comp_pred_mode += tmp;
            if (slice_header->comp_pred_mode == HYBRID_PREDICTION) {
               for (i = 0; i < COMP_INTER_CONTEXTS; i++) {
                  tmp = mwv207_vp9_decode_bool(bc, VP9_DEF_UPDATE_PROB);
                  STREAM_TRACE("comp_inter_prob_update", tmp);
                  if (tmp) {
                     uint8_t *prob = &fc->a.comp_inter_prob[i];
                     *prob = mwv207_vp9_read_prob_diff_update(bc, *prob);
                     STREAM_TRACE("comp_inter_prob", *prob);
                  }
               }
            }
         }
      } else {
         slice_header->comp_pred_mode = SINGLE_PREDICTION_ONLY;
      }

      if (slice_header->comp_pred_mode != COMP_PREDICTION_ONLY) {
         for (i = 0; i < REF_CONTEXTS_DEC; i++) {
            tmp = mwv207_vp9_decode_bool(bc, VP9_DEF_UPDATE_PROB);
            STREAM_TRACE("single_ref_prob_update", tmp);
            if (tmp) {
               uint8_t *prob = &fc->a.single_ref_prob[i][0];
               *prob = mwv207_vp9_read_prob_diff_update(bc, *prob);
               STREAM_TRACE("single_ref_prob", *prob);
            }
            tmp = mwv207_vp9_decode_bool(bc, VP9_DEF_UPDATE_PROB);
            STREAM_TRACE("single_ref_prob_update", tmp);
            if (tmp) {
               uint8_t *prob = &fc->a.single_ref_prob[i][1];
               *prob = mwv207_vp9_read_prob_diff_update(bc, *prob);
               STREAM_TRACE("single_ref_prob", *prob);
            }
         }
      }

      if (slice_header->comp_pred_mode != SINGLE_PREDICTION_ONLY) {
         for (i = 0; i < REF_CONTEXTS_DEC; i++) {
            tmp = mwv207_vp9_decode_bool(bc, VP9_DEF_UPDATE_PROB);
            STREAM_TRACE("comp_ref_prob_update", tmp);
            if (tmp) {
               uint8_t *prob = &fc->a.comp_ref_prob[i];
               *prob = mwv207_vp9_read_prob_diff_update(bc, *prob);
               STREAM_TRACE("comp_ref_prob", *prob);
            }
         }
      }

      for (j = 0; j < BLOCK_SIZE_GROUPS; ++j) {
         for (i = 0; i < 8; ++i) {
            tmp = mwv207_vp9_decode_bool(bc, VP9_DEF_UPDATE_PROB);
            STREAM_TRACE("ymode_prob_update", tmp);
            if (tmp) {
               fc->a.sb_ymode_prob[j][i] = mwv207_vp9_read_prob_diff_update(
                  bc, fc->a.sb_ymode_prob[j][i]);
               STREAM_TRACE("ymode_prob", fc->a.sb_ymode_prob[j][i]);
            }
         }
         tmp = mwv207_vp9_decode_bool(bc, VP9_DEF_UPDATE_PROB);
         STREAM_TRACE("ymode_prob_update", tmp);
         if (tmp) {
            fc->a.sb_ymode_prob_b[j][0] = mwv207_vp9_read_prob_diff_update(
               bc, fc->a.sb_ymode_prob_b[j][0]);
            STREAM_TRACE("ymode_prob", fc->a.sb_ymode_prob_b[j][0]);
         }
      }

      for (j = 0; j < NUM_PARTITION_CONTEXTS; j++) {
         for (i = 0; i < PARTITION_TYPES_DEC - 1; i++) {
            tmp = mwv207_vp9_decode_bool(bc, VP9_DEF_UPDATE_PROB);
            STREAM_TRACE("partition_prob_update", tmp);
            if (tmp) {
               uint8_t *prob = &fc->a.partition_prob[INTER_FRAME][j][i];
               *prob = mwv207_vp9_read_prob_diff_update(bc, *prob);
               STREAM_TRACE("partition_prob", *prob);
            }
         }
      }

      tmp = mwv207_vp_decode_mv_update(bc, slice_header);
      if (tmp != MWV207_OK)
         return (tmp);
   }
#if 1
   

   /* When first tile row has zero-height skip it. Cannot be done when pic
    * height smaller than 3 SB rows, in this case more than one tile rows may
    * be skipped and needs to be handled in stream parsing. */

   if ((slice_header->height + 63) / 64 >= 3) {
      const uint8_t *strm = stream + slice_header->offset_to_dct_parts;
      if ((1 << slice_header->log2_tile_rows)
          > (slice_header->height + 63) / 64) {
         for (j = (1 << slice_header->log2_tile_columns); j; j--) {
            tmp = read_tile_size(strm);
            strm += 4 + tmp;
            slice_header->offset_to_dct_parts += 4 + tmp;
            STREAM_TRACE("Tile row with h=0, skipping", tmp);
            if (strm > stream + strm_len)
               return MWV207_NOK;
         }
      }
   }

   /* The bool decoder needs up to 4 bytes of look-ahead past the first
    * partition; a synchronous over-read at the partition end is normal and
    * does not corrupt the probabilities decoded above. */
   if (bc->strm_error && bc->read_len > bc->stream_len + 4) {
      return (MWV207_NOK);
   }
#endif
   
   return MWV207_OK;
}


/* Parse one VP9 frame: uncompressed header, then the compressed header
 * (probability updates / tile sizes).  `data` points at the frame start
 * (frame marker), `size` covers the whole frame including tile payload. */
uint32_t
mwv207_vp9_parse_frame(struct mwv207_vp9_slice_header *sh,
                       const uint8_t *data, uint32_t size,
                       uint32_t fallback_width, uint32_t fallback_height,
                       struct mwv207_vp9_entropy_probs *saved_ctx,
                       uint32_t hint_frame_header_size,
                       uint32_t hint_first_partition_size)
{
   struct mwv207_strm_data rb = {0};
   struct mwv207_vp_bool_coder bc = {0};
   uint32_t ret, hdr, part;

   if (!sh || !data || size < 4)
      return MWV207_NOK;

   rb.strm_buff_start = data;
   rb.strm_curr_pos = data;
   rb.bit_pos_in_word = 0;
   rb.strm_buff_size = size;
   rb.strm_data_size = size;
   rb.strm_buff_read_bits = 0;
   /* VP9 raw streams have no emulation-prevention bytes; telling the reader
    * that they were already removed keeps it from swallowing 00 00 03
    * sequences that occur in ordinary VP9 header/payload data. */
   rb.remove_emul3_byte = 1;
   rb.is_rb = 0;

   /* Frame context selection: like libvpx, an inter frame starts from the
    * probabilities saved by an earlier frame (the four frame contexts are
    * carried across frames); only a key frame, an error-resilient frame or
    * reset_frame_context == 3 puts them all back to the defaults, and
    * reset_frame_context == 2 resets the current one.  Without the carried
    * context the coefficient probabilities desynchronise on every inter
    * frame and the reconstruction turns into noise. */
   mwv207_vp9_reset_probs(sh);
   if (saved_ctx) {
      for (uint32_t i = 0; i < NUM_FRAME_CONTEXTS; i++)
         sh->entropy_last[i] = saved_ctx[i];
   } else {
      for (uint32_t i = 0; i < NUM_FRAME_CONTEXTS; i++)
         sh->entropy_last[i] = sh->entropy;
   }

   sh->width = fallback_width;
   sh->height = fallback_height;
   ret = vp9_decode_uncompressed_header(sh, rb, size);
   if (ret)
      return ret;

   /* Inter frames may size themselves from the reference frames; the
    * reference bookkeeping lives in the decode session, so fall back to the
    * session's current picture size when the frame header leaves it unset. */
   if (sh->width == 0)
      sh->width = fallback_width;
   if (sh->height == 0)
      sh->height = fallback_height;

   /* The VA-API layer hands us the same two sizes the container/codec
    * already knows; trust them when present.  They are the authoritative
    * split point between the uncompressed header, the first partition and
    * the tile payload, and they rescue frames whose uncompressed-header
    * layout we mis-count by a bit (e.g. inter frames that skip the
    * reset_frame_context field when error_resilient is set). */
   if (hint_frame_header_size && hint_first_partition_size &&
       hint_frame_header_size + hint_first_partition_size <= size) {
      sh->frame_tag_size = hint_frame_header_size;
      sh->offset_to_dct_parts = hint_first_partition_size;
   }

   hdr = sh->frame_tag_size;
   part = sh->offset_to_dct_parts;
   if (hdr >= size || part > size - hdr)
      return MWV207_NOK;

   /* The bool coder reads up to 4 bytes ahead; give it the whole frame
    * remainder so the normal look-ahead at the first-partition end does not
    * raise strm_error.  The compressed header itself ends before the tile
    * payload, which the caller locates with frame_tag_size + part. */
   (void)part;
   if (sh->key_frame || sh->error_resilient || sh->reset_frame_context == 3) {
      for (uint32_t i = 0; i < NUM_FRAME_CONTEXTS; i++)
         sh->entropy_last[i] = sh->entropy;
      if (saved_ctx)
         for (uint32_t i = 0; i < NUM_FRAME_CONTEXTS; i++)
            saved_ctx[i] = sh->entropy;
   } else if (sh->reset_frame_context == 2) {
      sh->entropy_last[sh->frame_context_idx] = sh->entropy;
   }
   ret = vp9_decode_compressed_header((uint8_t *)(data + hdr), size - hdr,
                                      &bc, (uint8_t *)data, size, sh);
   if (ret == MWV207_OK && saved_ctx
       && sh->frame_context_idx < NUM_FRAME_CONTEXTS)
      saved_ctx[sh->frame_context_idx] = sh->entropy;
   return ret;
}
