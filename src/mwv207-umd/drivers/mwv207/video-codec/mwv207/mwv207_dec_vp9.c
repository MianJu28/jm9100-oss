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

/*
 * VP9 decoder register programming.
 *
 * The frame headers (uncompressed + compressed incl. probability updates and
 * tile sizes) are parsed in software; the ASIC gets the tile payload window
 * and the entropy probability tables from a linear scratch BO.
 *
 * Current stage: intra/key frames, single tile, no segmentation, 8-bit NV12.
 * Reference frame programming and 10-bit handling follow (UNRESOLVED D1).
 */

#include "mwv207_dec_vp9.h"
#include "mwv207_dec_ppu.h"
#include "mwv207_video_dec.h"
#include "mwv207_video_dec_cfg.h"
#include "mwv207_decoder_vp9_slice_header.h"
#include "vl/vl_video_buffer.h"
#include "mwv207_utils.h"
#include "mwv207_context.h"

/* VP9 allows up to 64 tile columns (log2 6) and 4 tile rows (log2 2). */
#define VP9_MAX_TILE_COLS 64u
#define VP9_MAX_TILE_ROWS 4u
#define VP9_MAX_TILES (VP9_MAX_TILE_COLS * VP9_MAX_TILE_ROWS)



#define VP9_PROB_TABLE_SIZE 0xea0u /* sizeof(struct mwv207_vp9_entropy_probs) */
#define VP9_PROB_OFFSET 0x0u
#define VP9_COUNTER_OFFSET 0xea0u
/* The vendor layout reserves 13264 bytes for the symbol counters, but the
 * entropy engine overruns that on coefficient-dense frames and tramples the
 * tile-size table right behind it (fixed transform-size streams then desync
 * halfway through the frame).  Give it a wide berth instead. */
#define VP9_COUNTER_SIZE 0x20000u
#define VP9_STREAM_COPY_SIZE (4u << 20)
/* Slot 0 holds the MV image of the frame being decoded, 1..3 the copies the
 * LAST/GOLDEN/ALTREF references read as their temporal MV candidate. */
#define VP9_MV_SLOTS 3u

#define VP9_FILTER_SIZE (2u << 20)

static inline uint32_t
vp9_align4k(uint32_t v)
{
   return (v + 0xfffu) & ~0xfffu;
}

struct mwv207_vp9_state {
   struct mwv207_vp9_slice_header sh;
   /* VP9 carries four coefficient-probability contexts across frames. */
   struct mwv207_vp9_entropy_probs saved_ctx[NUM_FRAME_CONTEXTS];
   /* Copy of saved_ctx[] taken right before the header parser applies the
    * frame's probability updates; the adaptation blends the updated table
    * with this previous one. */
   struct mwv207_vp9_entropy_probs pre_ctx[NUM_FRAME_CONTEXTS];
   /* saved_ctx[] holds the VP9 default tables from the first frame on, the
    * way libvpx initialises its frame contexts at decoder start. */
   bool ctx_ready;
   bool prev_frame_key;
   struct mwv207_video_buffer *scratch;
   void *scratch_cpu;
   uint32_t scratch_size;
   uint32_t seg_offset;
   uint32_t seg_size;
   uint32_t tile_offset;
   uint32_t filter_offset;
   uint32_t stream_offset;
   /* Bounce buffer for the hardware's per-block motion vector store. */
   uint32_t mv_offset;
   uint32_t mv_size;
   uint32_t mv_bytes;
   bool prev_was_inter;
   bool prev_was_shown;
   bool saw_error_resilient;
   /* The loop filter reference/mode deltas are updated by the header only
    * when the stream says so; otherwise they carry over from the previous
    * frame (libvpx keeps them in cm->lf).  The defaults are applied by the
    * header parser on every intra/key frame. */
   int32_t lf_ref_delta[MAX_REF_LF_DELTAS];
   int32_t lf_mode_delta[MAX_MODE_LF_DELTAS];
   /* Per-segment feature values persist across frames unless the header
    * carries fresh segment data (segment_data_update == 1). */
   uint32_t seg_feature_enable[MAX_MB_SEGMENTS][SEG_LVL_MAX];
   int32_t seg_feature_data[MAX_MB_SEGMENTS][SEG_LVL_MAX];
   uint32_t seg_feature_mode;
   bool seg_feature_valid;
   /* The hardware decodes the per-block segment ids into a map buffer and
    * reads the previous map back for temporal prediction, so the two maps
    * alternate: a frame that updates the map writes the inactive one, which
    * becomes the read map of the next frame. */
   uint32_t seg_active;
   /* VP9 reference frame slots: these point at the pipe_video_buffer of the
    * frame that last refreshed this slot.  The VA client keeps decoded
    * frames alive while they are referenced, so no extra reference is
    * taken here. */
   struct mwv207_video_buffer *ref_slots[16];
   struct mwv207_video_buffer *ref_store;
   struct mwv207_video_buffer *pending;
   uint32_t pending_flags, pending_luma;
   uint32_t ref_buf_size, ref_luma, ref_frame_bytes;
   uint32_t width;
   uint32_t height;
   /* Tile payload of the current frame: the hardware walks the tiles by
    * itself once every one of them carries a 4-byte size prefix. */
   uint32_t tile_n;
   /* VP9 allows up to 64 tile columns (log2 6) and 4 tile rows (log2 2). */
   uint32_t tile_off[VP9_MAX_TILES];
   uint32_t tile_len[VP9_MAX_TILES];
};

void
mwv207_dec_vp9_destroy_state(void *state);

/* Reference banks.  VP9 uses three reference pictures per frame: LAST,
 * GOLDEN and ALTREF.  The hardware exposes eight independent REFER banks;
 * the vendor driver maps LAST->REFER0, GOLDEN->REFER4, ALTREF->REFER5. */
static const uint32_t vp9_ref_ybase_lsb[3] = {
   JMDEC_REFER0_YBASE_LSB, JMDEC_REFER4_YBASE_LSB, JMDEC_REFER5_YBASE_LSB,
};
static const uint32_t vp9_ref_ybase_msb[3] = {
   JMDEC_REFER0_YBASE_MSB, JMDEC_REFER4_YBASE_MSB, JMDEC_REFER5_YBASE_MSB,
};
static const uint32_t vp9_ref_cbase_lsb[3] = {
   JMDEC_REFER0_CBASE_LSB, JMDEC_REFER4_CBASE_LSB, JMDEC_REFER5_CBASE_LSB,
};
static const uint32_t vp9_ref_cbase_msb[3] = {
   JMDEC_REFER0_CBASE_MSB, JMDEC_REFER4_CBASE_MSB, JMDEC_REFER5_CBASE_MSB,
};
static const uint32_t vp9_ref_w_reg[3] = {
   JMDEC_LREF_WIDTH, JMDEC_GREF_WIDTH, JMDEC_AREF_WIDTH,
};
static const uint32_t vp9_ref_h_reg[3] = {
   JMDEC_LREF_HEIGHT, JMDEC_GREF_HEIGHT, JMDEC_AREF_HEIGHT,
};
static const uint32_t vp9_ref_hscale_reg[3] = {
   JMDEC_LREF_HOR_SCALE, JMDEC_GREF_HOR_SCALE, JMDEC_AREF_HOR_SCALE,
};
static const uint32_t vp9_ref_vscale_reg[3] = {
   JMDEC_LREF_VER_SCALE, JMDEC_GREF_VER_SCALE, JMDEC_AREF_VER_SCALE,
};
static const uint32_t vp9_ref_sign_bias_reg[3] = {
   JMDEC_LAST_SIGN_BIAS, JMDEC_GREF_SIGN_BIAS, JMDEC_AREF_SIGN_BIAS,
};
static const uint32_t vp9_ref_y_stride_reg[3] = {
   JMDEC_LREF_Y_STRIDE, JMDEC_GREF_Y_STRIDE, JMDEC_AREF_Y_STRIDE,
};
static const uint32_t vp9_ref_c_stride_reg[3] = {
   JMDEC_LREF_C_STRIDE, JMDEC_GREF_C_STRIDE, JMDEC_AREF_C_STRIDE,
};
static const uint32_t vp9_ref_tybase_lsb[3] = {
   JMDEC_REFER0_TYBASE_LSB, JMDEC_REFER4_TYBASE_LSB, JMDEC_REFER5_TYBASE_LSB,
};
static const uint32_t vp9_ref_tybase_msb[3] = {
   JMDEC_REFER0_TYBASE_MSB, JMDEC_REFER4_TYBASE_MSB, JMDEC_REFER5_TYBASE_MSB,
};
static const uint32_t vp9_ref_tcbase_lsb[3] = {
   JMDEC_REFER0_TCBASE_LSB, JMDEC_REFER4_TCBASE_LSB, JMDEC_REFER5_TCBASE_LSB,
};
static const uint32_t vp9_ref_dbase_lsb[3] = {
   JMDEC_REFER0_DBASE_LSB, JMDEC_REFER4_DBASE_LSB, JMDEC_REFER5_DBASE_LSB,
};
static const uint32_t vp9_ref_dbase_msb[3] = {
   JMDEC_REFER0_DBASE_MSB, JMDEC_REFER4_DBASE_MSB, JMDEC_REFER5_DBASE_MSB,
};
static const uint32_t vp9_ref_tcbase_msb[3] = {
   JMDEC_REFER0_TCBASE_MSB, JMDEC_REFER4_TCBASE_MSB, JMDEC_REFER5_TCBASE_MSB,
};

