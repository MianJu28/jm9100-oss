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

#ifndef COMMONSTRUCT_H
#define COMMONSTRUCT_H

#include <stdint.h>

#define VP9_REF_SCALE_SHIFT 14

#define NUM_REF_FRAMES 8
#define NUM_REF_FRAMES_LG2 3

#define ALLOWED_REFS_PER_FRAME 3

#define NUM_FRAME_CONTEXTS_LG2 2
#define NUM_FRAME_CONTEXTS (1 << NUM_FRAME_CONTEXTS_LG2)

#define DCPREDSIMTHRESH 0
#define DCPREDCNTTHRESH 3

#define PREDICTION_PROBS 3

#define DEFAULT_PRED_PROB_0 120
#define DEFAULT_PRED_PROB_1 80
#define DEFAULT_PRED_PROB_2 40

#define VP9_DEF_UPDATE_PROB 252

#define MBSKIP_CONTEXTS 3

#define MAX_MB_SEGMENTS 8
#define MB_SEG_TREE_PROBS (MAX_MB_SEGMENTS - 1)

#define MAX_REF_LF_DELTAS 4
#define MAX_MODE_LF_DELTAS 2

#define SEGMENT_DELTADATA 0
#define SEGMENT_ABSDATA 1
#define MAX_MV_REFS 9

#define VP9_SWITCHABLE_FILTERS 3

#define COMP_PRED_CONTEXTS 2

#define COEF_UPDATE_PROB 252
#define VP9_PROB_HALF 128
#define VP9_NMV_UPDATE_PROB 252
#define VP9_MV_UPDATE_PRECISION 7
#define MV_JOINTS 4
#define MV_CLASSES 11
#define CLASS0_BITS 1
#define CLASS0_SIZE (1 << CLASS0_BITS)
#define MV_OFFSET_BITS (MV_CLASSES + CLASS0_BITS - 2)

#define MV_MAX_BITS (MV_CLASSES + CLASS0_BITS + 2)
#define MV_MAX ((1 << MV_MAX_BITS) - 1)
#define MV_VALS ((MV_MAX << 1) + 1)

#define MAX_ENTROPY_TOKENS 12
#define ENTROPY_NODES 11

/* The first nodes of the entropy probs are unconstrained, the rest are
 * modeled with statistic distribution. */
#define UNCONSTRAINED_NODES 3
#define MODEL_NODES (ENTROPY_NODES - UNCONSTRAINED_NODES)
#define PIVOT_NODE 2 
#define COEFPROB_MODELS 128

/* Entropy nodes above is divided in two parts, first three probs in part1
 * and the modeled probs in part2. Part1 is padded so that tables align with
 *  32 byte addresses, so there is four bytes for each table. */
#define ENTROPY_NODES_PART1 4
#define ENTROPY_NODES_PART2 8
#define INTER_MODE_CONTEXTS_DEC 7

#define INTRA_INTER_CONTEXTS 4
#define COMP_INTER_CONTEXTS 5
#define REF_CONTEXTS_DEC 5

#define BLOCK_TYPES 2
#define REF_TYPES 2 
#define COEF_BANDS 6
#define PREV_COEF_CONTEXTS 6

#define MODULUS_PARAM 13

#define ACTIVE_HT 110 

#define MAX_MV_REF_CANDIDATES 2

#define ZERO_TOKEN 0
#define ONE_TOKEN 1
#define TWO_TOKEN 2
#define THREE_TOKEN 3
#define FOUR_TOKEN 4
#define DCT_VAL_CATEGORY1 5
#define DCT_VAL_CATEGORY2 6
#define DCT_VAL_CATEGORY3 7
#define DCT_VAL_CATEGORY4 8
#define DCT_VAL_CATEGORY5 9
#define DCT_VAL_CATEGORY6 10
#define DCT_EOB_TOKEN 11
#define MAX_ENTROPY_TOKENS 12

#define DCT_EOB_MODEL_TOKEN 3