/* Per-segment feature registers.
 *
 * The register file carries *final* per-segment values: the vendor UMD
 * resolves segmentation_abs_or_delta_update in software (adding the frame
 * base quantiser / loop filter level in the delta case, clamping, and
 * encoding the reference frame as "value + 1" so that 0 means "no per
 * segment reference"), and the hardware just applies them per block.
 * Features a segment does not enable fall back to the frame defaults.
 */
static void
vp9_write_segment_regs(struct mwv207_video_reg_buf *reg_buf,
                       struct mwv207_vp9_slice_header *sh)
{
   static const enum mwv207_9200_hw_if_name quant_reg[MAX_MB_SEGMENTS] = {
      JMDEC_QUANT_SEG0, JMDEC_QUANT_SEG1, JMDEC_QUANT_SEG2, JMDEC_QUANT_SEG3,
      JMDEC_QUANT_SEG4, JMDEC_QUANT_SEG5, JMDEC_QUANT_SEG6, JMDEC_QUANT_SEG7,
   };
   static const enum mwv207_9200_hw_if_name filt_reg[MAX_MB_SEGMENTS] = {
      JMDEC_FILT_LEVEL_SEG0, JMDEC_FILT_LEVEL_SEG1, JMDEC_FILT_LEVEL_SEG2,
      JMDEC_FILT_LEVEL_SEG3, JMDEC_FILT_LEVEL_SEG4, JMDEC_FILT_LEVEL_SEG5,
      JMDEC_FILT_LEVEL_SEG6, JMDEC_FILT_LEVEL_SEG7,
   };
   static const enum mwv207_9200_hw_if_name refpic_reg[MAX_MB_SEGMENTS] = {
      JMDEC_REFPIC_SEG0, JMDEC_REFPIC_SEG1, JMDEC_REFPIC_SEG2,
      JMDEC_REFPIC_SEG3, JMDEC_REFPIC_SEG4, JMDEC_REFPIC_SEG5,
      JMDEC_REFPIC_SEG6, JMDEC_REFPIC_SEG7,
   };
   static const enum mwv207_9200_hw_if_name skip_reg[MAX_MB_SEGMENTS] = {
      JMDEC_SKIP_SEG0, JMDEC_SKIP_SEG1, JMDEC_SKIP_SEG2, JMDEC_SKIP_SEG3,
      JMDEC_SKIP_SEG4, JMDEC_SKIP_SEG5, JMDEC_SKIP_SEG6, JMDEC_SKIP_SEG7,
   };
   int32_t base_q = sh->lossless ? 0 : (int32_t)sh->qp_yac;
   int32_t base_lf = (int32_t)(sh->loop_filter_level & 0x3f);
   bool abs_mode = sh->segment_feature_mode != 0;
   bool intra = sh->key_frame || sh->intra_only;
   uint32_t i;

   /* Even with segmentation disabled the ASIC takes the frame quantiser and
    * loop filter level from segment 0's feature words, so always write the
    * per-segment table (segments 1..7 then simply carry the defaults). */
   for (i = 0; i < MAX_MB_SEGMENTS; i++) {
      int32_t q = base_q, lf = base_lf, ref = 0, skip = 0;

      if (sh->segment_feature_enable[i][SEG_LVL_ALT_Q]) {
         int32_t d = sh->segment_feature_data[i][SEG_LVL_ALT_Q];
         q = abs_mode ? d : base_q + d;
         q = q < 0 ? 0 : (q > 255 ? 255 : q);
      }
      if (sh->segment_feature_enable[i][SEG_LVL_ALT_LF]) {
         int32_t d = sh->segment_feature_data[i][SEG_LVL_ALT_LF];
         lf = abs_mode ? d : base_lf + d;
         lf = lf < 0 ? 0 : (lf > 63 ? 63 : lf);
      }
      if (!intra && sh->segment_feature_enable[i][SEG_LVL_REF_FRAME])
         ref = sh->segment_feature_data[i][SEG_LVL_REF_FRAME] + 1;
      if (sh->segment_feature_enable[i][SEG_LVL_SKIP])
         skip = 1;

      reg_buf->set_reg(reg_buf, quant_reg[i], (uint32_t)q);
      reg_buf->set_reg(reg_buf, filt_reg[i], (uint32_t)lf);
      reg_buf->set_reg(reg_buf, refpic_reg[i], (uint32_t)ref);
      reg_buf->set_reg(reg_buf, skip_reg[i], (uint32_t)skip);
   }
}

static void
vp9_write_ref_regs(struct mwv207_vp9_state *st,
                   struct mwv207_video_reg_buf *reg_buf,
                   struct mwv207_vp9_slice_header *sh, uint32_t luma_size,
                   uint32_t frame_bytes, uint32_t y_stride)
{
   uint32_t k;

   if (sh->key_frame || sh->intra_only)
      return;
   if (!st->ref_store || !st->ref_store->res)
      return;
   for (k = 0; k < 3; k++) {
      uint32_t slot = sh->active_ref_idx[k];
      struct mwv207_bo *bo;
      uint64_t off = (uint64_t)slot * frame_bytes;

      if (slot >= 16)
         continue;
      bo = st->ref_store->res->bo;
      if (!bo)
         continue;

      reg_buf->set_addr_reg_hi(reg_buf, vp9_ref_ybase_msb[k], bo, off,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf, vp9_ref_ybase_lsb[k], bo, off,
                            MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg_hi(reg_buf, vp9_ref_cbase_msb[k], bo,
                               off + luma_size, MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf, vp9_ref_cbase_lsb[k], bo,
                            off + luma_size, MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg_hi(reg_buf, vp9_ref_tybase_msb[k], bo, off,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf, vp9_ref_tybase_lsb[k], bo, off,
                            MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg_hi(reg_buf, vp9_ref_tcbase_msb[k], bo,
                               off + luma_size, MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf, vp9_ref_tcbase_lsb[k], bo,
                            off + luma_size, MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_reg(reg_buf, vp9_ref_y_stride_reg[k], y_stride);
      reg_buf->set_reg(reg_buf, vp9_ref_c_stride_reg[k], y_stride);
      reg_buf->set_reg(reg_buf, vp9_ref_w_reg[k], sh->width);
      reg_buf->set_reg(reg_buf, vp9_ref_h_reg[k], sh->height);
      /* 1.0 in the hardware's 14-bit fixed point (vendor: (w << 14) / w). */
      reg_buf->set_reg(reg_buf, vp9_ref_hscale_reg[k], 1u << 14);
      reg_buf->set_reg(reg_buf, vp9_ref_vscale_reg[k], 1u << 14);
      /* ref_frame_sign_bias[] is indexed by REF_FRAME (INTRA, LAST, GOLDEN,
       * ALTREF); our k is 0..2 for LAST/GOLDEN/ALTREF. */
      reg_buf->set_reg(reg_buf, vp9_ref_sign_bias_reg[k],
                       sh->ref_frame_sign_bias[k + 1] ? 1 : 0);
      if (st->scratch && st->scratch->res) {
         /* VP9's temporal MV candidate is the previous decoded frame's MV
          * image (vp9_mvref_common.c: cm->prev_frame->mvs).  The ASIC reads
          * it through the reference's DBASE, so hand every reference used by
          * this frame a copy of slot 0, which still holds the previous
          * frame's MV image (the current one is written after submit). */
         uint32_t prev = st->mv_offset + (1u + k) * st->mv_size;
         if (st->scratch_cpu && st->mv_bytes)
            memcpy((uint8_t *)st->scratch_cpu + prev,
                   (uint8_t *)st->scratch_cpu + st->mv_offset, st->mv_bytes);
         reg_buf->set_addr_reg_hi(reg_buf, vp9_ref_dbase_msb[k],
                                  st->scratch->res->bo, prev,
                                  MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg(reg_buf, vp9_ref_dbase_lsb[k],
                               st->scratch->res->bo, prev,
                               MWV207_BO_ACC_FLAG_WRITE);
      }
   }
}


static void
vp9_refresh_slots(struct mwv207_video_decoder *decoder,
                  struct mwv207_vp9_state *st, uint32_t refresh_flags,
                  struct mwv207_video_buffer *buf, uint32_t frame_bytes,
                  uint32_t luma_size)
{
   uint32_t i, need = frame_bytes;

   if (!buf || !buf->res || !buf->res->bo)
      return;
   if (st->ref_buf_size < need * 16) {
      if (st->ref_store)
         mwv207_video_buffer_destroy((struct pipe_video_buffer *)st->ref_store);
      st->ref_store = mwv207_video_buffer_create_2(
         decoder->base.context, need * 16, PIPE_USAGE_STAGING);
      st->ref_buf_size = st->ref_store ? need * 16 : 0;
      st->ref_luma = luma_size;
      st->ref_frame_bytes = frame_bytes;
   }
   if (!st->ref_store || !st->ref_store->res)
      return;
   for (i = 0; i < 16; i++) {
      uint8_t *src, *dst;
      if (!(refresh_flags & (1u << i)))
         continue;
      src = decoder->ws->bo_map(decoder->ws, buf->res->bo);
      dst = decoder->ws->bo_map(decoder->ws, st->ref_store->res->bo);
      if (src && dst)
         memcpy(dst + (size_t)i * need, src, need);
   }
}


static void
vp9_destroy_state(struct mwv207_vp9_state *st)
{
   if (!st)
      return;
   if (st->scratch)
      mwv207_video_buffer_destroy((struct pipe_video_buffer *)st->scratch);
   FREE(st);
}

static struct mwv207_vp9_state *
vp9_get_state(struct mwv207_video_decoder *decoder, uint32_t width,
              uint32_t height)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_vp9_state *st = dec->vp9_state;
   uint32_t supercols = (((width + 7) & ~7u) + 63) >> 6;
   uint32_t superrows = (((height + 7) & ~7u) + 63) >> 6;
   uint32_t tile_size = vp9_align4k(supercols * superrows * 4u + 64u);
   uint32_t seg_size = vp9_align4k(width * height);
   uint32_t ctr_size = VP9_COUNTER_SIZE;
   uint32_t need = VP9_COUNTER_OFFSET + ctr_size;

   need = vp9_align4k(need + tile_size);
   need = vp9_align4k(need + seg_size * 2);
   need += VP9_FILTER_SIZE + VP9_STREAM_COPY_SIZE;

   if (st && (width > st->width || height > st->height)) {
      vp9_destroy_state(st);
      dec->vp9_state = NULL;
      st = NULL;
   }

   if (!st) {
      st = CALLOC_STRUCT(mwv207_vp9_state);
      if (!st)
         return NULL;
      /* Vendor layout: prob table at 0, counters right after it, the tile
       * size table immediately behind the counters. */
      st->tile_offset = VP9_COUNTER_OFFSET + ctr_size;
      st->seg_offset = vp9_align4k(st->tile_offset + tile_size);
      st->seg_size = seg_size;
      /* Two segment map buffers: the hardware reads the map built by the
       * previous frame (temporal update) and writes the current one. */
      st->filter_offset = vp9_align4k(st->seg_offset + st->seg_size * 2u);
      st->width = width;
      st->height = height;
      st->stream_offset = vp9_align4k(st->filter_offset + VP9_FILTER_SIZE);
      st->mv_offset = vp9_align4k(st->stream_offset + VP9_STREAM_COPY_SIZE);
      /* The vendor places the motion vector store right behind the decoded
       * picture, luma * 3 / 2 bytes later.  We only need somewhere valid for
       * the hardware to write to, so give it a buffer of that size. */
      st->mv_size = vp9_align4k(width * height * 3 / 2 + 64);
      /* The hardware stores one 16 byte record per 8x8 block, arranged in
       * 64x64 superblocks, so only the first sbw * sbh * 1024 bytes of the
       * image are ever written.  Copying the whole luma * 3/2 span instead
       * is several times more data than needed. */
      st->mv_bytes = ((width + 63) / 64) * ((height + 63) / 64) * 1024u;
      st->scratch_size = st->mv_offset + (1u + VP9_MV_SLOTS) * st->mv_size;
      st->scratch = mwv207_video_buffer_create_2(
         decoder->base.context, st->scratch_size, PIPE_USAGE_STAGING);
      if (!st->scratch) {
         FREE(st);
         return NULL;
      }
      st->scratch_cpu =
         decoder->ws->bo_map(decoder->ws, st->scratch->res->bo);
      if (st->scratch_cpu)
         memset(st->scratch_cpu, 0, st->scratch_size);
      if (!st->scratch_cpu) {
         vp9_destroy_state(st);
         return NULL;
      }
      memset(st->scratch_cpu, 0, st->scratch_size);
      dec->vp9_state = st;
   }

   return st;
}

/* ---------------------------------------------------------------------------
 * Probability adaptation for frame_parallel_decoding_mode == 0
 *
 * When a stream is not coded for frame-parallel decoding and is not error
 * resilient, VP9 adapts the entropy probabilities of every frame from the
 * symbol counts collected while decoding it.  The ASIC counts the symbols in
 * the context counter buffer (same layout as
 * struct mwv207_vp9_entropy_counts) but keeps its own probability table
 * untouched, so the adaptation has to happen here and the result has to be
 * stored in the frame context the next frame will start from.
 *
 * The algorithm is the one from libvpx: a probability is moved towards the
 * per-node estimate n0/(n0+n1), by at most count * max_update_factor /
 * count_sat (in 1/256 units).
 * ------------------------------------------------------------------------- */

#define VP9_TREE_SIZE_INTRA_MODE 18
#define VP9_MODE_MV_COUNT_SAT 20
static const int8_t vp9_tree_intra_mode[VP9_TREE_SIZE_INTRA_MODE] = {
   0, 2, -9, 4, -1, 6, 8, 12, -2, 10, -4, -5, -3, 14, -8, 16, -6, -7
};
static const int8_t vp9_tree_partition[6] = { -0, 2, -1, 4, -2, -3 };
static const int8_t vp9_tree_switchable[4] = { -0, 2, -1, -2 };
static const int8_t vp9_tree_mv_joint[6] = { -0, 2, -1, 4, -2, -3 };
static const int8_t vp9_tree_mv_class[20] = {
   -0, 2, -1, 4, 6, 8, -2, -3, 10, 12, -4, -5, -6, 14, 16, 18, -7, -8, -9, -10
};
static const int8_t vp9_tree_mv_class0[2] = { -0, -1 };
static const int8_t vp9_tree_mv_fp[6] = { -0, 2, -1, 4, -2, -3 };
static const int8_t vp9_tree_tx32[6] = { -0, 2, -1, 4, -2, -3 };
static const int8_t vp9_tree_tx16[4] = { -0, 2, -1, -2 };
static const int8_t vp9_tree_tx8[2] = { -0, -1 };

#define VP9_COEF_COUNT_SAT 24


static uint8_t
vp9_prob_from_counts(uint32_t n0, uint32_t n1)
{
   uint32_t den = n0 + n1;
   uint32_t p;

   if (!den)
      return 128;

   p = (uint32_t)(((uint64_t)n0 * 256u + (den >> 1)) / den);
   if (p < 1)
      p = 1;
   if (p > 255)
      p = 255;
   return (uint8_t)p;
}

static uint8_t
vp9_weighted_prob(uint8_t pre, uint8_t prob, uint32_t factor)
{
   return (uint8_t)((pre * (256u - factor) + prob * factor + 128u) >> 8);
}

static uint8_t
vp9_merge_mode_mv(uint8_t pre, uint32_t n0, uint32_t n1)
{
   static const uint8_t count_to_factor[VP9_MODE_MV_COUNT_SAT + 1] = {
      0, 6, 12, 19, 25, 32, 38, 44, 51, 57, 64,
      70, 76, 83, 89, 96, 102, 108, 115, 121, 128
   };
   uint32_t den = n0 + n1, count, factor;
   uint8_t prob;

   if (!den)
      return pre;

   count = den > VP9_MODE_MV_COUNT_SAT ? VP9_MODE_MV_COUNT_SAT : den;
   factor = count_to_factor[count];
   prob = vp9_prob_from_counts(n0, n1);
   return vp9_weighted_prob(pre, prob, factor);
}

static uint8_t
vp9_merge_coef(uint8_t pre, uint32_t n0, uint32_t n1, uint32_t count_sat,
               uint32_t max_update_factor)
{
   uint32_t den = n0 + n1, count, factor;
   uint8_t prob;

   if (!den)
      return pre;

   count = den > count_sat ? count_sat : den;
   factor = max_update_factor * count / count_sat;
   prob = vp9_prob_from_counts(n0, n1);
   return vp9_weighted_prob(pre, prob, factor);
}

/* Walk one binary coding tree and adapt every internal node.  The leaf index
 * of `counts` is the symbol value the tree codes. */
static uint32_t
vp9_tree_merge(const int8_t *tree, int32_t node, const uint8_t *pre,
               const uint32_t *counts, uint8_t *probs)
{
   int32_t l = tree[node], r = tree[node + 1];
   uint32_t lc = l <= 0 ? counts[-l] : vp9_tree_merge(tree, l, pre, counts, probs);
   uint32_t rc = r <= 0 ? counts[-r] : vp9_tree_merge(tree, r, pre, counts, probs);

   probs[node >> 1] = vp9_merge_mode_mv(pre[node >> 1], lc, rc);
   return lc + rc;
}

static void
vp9_adapt_coef_probs(struct mwv207_vp9_adaptive_entropy_probs *fc,
                     const struct mwv207_vp9_adaptive_entropy_probs *pre,
                     const struct mwv207_vp9_entropy_counts *ct,
                     uint32_t count_sat, uint32_t update_factor)
{
   uint32_t t, i, j, k, l, m;

   for (t = 0; t < 4; t++) {
      uint8_t *probs;
      const uint8_t *pre_probs;
      const uint32_t *counts;
      const uint32_t *eobs;

      probs = t == 0 ? &fc->prob_coeffs[0][0][0][0][0]
                     : t == 1 ? &fc->prob_coeffs8x8[0][0][0][0][0]
                     : t == 2 ? &fc->prob_coeffs16x16[0][0][0][0][0]
                              : &fc->prob_coeffs32x32[0][0][0][0][0];
      pre_probs = t == 0 ? &pre->prob_coeffs[0][0][0][0][0]
                         : t == 1 ? &pre->prob_coeffs8x8[0][0][0][0][0]
                         : t == 2 ? &pre->prob_coeffs16x16[0][0][0][0][0]
                                  : &pre->prob_coeffs32x32[0][0][0][0][0];
      counts = t == 0 ? &ct->count_coeffs[0][0][0][0][0]
                      : t == 1 ? &ct->count_coeffs8x8[0][0][0][0][0]
                      : t == 2 ? &ct->count_coeffs16x16[0][0][0][0][0]
                               : &ct->count_coeffs32x32[0][0][0][0][0];
      eobs = &ct->count_eobs[t][0][0][0][0];

      for (i = 0; i < BLOCK_TYPES; i++)
         for (j = 0; j < REF_TYPES; j++)
            for (k = 0; k < COEF_BANDS; k++) {
               uint32_t ctx_count = k == 0 ? 3 : PREV_COEF_CONTEXTS;
               for (l = 0; l < ctx_count; l++) {
                  uint32_t idx = (((i * REF_TYPES + j) * COEF_BANDS + k)
                                  * PREV_COEF_CONTEXTS + l);
                  uint32_t n0 = counts[idx * 4 + 0];
                  uint32_t n1 = counts[idx * 4 + 1];
                  uint32_t n2 = counts[idx * 4 + 2];
                  uint32_t neob = counts[idx * 4 + 3];
                  uint32_t etot = eobs[idx];
                  uint32_t branch[3][2];

                  branch[0][0] = neob;
                  branch[0][1] = etot > neob ? etot - neob : 0;
                  branch[1][0] = n0;
                  branch[1][1] = n1 + n2;
                  branch[2][0] = n1;
                  branch[2][1] = n2;
                  for (m = 0; m < UNCONSTRAINED_NODES; m++)
                     probs[idx * 4 + m] = vp9_merge_coef(
                        pre_probs[idx * 4 + m], branch[m][0], branch[m][1],
                        count_sat, update_factor);
               }
            }
   }
}

static void
vp9_adapt_mode_probs(struct mwv207_vp9_adaptive_entropy_probs *fc,
                     const struct mwv207_vp9_adaptive_entropy_probs *pre,
                     const struct mwv207_vp9_entropy_counts *ct,
                     uint32_t tx_mode_select, uint32_t switchable_interp)
{
   uint32_t i, j;

   for (i = 0; i < INTRA_INTER_CONTEXTS; i++)
      fc->intra_inter_prob[i] = vp9_merge_mode_mv(
         pre->intra_inter_prob[i], ct->intra_inter_count[i][0],
         ct->intra_inter_count[i][1]);

   for (i = 0; i < COMP_INTER_CONTEXTS; i++)
      fc->comp_inter_prob[i] = vp9_merge_mode_mv(
         pre->comp_inter_prob[i], ct->comp_inter_count[i][0],
         ct->comp_inter_count[i][1]);

   for (i = 0; i < REF_CONTEXTS_DEC; i++)
      fc->comp_ref_prob[i] = vp9_merge_mode_mv(
         pre->comp_ref_prob[i], ct->comp_ref_count[i][0],
         ct->comp_ref_count[i][1]);

   for (i = 0; i < REF_CONTEXTS_DEC; i++)
      for (j = 0; j < 2; j++)
         fc->single_ref_prob[i][j] = vp9_merge_mode_mv(
            pre->single_ref_prob[i][j], ct->single_ref_count[i][j][0],
            ct->single_ref_count[i][j][1]);

   /* The hardware does not count the four inter modes; it counts the two
    * branches of each node of the inter mode tree, which is what the
    * adaptation needs anyway. */
   for (i = 0; i < INTER_MODE_CONTEXTS_DEC; i++)
      for (j = 0; j < VP9_INTER_MODES - 1; j++)
         fc->inter_mode_prob[i][j] = vp9_merge_mode_mv(
            pre->inter_mode_prob[i][j],
            ct->inter_mode_counts[i][j][0],
            ct->inter_mode_counts[i][j][1]);

   for (i = 0; i < BLOCK_SIZE_GROUPS; i++) {
      uint8_t probs[9];
      const uint8_t pre_probs[9] = {
         pre->sb_ymode_prob[i][0], pre->sb_ymode_prob[i][1],
         pre->sb_ymode_prob[i][2], pre->sb_ymode_prob[i][3],
         pre->sb_ymode_prob[i][4], pre->sb_ymode_prob[i][5],
         pre->sb_ymode_prob[i][6], pre->sb_ymode_prob[i][7],
         pre->sb_ymode_prob_b[i][0]
      };
      vp9_tree_merge(vp9_tree_intra_mode, 0, pre_probs,
                     ct->sb_ymode_counts[i], probs);
      for (j = 0; j < 8; j++)
         fc->sb_ymode_prob[i][j] = probs[j];
      fc->sb_ymode_prob_b[i][0] = probs[8];
   }

   for (i = 0; i < VP9_INTRA_MODES; i++) {
      uint8_t probs[9];
      const uint8_t pre_probs[9] = {
         pre->uv_mode_prob[i][0], pre->uv_mode_prob[i][1],
         pre->uv_mode_prob[i][2], pre->uv_mode_prob[i][3],
         pre->uv_mode_prob[i][4], pre->uv_mode_prob[i][5],
         pre->uv_mode_prob[i][6], pre->uv_mode_prob[i][7],
         pre->uv_mode_prob_b[i][0]
      };
      vp9_tree_merge(vp9_tree_intra_mode, 0, pre_probs,
                     ct->uv_mode_counts[i], probs);
      for (j = 0; j < 8; j++)
         fc->uv_mode_prob[i][j] = probs[j];
      fc->uv_mode_prob_b[i][0] = probs[8];
   }

   for (i = 0; i < NUM_PARTITION_CONTEXTS; i++)
      vp9_tree_merge(vp9_tree_partition, 0,
                     pre->partition_prob[INTER_FRAME][i],
                     ct->partition_counts[i],
                     fc->partition_prob[INTER_FRAME][i]);

   if (switchable_interp) {
      for (i = 0; i < VP9_SWITCHABLE_FILTERS + 1; i++)
         vp9_tree_merge(vp9_tree_switchable, 0,
                        pre->switchable_interp_prob[i],
                        ct->switchable_interp_counts[i],
                        fc->switchable_interp_prob[i]);
   }

   if (tx_mode_select) {
      for (i = 0; i < TX_SIZE_CONTEXTS_DEC; i++) {
         vp9_tree_merge(vp9_tree_tx8, 0, pre->tx8x8_prob[i],
                        ct->tx8x8_count[i], fc->tx8x8_prob[i]);
         vp9_tree_merge(vp9_tree_tx16, 0, pre->tx16x16_prob[i],
                        ct->tx16x16_count[i], fc->tx16x16_prob[i]);
         vp9_tree_merge(vp9_tree_tx32, 0, pre->tx32x32_prob[i],
                        ct->tx32x32_count[i], fc->tx32x32_prob[i]);
      }
   }

   for (i = 0; i < MBSKIP_CONTEXTS; i++)
      fc->mbskip_probs[i] = vp9_merge_mode_mv(
         pre->mbskip_probs[i], ct->mbskip_count[i][0], ct->mbskip_count[i][1]);
}

static void
vp9_adapt_mv_probs(struct mwv207_vp9_adaptive_entropy_probs *fc,
                   const struct mwv207_vp9_adaptive_entropy_probs *pre,
                   const struct mwv207_vp9_entropy_counts *ct,
                   uint32_t allow_hp)
{
   uint32_t i, j;

   vp9_tree_merge(vp9_tree_mv_joint, 0, pre->nmvc.joints,
                  ct->nmvcount.joints, fc->nmvc.joints);

   for (i = 0; i < 2; i++) {
      fc->nmvc.sign[i] = vp9_merge_mode_mv(pre->nmvc.sign[i],
                                           ct->nmvcount.sign[i][0],
                                           ct->nmvcount.sign[i][1]);
      vp9_tree_merge(vp9_tree_mv_class, 0, pre->nmvc.classes[i],
                     ct->nmvcount.classes[i], fc->nmvc.classes[i]);
      vp9_tree_merge(vp9_tree_mv_class0, 0, pre->nmvc.class0[i],
                     ct->nmvcount.class0[i], fc->nmvc.class0[i]);

      for (j = 0; j < MV_OFFSET_BITS; j++)
         fc->nmvc.bits[i][j] = vp9_merge_mode_mv(
            pre->nmvc.bits[i][j], ct->nmvcount.bits[i][j][0],
            ct->nmvcount.bits[i][j][1]);

      for (j = 0; j < CLASS0_SIZE; j++)
         vp9_tree_merge(vp9_tree_mv_fp, 0, pre->nmvc.class0_fp[i][j],
                        ct->nmvcount.class0_fp[i][j],
                        fc->nmvc.class0_fp[i][j]);

      vp9_tree_merge(vp9_tree_mv_fp, 0, pre->nmvc.fp[i],
                     ct->nmvcount.fp[i], fc->nmvc.fp[i]);

      if (allow_hp) {
         fc->nmvc.class0_hp[i] = vp9_merge_mode_mv(
            pre->nmvc.class0_hp[i], ct->nmvcount.class0_hp[i][0],
            ct->nmvcount.class0_hp[i][1]);
         fc->nmvc.hp[i] = vp9_merge_mode_mv(
            pre->nmvc.hp[i], ct->nmvcount.hp[i][0],
            ct->nmvcount.hp[i][1]);
      }
   }
}

/* Adapt the probabilities of the frame that the decode unit has just
 * finished; the result becomes the starting point of the next frame that
 * uses the same frame context. */
static void
vp9_adapt_probs(struct mwv207_vp9_state *st, struct mwv207_vp9_slice_header *sh)
{
   struct mwv207_vp9_entropy_probs *ctx;
   const struct mwv207_vp9_entropy_counts *ct;
   uint32_t count_sat, update_factor;
   int intra_only;

   if (!sh || sh->error_resilient || sh->frame_parallel_decoding)
      return;
   if (sh->frame_context_idx >= NUM_FRAME_CONTEXTS)
      return;

   intra_only = sh->key_frame || sh->intra_only;
   ctx = &st->saved_ctx[sh->frame_context_idx];
   ct = (const struct mwv207_vp9_entropy_counts *)((const uint8_t *)st->scratch_cpu
                                                   + VP9_COUNTER_OFFSET);

   if (intra_only) {
      count_sat = VP9_COEF_COUNT_SAT;
      update_factor = 112;
   } else if (st->prev_frame_key) {
      count_sat = VP9_COEF_COUNT_SAT;
      update_factor = 128;
   } else {
      count_sat = VP9_COEF_COUNT_SAT;
      update_factor = 112;
   }

   {
      struct mwv207_vp9_entropy_probs fc = *ctx;
      const struct mwv207_vp9_entropy_probs *pre = &st->pre_ctx[sh->frame_context_idx];

      vp9_adapt_coef_probs(&fc.a, &pre->a, ct, count_sat, update_factor);
      if (!intra_only) {
         vp9_adapt_mode_probs(&fc.a, &pre->a, ct,
                              sh->transform_mode == 4,
                              sh->mcomp_filter_type == 4);
         vp9_adapt_mv_probs(&fc.a, &pre->a, ct, sh->allow_high_precision_mv);
      }
      *ctx = fc;
   }
}

static void
vp9_set_addr(struct mwv207_video_reg_buf *reg_buf, uint32_t id_lsb,
             struct mwv207_bo *bo, uint64_t offset)
{
   uint32_t id_msb;

   switch (id_lsb) {
   case JMDEC_STREAM_BASE_LSB: id_msb = JMDEC_STREAM_BASE_MSB; break;
   case JMDEC_DEC_OUT_YBASE_LSB: id_msb = JMDEC_DEC_OUT_YBASE_MSB; break;
   case JMDEC_DEC_OUT_CBASE_LSB: id_msb = JMDEC_DEC_OUT_CBASE_MSB; break;
   case JMDEC_DEC_OUT_DBASE_LSB: id_msb = JMDEC_DEC_OUT_DBASE_MSB; break;
   case JMDEC_PROB_TAB_BASE_LSB: id_msb = JMDEC_PROB_TAB_BASE_MSB; break;
   case JMDEC_CTX_COUNTER_BASE_LSB: id_msb = JMDEC_CTX_COUNTER_BASE_MSB; break;
   case JMDEC_SEGMENT_READ_BASE_LSB: id_msb = JMDEC_SEGMENT_READ_BASE_MSB; break;
   case JMDEC_SEGMENT_WRITE_BASE_LSB: id_msb = JMDEC_SEGMENT_WRITE_BASE_MSB; break;
   case JMDEC_TILE_BASE_LSB: id_msb = JMDEC_TILE_BASE_MSB; break;
   case JMDEC_DEC_VERT_FILT_BASE_LSB: id_msb = JMDEC_DEC_VERT_FILT_BASE_MSB; break;
   case JMDEC_DEC_BSD_CTRL_BASE_LSB: id_msb = JMDEC_DEC_BSD_CTRL_BASE_MSB; break;
   default: return;
   }

   reg_buf->set_addr_reg_hi(reg_buf, id_msb, bo, offset,
                            MWV207_BO_ACC_FLAG_WRITE);
   reg_buf->set_addr_reg(reg_buf, id_lsb, bo, offset,
                         MWV207_BO_ACC_FLAG_WRITE);
}

void
mwv207_dec_vp9_set_regs(struct mwv207_video_decoder *decoder,
                        struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;
   struct mwv207_vp9_picture_desc *pic_param = &decoder->picture_desc.vp9;
   struct mwv207_video_buffer *buf = (struct mwv207_video_buffer *)target;
   struct mwv207_video_buffer *bs_buf =
      decoder->bs_buffers[decoder->cur_buffer];
   struct mwv207_vp9_state *st;
   struct mwv207_vp9_slice_header *sh;
   const uint8_t *stream = decoder->streams[0];
   uint32_t stream_len = decoder->streams_len[0];
   uint32_t width = pic_param->picture_parameter.frame_width;
   uint32_t height = pic_param->picture_parameter.frame_height;
   uint32_t bit_depth = pic_param->picture_parameter.bit_depth;
   uint32_t tile_off, tile_len, out_w_y;
   uint32_t vp9_luma_size = 0;
   int ret;

   if (!width || !height || !stream || !bs_buf) {
      MWV207_LOG(MWV207_LEVEL_ERROR, "vp9: invalid frame setup\n");
      reg_buf->set_reg(reg_buf, JMDEC_DEC_E, 0);
      return;
   }

   st = vp9_get_state(decoder, width, height);
   if (!st) {
      MWV207_LOG(MWV207_LEVEL_ERROR, "vp9: scratch allocation failed\n");
      reg_buf->set_reg(reg_buf, JMDEC_DEC_E, 0);
      return;
   }
   sh = &st->sh;
   memset(sh, 0, sizeof(*sh));
   /* The reference slots are addressed by the bitstream's own slot numbers:
    * we copy every decoded frame into the slots selected by
    * refresh_frame_flags, so a refreshed slot always holds the newest frame
    * and the slot->frame mapping is the identity.  Keep it that way after the
    * memset, otherwise every active reference resolves to slot 0 and blocks
    * that use GOLDEN/ALTREF predict from the LAST frame instead. */
   {
      uint32_t map_i;
      for (map_i = 0; map_i < NUM_REF_FRAMES; map_i++)
         sh->ref_frame_map[map_i] = (int32_t)map_i;
   }

   sh->dec_mode = DEC_MODE_VP9;
   sh->width = width;
   sh->height = height;
   sh->bit_depth = bit_depth ? bit_depth : 8;
   sh->vp_profile = pic_param->picture_parameter.profile;

   sh->pic_param_passed_width = width;
   sh->pic_param_passed_height = height;
   memcpy(sh->mb_ref_lf_delta, st->lf_ref_delta, sizeof(sh->mb_ref_lf_delta));
   memcpy(sh->mb_mode_lf_delta, st->lf_mode_delta, sizeof(sh->mb_mode_lf_delta));
   if (!st->ctx_ready) {
      struct mwv207_vp9_slice_header defaults;
      uint32_t i;

      memset(&defaults, 0, sizeof(defaults));
      mwv207_vp9_reset_probs(&defaults);
      for (i = 0; i < NUM_FRAME_CONTEXTS; i++)
         st->saved_ctx[i] = defaults.entropy;
      st->ctx_ready = true;
   }
   memcpy(st->pre_ctx, st->saved_ctx, sizeof(st->saved_ctx));
   ret = mwv207_vp9_parse_frame(
      sh, stream, stream_len, width, height, st->saved_ctx,
      pic_param->picture_parameter.frame_header_length_in_bytes,
      pic_param->picture_parameter.first_partition_size);
   if (!ret) {
      memcpy(st->lf_ref_delta, sh->mb_ref_lf_delta, sizeof(st->lf_ref_delta));
      memcpy(st->lf_mode_delta, sh->mb_mode_lf_delta, sizeof(st->lf_mode_delta));
   }
   if (ret) {
      MWV207_LOG(MWV207_LEVEL_ERROR, "vp9: header parse failed (%d)\n", ret);
      reg_buf->set_reg(reg_buf, JMDEC_DEC_E, 0);
      return;
   }

   width = sh->width ? sh->width : width;
   height = sh->height ? sh->height : height;
   /* Inter frames inherit the picture size from the reference frame and
    * signal no dimensions of their own; the reference registers below must
    * still carry the real size or motion compensation reads garbage. */
   sh->width = width;
   sh->height = height;

   /* The ASIC decodes the tile payload only; the frame header and the first
    * partition are consumed by the software parser. */
   tile_off = sh->frame_tag_size + sh->offset_to_dct_parts;
   if (tile_off >= stream_len) {
      MWV207_LOG(MWV207_LEVEL_ERROR, "vp9: bad partition sizes (%u/%u)\n",
                 sh->frame_tag_size, sh->offset_to_dct_parts);
      reg_buf->set_reg(reg_buf, JMDEC_DEC_E, 0);
      return;
   }
   tile_len = stream_len - tile_off;
   if (!sh->log2_tile_columns && !sh->log2_tile_rows) {
      st->tile_n = 1;
      st->tile_off[0] = tile_off;
      st->tile_len[0] = tile_len;
   }

   memcpy(st->scratch_cpu + VP9_PROB_OFFSET, &sh->entropy,
          VP9_PROB_TABLE_SIZE);
   {
      uint32_t sc = (((width + 7) & ~7u) + 63) >> 6;
      uint32_t sr = (((height + 7) & ~7u) + 63) >> 6;
      uint32_t l2c = sh->log2_tile_columns;
      uint32_t l2r = sh->log2_tile_rows;
      uint32_t ncol = 1u << l2c;
      uint32_t nrow = 1u << l2r;
      uint32_t n = ncol * nrow;
      uint16_t *tile = (uint16_t *)((uint8_t *)st->scratch_cpu
                                    + st->tile_offset);
      uint16_t cinfo[VP9_MAX_TILE_COLS], rinfo[VP9_MAX_TILE_ROWS];
      uint32_t i, j, k = 0, acc;

      if (l2c > 6 || l2r > 2) {
         MWV207_LOG(MWV207_LEVEL_ERROR,
                    "vp9: invalid tile grid %u x %u\n", ncol, nrow);
         reg_buf->set_reg(reg_buf, JMDEC_DEC_E, 0);
         return;
      }

      /* VP9 tile payload layout (spec 6.2.5): row-major over the
       * (ncol x nrow) grid, a 4-byte big-endian size in front of every tile
       * but the last, the last one taking the remainder of the frame. */
      if (n > 1) {
         uint32_t off = tile_off, sz, i2;

         if (off > stream_len) {
            MWV207_LOG(MWV207_LEVEL_ERROR,
                       "vp9: tile size table beyond the frame\n");
            reg_buf->set_reg(reg_buf, JMDEC_DEC_E, 0);
            return;
         }
         /* VP9 interleaves each tile with a 4-byte big-endian size in front
          * of it (all tiles but the last); the last one is the remainder. */
         for (i2 = 0; i2 + 1 < n; i2++) {
            const uint8_t *p = stream + off;
            if (off + 4 > stream_len)
               break;
            sz = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                 ((uint32_t)p[2] << 8) | p[3];
            off += 4;
            if (sz == 0 || off + sz > stream_len) {
               MWV207_LOG(MWV207_LEVEL_ERROR,
                          "vp9: bad tile size %u (tile %u)\n", sz, i2);
               reg_buf->set_reg(reg_buf, JMDEC_DEC_E, 0);
               return;
            }
            st->tile_off[i2] = off;
            st->tile_len[i2] = sz;
            off += sz;
         }
         st->tile_off[n - 1] = off;
         st->tile_len[n - 1] = stream_len - off;
         st->tile_n = n;
      }

      /* Upstream hantro_g2: per-column / per-row cumulative superblock
       * deltas, table laid out row-major as (coldelta, rowdelta) pairs.
       * ncol == nrow == 1 degenerates to (supercols, superrows). */
      acc = 0;
      for (i = 0; i < ncol; i++) {
         uint32_t next = (i + 1) * sc / ncol;
         cinfo[i] = (uint16_t)(next - acc);
         acc = next;
      }
      acc = 0;
      for (i = 0; i < nrow; i++) {
         uint32_t next = (i + 1) * sr / nrow;
         rinfo[i] = (uint16_t)(next - acc);
         acc = next;
      }
      for (i = 0; i < nrow; i++) {
         uint32_t r = rinfo[i];
         for (j = 0; j < ncol; j++) {
            tile[k++] = cinfo[j];
            tile[k++] = (uint16_t)r;
         }
      }
   }
   __builtin___clear_cache((char *)st->scratch_cpu,
                           (char *)st->scratch_cpu + st->scratch_size);

   dec->dec_param.bit_depth = sh->bit_depth;
   dec->dec_param.pp_enabled = 1;
   dec->dec_param.w_align = 6;
   dec->dec_param.use_video_compressor = 0;
   mwv207_video_dec_get_hw_features_by_id(0x00001F76, &dec->hw_feature);

   /* The decode unit writes a tiled NV12 picture into a staging buffer; the
    * PP unit converts it into the linear surface (both 8-bit now). */
   {
      out_w_y = NEXT_MULTIPLE(4 * (((width + 7) >> 3) << 3)
                              * dec->dec_param.bit_depth,
                              ALIGN(dec->dec_param.w_align) * 8) / 8;
      /* The tiled NV12 layout stores groups of four luma rows per stride,
       * so one luma plane is stride * ceil(height / 4) bytes; the chroma
       * planes use half that stride and ceil(height / 8) rows.  Sizes must
       * be rounded up, otherwise e.g. 640x358 loses the padding rows and the
       * chroma planes overlap the luma tail. */
      uint32_t chroma_rows = (height + 6) / 8;
      /* The decode unit always pads the luma plane to twice the number of
       * chroma row groups (i.e. to a multiple of eight lines), so derive the
       * luma size from the chroma rows instead of from height / 4. */
      uint32_t luma_rows = 2 * chroma_rows;
      uint32_t luma_size = out_w_y * luma_rows;
      uint32_t chroma_size = (out_w_y / 2) * chroma_rows;
      uint32_t frame_bytes = luma_size + 2 * chroma_size;
      vp9_luma_size = luma_size;
      st->ref_frame_bytes = frame_bytes;
      uint32_t buff_size = NEXT_MULTIPLE(frame_bytes, 16);

      buf->mwv207_offset_info.luma_size = luma_size;
      if (!buf->res)
         mwv207_video_buffer_alloc_buffer(decoder->base.context, buf,
                                          buff_size, PIPE_USAGE_STAGING);
      if (!buf->res) {
         MWV207_LOG(MWV207_LEVEL_ERROR, "vp9: output buffer allocation failed\n");
         reg_buf->set_reg(reg_buf, JMDEC_DEC_E, 0);
         return;
      }
      /* A show_existing_frame packet carries no coded picture: it asks for
       * an already decoded reference frame to be displayed again.  Hand the
       * copy we keep of that frame straight to the output surface and leave
       * the decode unit idle. */
      if (sh->show_existing_frame) {
         uint32_t idx = sh->show_existing_frame_index & 7u;
         uint32_t slot = (uint32_t)sh->ref_frame_map[idx];
         uint32_t need = frame_bytes;

         if (st->ref_store && st->ref_store->res && slot < 16 &&
             st->ref_luma == luma_size) {
            uint8_t *src = decoder->ws->bo_map(decoder->ws,
                                               st->ref_store->res->bo);
            uint8_t *dst = decoder->ws->bo_map(decoder->ws, buf->res->bo);
            if (src && dst)
               memcpy(dst, src + (size_t)slot * need, need);
         } else {
            MWV207_LOG(MWV207_LEVEL_ERROR,
                       "vp9: show_existing_frame slot %u unavailable\n",
                       slot);
         }
         reg_buf->set_reg(reg_buf, JMDEC_DEC_E, 0);
         st->pending = NULL;
         return;
      }
      vp9_set_addr(reg_buf, JMDEC_DEC_OUT_YBASE_LSB, buf->res->bo, 0);
      vp9_set_addr(reg_buf, JMDEC_DEC_OUT_CBASE_LSB, buf->res->bo, luma_size);
      /* The motion vectors the entropy engine decodes are written to the
       * "direct MV" area so that later blocks can use them for spatial MV
       * prediction; without it every block after the first row predicts
       * from garbage.  The vendor programs DEC_OUT_DBASE for VP9 too. */
      vp9_set_addr(reg_buf, JMDEC_DEC_OUT_DBASE_LSB,
                   st->scratch->res->bo, st->mv_offset);
   }

   reg_buf->set_reg(reg_buf, JMDEC_DEC_MODE, DEC_MODE_VP9);
   mwv207_dec_set_common_config_regs(dec);
   /* Same single-core setup the HEVC path uses; defaults differ enough to
    * leave the VP9 core waiting on a second pipe. */
   reg_buf->set_reg(reg_buf, JMDEC_HDR_SKIP_LENGTH, 0);
   reg_buf->set_reg(reg_buf, JMDEC_APF_ONE_PID, 0);
   reg_buf->set_reg(reg_buf, JMDEC_DEC_MULTICORE_E, 0);
   reg_buf->set_reg(reg_buf, JMDEC_DEC_WRITESTAT_E, 0);
   reg_buf->set_reg(reg_buf, JMDEC_PIC_WIDTH_IN_CBS, (width + 7) >> 3);
   reg_buf->set_reg(reg_buf, JMDEC_PIC_HEIGHT_IN_CBS, (height + 7) >> 3);
   reg_buf->set_reg(reg_buf, JMDEC_PIC_WIDTH_4X4, ((width + 7) >> 2) & ~1u);
   reg_buf->set_reg(reg_buf, JMDEC_PIC_HEIGHT_4X4, ((height + 7) >> 2) & ~1u);
   /* VP9 superblock is 64x64, minimum coding block 8x8 (vendor uses
    * MAX_CB_SIZE=6 / MIN_CB_SIZE=3). */
   reg_buf->set_reg(reg_buf, JMDEC_MAX_CB_SIZE, 6);
   reg_buf->set_reg(reg_buf, JMDEC_MIN_CB_SIZE, 3);

   reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_DIS, 0);
   /* no DEC400 compressor: decoder must write uncompressed output */
   reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_EC_BYPASS, 1);
   reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_Y_STRIDE, out_w_y);
   reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_C_STRIDE, out_w_y);
   reg_buf->set_reg(reg_buf, JMDEC_BIT_DEPTH_OUT_MINUS8, sh->bit_depth - 8);
   reg_buf->set_reg(reg_buf, JMDEC_BIT_DEPTH_Y_MINUS8, sh->bit_depth - 8);
   reg_buf->set_reg(reg_buf, JMDEC_BIT_DEPTH_C_MINUS8, sh->bit_depth - 8);
   {
      /* The ASIC derives the block quantiser from segment 0's feature word
       * even when segmentation is disabled; 294..296 are signed per-plane
       * deltas relative to that base. */
      uint32_t dy = (uint32_t)(sh->lossless ? 0 : (int32_t)sh->qp_ydc) & 0x3f;
      uint32_t dc = (uint32_t)(sh->lossless ? 0 : (int32_t)sh->qp_ch_dc) & 0x3f;
      uint32_t da = (uint32_t)(sh->lossless ? 0 : (int32_t)sh->qp_ch_ac) & 0x3f;
      reg_buf->set_reg(reg_buf, JMDEC_QP_DELTA_Y_DC, dy);
      reg_buf->set_reg(reg_buf, JMDEC_QP_DELTA_CH_DC, dc);
      reg_buf->set_reg(reg_buf, JMDEC_QP_DELTA_CH_AC, da);
   }
   /* Segmentation: reuse the previous frame's feature values when this
    * frame does not carry a fresh set, then hand the resolved per-segment
    * words to the hardware. */
   if (sh->segment_enabled) {
      if (sh->segment_data_update || !st->seg_feature_valid) {
         memcpy(st->seg_feature_enable, sh->segment_feature_enable,
                sizeof(st->seg_feature_enable));
         memcpy(st->seg_feature_data, sh->segment_feature_data,
                sizeof(st->seg_feature_data));
         st->seg_feature_mode = sh->segment_feature_mode;
         st->seg_feature_valid = true;
      } else {
         memcpy(sh->segment_feature_enable, st->seg_feature_enable,
                sizeof(st->seg_feature_enable));
         memcpy(sh->segment_feature_data, st->seg_feature_data,
                sizeof(st->seg_feature_data));
         sh->segment_feature_mode = st->seg_feature_mode;
      }
   }
   vp9_write_segment_regs(reg_buf, sh);
   reg_buf->set_reg(reg_buf, JMDEC_SEGMENT_E, sh->segment_enabled ? 1 : 0);
   reg_buf->set_reg(reg_buf, JMDEC_SEGMENT_UPD_E,
                    sh->segment_map_update ? 1 : 0);
   reg_buf->set_reg(reg_buf, JMDEC_SEGMENT_TEMP_UPD_E,
                    sh->segment_map_temporal_update ? 1 : 0);
   reg_buf->set_reg(reg_buf, JMDEC_LOSSLESS_E, sh->lossless ? 1 : 0);
   reg_buf->set_reg(reg_buf, JMDEC_IDR_PIC_E,
                    (sh->key_frame || sh->intra_only) ? 1 : 0);
   reg_buf->set_reg(reg_buf, JMDEC_TRANSFORM_MODE, sh->transform_mode & 0x7);
   reg_buf->set_reg(reg_buf, JMDEC_MCOMP_FILT_TYPE, sh->mcomp_filter_type & 0x7);
   reg_buf->set_reg(reg_buf, JMDEC_HIGH_PREC_MV_E,
                    sh->key_frame ? 0 : (sh->allow_high_precision_mv ? 1 : 0));
   reg_buf->set_reg(reg_buf, JMDEC_COMP_PRED_MODE, sh->comp_pred_mode & 0x3);
   reg_buf->set_reg(reg_buf, JMDEC_TEMPOR_MVP_E, 0);
   reg_buf->set_reg(reg_buf, JMDEC_COMP_PRED_FIXED_REF, sh->comp_fixed_ref & 0x3);
   reg_buf->set_reg(reg_buf, JMDEC_COMP_PRED_VAR_REF0, sh->comp_var_ref[0] & 0x3);
   reg_buf->set_reg(reg_buf, JMDEC_COMP_PRED_VAR_REF1, sh->comp_var_ref[1] & 0x3);
   /* The ASIC stores the motion vectors of every block in the dir_mv area
    * and reads the neighbours back while decoding the next blocks; without
    * WRITE_MVS_E the prediction drifts apart towards the bottom of the
    * frame.  The vendor enables it for inter frames only. */
   if (!sh->key_frame && !sh->intra_only && st->scratch && st->scratch->res) {
      reg_buf->set_reg(reg_buf, JMDEC_WRITE_MVS_E, 1);
      vp9_set_addr(reg_buf, JMDEC_DIR_MV_BASE_LSB, st->scratch->res->bo,
                   st->mv_offset);
   } else {
      reg_buf->set_reg(reg_buf, JMDEC_WRITE_MVS_E, 0);
   }
   /* VP9 predicts motion vectors from the previous frame's MV image when the
    * stream is not error resilient and the previous frame was an inter frame
    * of the same size (use_prev_frame_mvs).  The ASIC does this when
    * TEMPOR_MVP_E is set; without it the second half of every P frame
    * predicts from a zero MV field and drifts. */
   {
      bool tempor = !sh->key_frame && !sh->intra_only &&
                    !sh->error_resilient &&
                    sh->width == st->width &&
                    sh->height == st->height &&
                    st->prev_was_inter && st->prev_was_shown;
      reg_buf->set_reg(reg_buf, JMDEC_TEMPOR_MVP_E, tempor ? 1 : 0);
      /* This frame becomes the "previous frame" for temporal MV prediction. */
      st->prev_was_inter = !sh->key_frame && !sh->intra_only && !sh->show_existing_frame;
      st->prev_was_shown = sh->show_frame != 0;
   }
   reg_buf->set_reg(reg_buf, JMDEC_DEC_TILE_INT_E, 0);

   reg_buf->set_reg(reg_buf, JMDEC_FILT_LEVEL, sh->loop_filter_level & 0x3f);
   reg_buf->set_reg(reg_buf, JMDEC_FILTERING_DIS,
                    sh->loop_filter_level ? 0 : 1);
   reg_buf->set_reg(reg_buf, JMDEC_FILT_SHARPNESS,
                    sh->loop_filter_sharpness & 0x7);
   /* The per-reference and per-mode loop filter level deltas are 7-bit
    * signed values; the hardware sign-extends them. */
   reg_buf->set_reg(reg_buf, JMDEC_FILT_REF_ADJ_0,
                    sh->mode_ref_lf_enabled ? (uint32_t)sh->mb_ref_lf_delta[0] : 0);
   reg_buf->set_reg(reg_buf, JMDEC_FILT_REF_ADJ_1,
                    sh->mode_ref_lf_enabled ? (uint32_t)sh->mb_ref_lf_delta[1] : 0);
   reg_buf->set_reg(reg_buf, JMDEC_FILT_REF_ADJ_2,
                    sh->mode_ref_lf_enabled ? (uint32_t)sh->mb_ref_lf_delta[2] : 0);
   reg_buf->set_reg(reg_buf, JMDEC_FILT_REF_ADJ_3,
                    sh->mode_ref_lf_enabled ? (uint32_t)sh->mb_ref_lf_delta[3] : 0);
   reg_buf->set_reg(reg_buf, JMDEC_FILT_MB_ADJ_0,
                    sh->mode_ref_lf_enabled ? (uint32_t)sh->mb_mode_lf_delta[0] : 0);
   reg_buf->set_reg(reg_buf, JMDEC_FILT_MB_ADJ_1,
                    sh->mode_ref_lf_enabled ? (uint32_t)sh->mb_mode_lf_delta[1] : 0);

   /* JMDEC_REFER_LTERM_E is a 32-bit field at dword 38, the same dword that
    * holds AREF_HOR/VER_SCALE; writing it must happen before the reference
    * scales or it wipes them. */
   reg_buf->set_reg(reg_buf, JMDEC_NO_FWD_REF_E, 0);
   reg_buf->set_reg(reg_buf, JMDEC_REFER_VALID_E, 1);
   reg_buf->set_reg(reg_buf, JMDEC_REFER_LTERM_E, 0);
   vp9_write_ref_regs(st, reg_buf, sh, vp9_luma_size, st->ref_frame_bytes, out_w_y);

   vp9_set_addr(reg_buf, JMDEC_PROB_TAB_BASE_LSB, st->scratch->res->bo,
                VP9_PROB_OFFSET);
   vp9_set_addr(reg_buf, JMDEC_CTX_COUNTER_BASE_LSB, st->scratch->res->bo,
                VP9_COUNTER_OFFSET);
   /* The segment map is a ping-pong buffer pair: the hardware decodes this
    * frame's segment ids into the write map, and a following frame that
    * does not carry a map update predicts its ids from the map this frame
    * left behind.  An intra-only or error resilient frame restarts the
    * prediction, so both maps go back to segment 0. */
   if (sh->intra_only || sh->error_resilient)
      memset(st->scratch_cpu + st->seg_offset, 0, st->seg_size * 2u);
   vp9_set_addr(reg_buf, JMDEC_SEGMENT_READ_BASE_LSB, st->scratch->res->bo,
                st->seg_offset + st->seg_active * st->seg_size);
   vp9_set_addr(reg_buf, JMDEC_SEGMENT_WRITE_BASE_LSB, st->scratch->res->bo,
                st->seg_offset + (1u - st->seg_active) * st->seg_size);
   if (sh->segment_enabled && sh->segment_map_update)
      st->seg_active ^= 1u;

   vp9_set_addr(reg_buf, JMDEC_TILE_BASE_LSB, st->scratch->res->bo,
                st->tile_offset);
   vp9_set_addr(reg_buf, JMDEC_DEC_VERT_FILT_BASE_LSB, st->scratch->res->bo,
                st->filter_offset);
   vp9_set_addr(reg_buf, JMDEC_DEC_BSD_CTRL_BASE_LSB, st->scratch->res->bo,
                st->filter_offset + (VP9_FILTER_SIZE / 2));

   if (dec->dec_param.pp_enabled) {
      mwv207_pp_unit_config ppu_ext_cfg[DEC_MAX_PPU_COUNT] = {0};
      uint32_t in_width = align(decoder->base.width, 16);
      uint32_t in_height = align(decoder->base.height, 16);

      mwv207_dec_pp_params_init(dec, ppu_ext_cfg, dec->dec_param.bit_depth,
                                target);
      mwv207_pp_unit_set_int_config(dec->ppu_cfg, ppu_ext_cfg,
                                    dec->dec_param.bit_depth, 0, 0);

      for (uint32_t i = 0; i < DEC_MAX_PPU_COUNT; i++) {
         if ((dec->hw_feature.pp_up_lanczos[i]
              || dec->hw_feature.pp_down_lanczos[i])
             && (dec->ppu_cfg[i].lanczos_table == NULL)) {
            uint32_t size = LANCZOS_BUFFER_SIZE * sizeof(int32_t);

            dec->ppu_cfg[i].lanczos_table = mwv207_video_buffer_create_2(
               decoder->base.context, size, PIPE_USAGE_STAGING);
         }
      }

      if (mwv207_check_pp_unit_config(&dec->hw_feature, in_width, in_height,
                                      0, dec->ppu_cfg)) {
         MWV207_LOG(MWV207_LEVEL_ERROR, "vp9: pp config check failed\n");
      }
      mwv207_calc_each_pp_buffer_size(dec->ppu_cfg, 0, 0);
      mwv207_pp_set_regs(reg_buf, &dec->hw_feature, &dec->ppu_cfg[0], 0, 0);
      reg_buf->set_reg(reg_buf, JMDEC_PP_IN_FORMAT_U, 1);
   }

   if (sh->log2_tile_columns || sh->log2_tile_rows) {
      reg_buf->set_reg(reg_buf, JMDEC_TILE_ENABLE, 1);
      reg_buf->set_reg(reg_buf, JMDEC_NUM_TILE_COLS_8K,
                       1u << sh->log2_tile_columns);
      reg_buf->set_reg(reg_buf, JMDEC_NUM_TILE_ROWS_8K,
                       1u << sh->log2_tile_rows);
   } else {
      reg_buf->set_reg(reg_buf, JMDEC_TILE_ENABLE, 0);
      reg_buf->set_reg(reg_buf, JMDEC_NUM_TILE_COLS_8K, 1);
      reg_buf->set_reg(reg_buf, JMDEC_NUM_TILE_ROWS_8K, 1);
   }

   {
      /* Multi-tile frames: the hardware walks the tile list handed over in
       * the tile table below and reads every tile straight from the original
       * bitstream, so the payload never has to be rebuilt into scratch
       * memory (the old "4-byte size in front of every tile" copy does not
       * match what the ASIC expects once the 8K tile counters are used). */
      uint32_t t_off = tile_off, t_len = tile_len;
      uint32_t a_off, start_bit, space, slen;
      struct mwv207_video_buffer *win_buf = bs_buf;
      uint32_t win_base = 0;

      a_off = t_off & ~0xfu;
      start_bit = (t_off - a_off) * 8;
      reg_buf->set_reg(reg_buf, JMDEC_STRM_START_BIT, start_bit);
      vp9_set_addr(reg_buf, JMDEC_STREAM_BASE_LSB, win_buf->res->bo,
                   win_base + a_off);
      reg_buf->set_reg(reg_buf, JMDEC_STRM_START_OFFSET, 0);

      /* The boolean decoder runs a few bytes ahead of the bit position; the
       * hardware treats STRM_STREAM_LEN as a hard limit, so handing it the
       * exact length makes it abandon the final block of the frame (the last
       * 8x8 block came out as 0).  Extend the length into the padding that
       * always follows the bitstream in the buffer. */
      space = win_buf->res->size - win_base - a_off;
      slen = t_len;
      if (slen < space) {
         uint32_t pad = 64;
         if (pad > space - slen)
            pad = space - slen;
         slen += pad;
      }
      reg_buf->set_reg(reg_buf, JMDEC_STREAM_LEN, slen);
      reg_buf->set_reg(reg_buf, JMDEC_STRM_BUFFER_LEN, space);
   }

   reg_buf->set_reg(reg_buf, JMDEC_DEC_E, 1);
   st->pending = buf;
   st->pending_flags = sh->refresh_frame_flags;
   st->pending_luma = vp9_luma_size;
}

void
mwv207_dec_vp9_submit_done(struct mwv207_video_decoder *decoder)
{
   struct mwv207_vp9_state *st = decoder->decoder->vp9_state;
   if (!st || !st->pending)
      return;
   vp9_adapt_probs(st, &st->sh);
   st->prev_frame_key = st->sh.key_frame;
   vp9_refresh_slots(decoder, st, st->pending_flags, st->pending,
                     st->ref_frame_bytes, st->pending_luma);
   st->pending = NULL;
}

void
mwv207_dec_vp9_destroy_state(void *state)
{
   vp9_destroy_state((struct mwv207_vp9_state *)state);
}