typedef uint32_t vp9_coeff_count[REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                           [UNCONSTRAINED_NODES + 1];
typedef uint8_t vp9_coeff_probs[REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                          [UNCONSTRAINED_NODES];

#define BLOCK_SIZE_GROUPS 4

enum mwv207_block_size_type {
   BLOCK_SIZE_AB4X4,
   BLOCK_SIZE_SB4X8,
   BLOCK_SIZE_SB8X4,
   BLOCK_SIZE_SB8X8,
   BLOCK_SIZE_SB8X16,
   BLOCK_SIZE_SB16X8,
   BLOCK_SIZE_MB16X16,
   BLOCK_SIZE_SB16X32,
   BLOCK_SIZE_SB32X16,
   BLOCK_SIZE_SB32X32,
   BLOCK_SIZE_SB32X64,
   BLOCK_SIZE_SB64X32,
   BLOCK_SIZE_SB64X64,
   BLOCK_SIZE_TYPES
};

enum mwv207_partition_type {
   PARTITION_NONE_DEC,
   PARTITION_HORZ_DEC,
   PARTITION_VERT_DEC,
   PARTITION_SPLIT_DEC,
   PARTITION_TYPES_DEC
};

#define PARTITION_PLOFFSET 4 
#define NUM_PARTITION_CONTEXTS (4 * PARTITION_PLOFFSET)

enum mwv207_vp9_frame_type {
   KEY_FRAME = 0,
   INTER_FRAME = 1,
   NUM_FRAME_TYPES,
};

enum mwv207_mb_prediction_mode {
   DC_PRED_DEC,
   V_PRED_DEC,
   H_PRED_DEC,
   D45_PRED_DEC, /* Directional 45 deg prediction  [anti-clockwise from 0 deg
                                                 hor] */
   D135_PRED_DEC, /* Directional 135 deg prediction [anti-clockwise from 0 deg
                   * hor]
                   */
   D117_PRED_DEC, /* Directional 112 deg prediction [anti-clockwise from 0 deg
                   * hor]
                   */
   D153_PRED_DEC, /* Directional 157 deg prediction [anti-clockwise from 0 deg
                   * hor]
                   */
   D27_PRED_DEC, /* Directional 22 deg prediction  [anti-clockwise from 0 deg
                                                 hor] */
   D63_PRED_DEC, /* Directional 67 deg prediction  [anti-clockwise from 0 deg
                                                 hor] */
   TM_PRED_DEC,
   NEARESTMV_DEC,
   NEARMV_DEC,
   ZEROMV_DEC,
   NEWMV_DEC,
   SPLITMV_DEC,
   MB_MODE_COUNT_DEC
};
#define VP9_INTRA_MODES (TM_PRED_DEC + 1)

#define VP9_INTER_MODES (1 + NEWMV_DEC - NEARESTMV_DEC)

enum mwv207_block_prediction_mode {
   B_DC_PRED,
   B_V_PRED,
   B_H_PRED,
   B_D45_PRED,
   B_D135_PRED,
   B_D117_PRED,
   B_D153_PRED,
   B_D27_PRED,
   B_D63_PRED,
   B_TM_PRED,
   LEFT4X4,
   ABOVE4X4,
   ZERO4X4,
   NEW4X4,
   B_MODE_COUNT
};

#define VP9_BINTRAMODES (LEFT4X4)
#define VP9_SUBMVREFS (1 + NEW4X4 - LEFT4X4)

#define SUBMVREF_COUNT 5

#define SUBPEL_SHIFTS 16

/* Integer pel reference mv threshold for use of high-precision 1/8 mv */
#define COMPANDED_MVREF_THRESH 8

#define TX_SIZE_CONTEXTS_DEC 2

enum mwv207_interpolation_filter_type {
   EIGHTTAP_SMOOTH,
   EIGHTTAP,
   EIGHTTAP_SHARP,
   BILINEAR,
   SWITCHABLE
};

extern const enum mwv207_interpolation_filter_type
   vp9_switchable_interp[VP9_SWITCHABLE_FILTERS];

enum mwv207_comp_pred_mode_type {
   SINGLE_PREDICTION_ONLY = 0,
   COMP_PREDICTION_ONLY = 1,
   HYBRID_PREDICTION = 2,
   NB_PREDICTION_TYPES = 3,
};

enum mwv207_txfm_mode {
   ONLY_4X4_DEC = 0,
   ALLOW_8X8_DEC = 1,
   ALLOW_16X16_DEC = 2,
   ALLOW_32X32_DEC = 3,
   TX_MODE_SELECT_DEC = 4,
   NB_TXFM_MODES_DEC = 5,
};

enum mwv207_mv_reference_frame {
   NONE = -1,
   INTRA_FRAME_DEC = 0,
   LAST_FRAME_DEC = 1,
   GOLDEN_FRAME_DEC = 2,
   ALTREF_FRAME_DEC = 3,
   MAX_REF_FRAMES_DEC = 4
};

enum mwv207_seg_level_features {
   SEG_LVL_ALT_Q = 0,
   SEG_LVL_ALT_LF = 1,
   SEG_LVL_REF_FRAME = 2,
   SEG_LVL_SKIP = 3,
   SEG_LVL_MAX = 4
};

enum { VP9_SEG_FEATURE_DELTA, VP9_SEG_FEATURE_ABS };

static const int vp9_seg_feature_data_signed[SEG_LVL_MAX] = {1, 1, 0, 0};
static const int vp9_seg_feature_data_max[SEG_LVL_MAX] = {255, 63, 3, 0};

enum mwv207_tx_size {
   TX_4X4_DEC = 0,
   TX_8X8_DEC = 1,
   TX_16X16_DEC = 2,
   TX_32X32_DEC = 3,
   TX_SIZE_MAX_SB_DEC,
};

enum mwv207_split_mv_partitioning_type {
   PARTITIONING_16X8 = 0,
   PARTITIONING_8X16,
   PARTITIONING_8X8,
   PARTITIONING_4X4,
   NB_PARTITIONINGS,
};

enum mwv207_pred_id {
   PRED_SEG_ID = 0,
   PRED_MBSKIP = 1,
   PRED_SWITCHABLE_INTERP = 2,
   PRED_INTRA_INTER = 3,
   PRED_COMP_INTER_INTER = 4,
   PRED_SINGLE_REF_P1 = 5,
   PRED_SINGLE_REF_P2 = 6,
   PRED_COMP_REF_P = 7,
   PRED_TX_SIZE = 8
};

enum mwv207_mv_joint_type {
   MV_JOINT_ZERO = 0,
   MV_JOINT_HNZVZ = 1,
   MV_JOINT_HZVNZ = 2,
   MV_JOINT_HNZVNZ = 3,
};

enum mwv207_mv_class_type {
   MV_CLASS_0 = 0,
   MV_CLASS_1 = 1,
   MV_CLASS_2 = 2,
   MV_CLASS_3 = 3,
   MV_CLASS_4 = 4,
   MV_CLASS_5 = 5,
   MV_CLASS_6 = 6,
   MV_CLASS_7 = 7,
   MV_CLASS_8 = 8,
   MV_CLASS_9 = 9,
   MV_CLASS_10 = 10,
};

struct mwv207_nmv_context {

   uint8_t joints[MV_JOINTS - 1];
   uint8_t sign[2];

   uint8_t class0[2][CLASS0_SIZE - 1];
   uint8_t fp[2][4 - 1];
   uint8_t class0_hp[2];
   uint8_t hp[2];
   uint8_t classes[2][MV_CLASSES - 1];

   uint8_t class0_fp[2][CLASS0_SIZE][4 - 1];
   uint8_t bits[2][MV_OFFSET_BITS];
};

struct mwv207_mmv_context_counts {
   uint32_t joints[MV_JOINTS];
   uint32_t sign[2][2];
   uint32_t classes[2][MV_CLASSES];
   uint32_t class0[2][CLASS0_SIZE];
   uint32_t bits[2][MV_OFFSET_BITS][2];
   uint32_t class0_fp[2][CLASS0_SIZE][4];
   uint32_t fp[2][4];
   uint32_t class0_hp[2][2];
   uint32_t hp[2][2];
};

typedef uint8_t vp9_prob;

#define VP9HWPAD(x, y) uint8_t x[y]

/* Adaptive entropy contexts, padding elements are added to have
 * 256 bit aligned tables for HW access.
 * Compile with TRACE_PROB_TABLES to print bases for each table. */
struct mwv207_vp9_adaptive_entropy_probs {

   uint8_t inter_mode_prob[INTER_MODE_CONTEXTS_DEC][4];
   uint8_t intra_inter_prob[INTRA_INTER_CONTEXTS];

   uint8_t uv_mode_prob[VP9_INTRA_MODES][8];
   uint8_t tx8x8_prob[TX_SIZE_CONTEXTS_DEC][TX_SIZE_MAX_SB_DEC - 3];
   uint8_t tx16x16_prob[TX_SIZE_CONTEXTS_DEC][TX_SIZE_MAX_SB_DEC - 2];
   uint8_t tx32x32_prob[TX_SIZE_CONTEXTS_DEC][TX_SIZE_MAX_SB_DEC - 1];
   uint8_t sb_ymode_prob_b[BLOCK_SIZE_GROUPS][1];
   uint8_t sb_ymode_prob[BLOCK_SIZE_GROUPS][8];

   uint8_t partition_prob[NUM_FRAME_TYPES][NUM_PARTITION_CONTEXTS]
                    [PARTITION_TYPES_DEC];

   uint8_t uv_mode_prob_b[VP9_INTRA_MODES][1];
   uint8_t switchable_interp_prob[VP9_SWITCHABLE_FILTERS + 1]
                            [VP9_SWITCHABLE_FILTERS - 1];
   uint8_t comp_inter_prob[COMP_INTER_CONTEXTS];
   uint8_t mbskip_probs[MBSKIP_CONTEXTS];
   VP9HWPAD(pad1, 1);

   struct mwv207_nmv_context nmvc;

   uint8_t single_ref_prob[REF_CONTEXTS_DEC][2];
   uint8_t comp_ref_prob[REF_CONTEXTS_DEC];
   VP9HWPAD(pad2, 17);

   uint8_t prob_coeffs[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                 [ENTROPY_NODES_PART1];
   uint8_t prob_coeffs8x8[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                    [ENTROPY_NODES_PART1];
   uint8_t prob_coeffs16x16[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                      [ENTROPY_NODES_PART1];
   uint8_t prob_coeffs32x32[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                      [ENTROPY_NODES_PART1];
};

struct mwv207_vp9_entropy_probs {

   /* Table formatted for 256b memory, probs 0to7 for all tables followed by
    * probs 8toN for all tables.
    * Compile with TRACE_PROB_TABLES to print bases for each table. */

   uint8_t kf_bmode_prob[VP9_INTRA_MODES][VP9_INTRA_MODES][8];

   uint8_t kf_bmode_prob_b[VP9_INTRA_MODES][VP9_INTRA_MODES][1];
   uint8_t ref_pred_probs[PREDICTION_PROBS];
   uint8_t mb_segment_tree_probs[MB_SEG_TREE_PROBS];
   uint8_t segment_pred_probs[PREDICTION_PROBS];
   uint8_t ref_scores[MAX_REF_FRAMES_DEC];
   uint8_t prob_comppred[COMP_PRED_CONTEXTS];
   VP9HWPAD(pad1, 9);

   uint8_t kf_uv_mode_prob[VP9_INTRA_MODES][8];
   uint8_t kf_uv_mode_prob_b[VP9_INTRA_MODES][1];
   VP9HWPAD(pad2, 6);

   struct mwv207_vp9_adaptive_entropy_probs
      a;
};

struct mwv207_vp9_entropy_counts {
   uint32_t inter_mode_counts[INTER_MODE_CONTEXTS_DEC][VP9_INTER_MODES - 1][2];
   uint32_t sb_ymode_counts[BLOCK_SIZE_GROUPS][VP9_INTRA_MODES];
   uint32_t uv_mode_counts[VP9_INTRA_MODES][VP9_INTRA_MODES];
   uint32_t partition_counts[NUM_PARTITION_CONTEXTS][PARTITION_TYPES_DEC];
   uint32_t switchable_interp_counts[VP9_SWITCHABLE_FILTERS + 1]
                               [VP9_SWITCHABLE_FILTERS];
   uint32_t intra_inter_count[INTRA_INTER_CONTEXTS][2];
   uint32_t comp_inter_count[COMP_INTER_CONTEXTS][2];
   uint32_t single_ref_count[REF_CONTEXTS_DEC][2][2];
   uint32_t comp_ref_count[REF_CONTEXTS_DEC][2];
   uint32_t tx32x32_count[TX_SIZE_CONTEXTS_DEC][TX_SIZE_MAX_SB_DEC];
   uint32_t tx16x16_count[TX_SIZE_CONTEXTS_DEC][TX_SIZE_MAX_SB_DEC - 1];
   uint32_t tx8x8_count[TX_SIZE_CONTEXTS_DEC][TX_SIZE_MAX_SB_DEC - 2];
   uint32_t mbskip_count[MBSKIP_CONTEXTS][2];

   struct mwv207_mmv_context_counts nmvcount;

   uint32_t count_coeffs[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                   [UNCONSTRAINED_NODES + 1];
   uint32_t count_coeffs8x8[BLOCK_TYPES][REF_TYPES][COEF_BANDS][PREV_COEF_CONTEXTS]
                      [UNCONSTRAINED_NODES + 1];
   uint32_t count_coeffs16x16[BLOCK_TYPES][REF_TYPES][COEF_BANDS]
                        [PREV_COEF_CONTEXTS][UNCONSTRAINED_NODES + 1];
   uint32_t count_coeffs32x32[BLOCK_TYPES][REF_TYPES][COEF_BANDS]
                        [PREV_COEF_CONTEXTS][UNCONSTRAINED_NODES + 1];

   uint32_t count_eobs[TX_SIZE_MAX_SB_DEC][BLOCK_TYPES][REF_TYPES][COEF_BANDS]
                 [PREV_COEF_CONTEXTS];
};

#endif

