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
#include "vl/vl_video_buffer.h"
void mwv207_vp9_reset_probs(struct mwv207_vp9_slice_header *sh);
#include "mwv207_video_dec.h"
#include "mwv207_video_dec_cfg.h"
#include "mwv207_decoder_vp9_slice_header.h"
#include "vl/vl_video_buffer.h"
#include "mwv207_utils.h"
#include "mwv207_context.h"

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

static int
vp9_trace(void)
{
   static int enabled = -1;
   if (enabled < 0)
      enabled = getenv("MWV207_VP9_DBG") ? 1 : 0;
   return enabled;
}

static inline uint32_t
vp9_align4k(uint32_t v)
{
   return (v + 0xfffu) & ~0xfffu;
}

struct mwv207_vp9_state {
   struct mwv207_vp9_slice_header sh;
   /* VP9 carries four coefficient-probability contexts across frames. */
   struct mwv207_vp9_entropy_probs saved_ctx[NUM_FRAME_CONTEXTS];
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
   /* VP9 reference frame slots: these point at the pipe_video_buffer of the
    * frame that last refreshed this slot.  The VA client keeps decoded
    * frames alive while they are referenced, so no extra reference is
    * taken here. */
   struct mwv207_video_buffer *ref_slots[16];
   struct mwv207_video_buffer *ref_store;
   struct mwv207_video_buffer *pending;
   uint32_t pending_flags, pending_luma;
   uint32_t ref_buf_size, ref_luma;
   uint32_t width;
   uint32_t height;
   /* Tile payload of the current frame: the hardware walks the tiles by
    * itself once every one of them carries a 4-byte size prefix. */
   uint32_t tile_n;
   uint32_t tile_off[16];
   uint32_t tile_len[16];
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

static void
vp9_write_ref_regs(struct mwv207_vp9_state *st,
                   struct mwv207_video_reg_buf *reg_buf,
                   struct mwv207_vp9_slice_header *sh, uint32_t luma_size,
                   uint32_t y_stride)
{
   uint32_t k;

   if (sh->key_frame || sh->intra_only)
      return;
   if (!st->ref_store || !st->ref_store->res)
      return;
   for (k = 0; k < 3; k++) {
      uint32_t slot = sh->active_ref_idx[k];
      struct mwv207_bo *bo;
      uint64_t off = (uint64_t)slot * (luma_size * 3 / 2);

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
      if (!getenv("MWV207_VP9_NOTY")) {
         reg_buf->set_addr_reg_hi(reg_buf, vp9_ref_tybase_msb[k], bo, off,
                                  MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg(reg_buf, vp9_ref_tybase_lsb[k], bo, off,
                               MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg_hi(reg_buf, vp9_ref_tcbase_msb[k], bo,
                                  off + luma_size, MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg(reg_buf, vp9_ref_tcbase_lsb[k], bo,
                               off + luma_size, MWV207_BO_ACC_FLAG_WRITE);
      }
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
                  struct mwv207_video_buffer *buf, uint32_t luma_size)
{
   uint32_t i, need = luma_size * 3 / 2;

   if (vp9_trace()) fprintf(stderr, "VP9REF refresh st=%p flags=0x%x buf=%p luma=%u posize=%u\n",
           (void *)st, refresh_flags, (void *)buf, luma_size,
           buf && buf->res ? (unsigned)buf->res->size : 0u);
   if (!buf || !buf->res || !buf->res->bo)
      return;
   if (getenv("MWV207_VP9_NOREF"))
      return;
   if (st->ref_buf_size < need * 16) {
      if (st->ref_store)
         mwv207_video_buffer_destroy((struct pipe_video_buffer *)st->ref_store);
      st->ref_store = mwv207_video_buffer_create_2(
         decoder->base.context, need * 16, PIPE_USAGE_STAGING);
      st->ref_buf_size = st->ref_store ? need * 16 : 0;
      st->ref_luma = luma_size;
   }
   if (!st->ref_store || !st->ref_store->res)
      return;
   for (i = 0; i < 16; i++) {
      uint8_t *src, *dst;
      if (!(refresh_flags & (1u << i)))
         continue;
      src = decoder->ws->bo_map(decoder->ws, buf->res->bo);
      dst = decoder->ws->bo_map(decoder->ws, st->ref_store->res->bo);
      if (src && dst) {
         memcpy(dst + (size_t)i * need, src, need);
         {
            const char *rd = getenv("MWV207_VP9_REFDUMP");
            if (rd && i == 0) {
               FILE *f = fopen(rd, "ab");
               if (f) {
                  fwrite(src, 1, st->ref_luma, f);
                  fclose(f);
               }
            }
         }
      }
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
   { const char *ce = getenv("MWV207_VP9_CTRSZ");
     if (ce) ctr_size = (uint32_t)strtoul(ce, NULL, 0); }
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
      st->filter_offset = vp9_align4k(st->seg_offset + st->seg_size);
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
   { struct mwv207_vp9_state *zs = dec->vp9_state;
     if (getenv("MWV207_VP9_SCRZERO") && zs && zs->scratch_cpu)
        memset(zs->scratch_cpu, 0, zs->scratch_size); }
   uint32_t stream_len = decoder->streams_len[0];
   uint32_t width = pic_param->picture_parameter.frame_width;
   uint32_t height = pic_param->picture_parameter.frame_height;
   uint32_t bit_depth = pic_param->picture_parameter.bit_depth;
   uint32_t tile_off, tile_len, align_off, out_w_y;
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
   if (vp9_trace()) fprintf(stderr, "VP9REF set_regs st=%p w=%u h=%u target=%p\n",
           (void *)st, width, height, (void *)buf);
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
   if (vp9_trace()) fprintf(stderr, "VP9DBG va: fhdr=%u fpart=%u\n",
           pic_param->picture_parameter.frame_header_length_in_bytes,
           pic_param->picture_parameter.first_partition_size);
   {
      uint32_t hint_hdr = getenv("MWV207_VP9_NOHINT") ? 0u
         : pic_param->picture_parameter.frame_header_length_in_bytes;
      uint32_t hint_part = getenv("MWV207_VP9_NOHINT") ? 0u
         : pic_param->picture_parameter.first_partition_size;
      ret = mwv207_vp9_parse_frame(sh, stream, stream_len, width, height,
                                   st->saved_ctx, hint_hdr, hint_part);
   }
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
   align_off = tile_off & ~0xfu;
   if (!sh->log2_tile_columns && !sh->log2_tile_rows) {
      st->tile_n = 1;
      st->tile_off[0] = tile_off;
      st->tile_len[0] = tile_len;
   }
   if (vp9_trace()) fprintf(stderr, "VP9DBG hdr: len=%u tag=%u part=%u tile_off=%u tile_len=%u\n",
           stream_len, sh->frame_tag_size, sh->offset_to_dct_parts, tile_off, tile_len);

   memcpy(st->scratch_cpu + VP9_PROB_OFFSET, &sh->entropy,
          VP9_PROB_TABLE_SIZE);
   {
      const char *pb = getenv("MWV207_VP9_PROBBUMP");
      if (pb) {
         unsigned bump = strtoul(pb, NULL, 0), q;
         uint8_t *pp = (uint8_t *)st->scratch_cpu + VP9_PROB_OFFSET;
         for (q = 0; q < VP9_PROB_TABLE_SIZE; q++)
            pp[q] = (uint8_t)(pp[q] + bump);
      }
   }
   if (getenv("MWV207_VP9_ZCTR"))
      memset(st->scratch_cpu + VP9_COUNTER_OFFSET, 0, VP9_COUNTER_SIZE);
   if (vp9_trace()) fprintf(stderr, "VP9DBG scratch first: %02x %02x %02x %02x %02x %02x %02x %02x\n",
           ((uint8_t *)st->scratch_cpu)[0], ((uint8_t *)st->scratch_cpu)[1],
           ((uint8_t *)st->scratch_cpu)[2], ((uint8_t *)st->scratch_cpu)[3],
           ((uint8_t *)st->scratch_cpu)[4], ((uint8_t *)st->scratch_cpu)[5],
           ((uint8_t *)st->scratch_cpu)[6], ((uint8_t *)st->scratch_cpu)[7]);

   {
      uint32_t sc = (((width + 7) & ~7u) + 63) >> 6;
      uint32_t sr = (((height + 7) & ~7u) + 63) >> 6;
      uint32_t l2c = sh->log2_tile_columns & 3u;
      uint32_t l2r = sh->log2_tile_rows & 3u;
      uint32_t ncol = 1u << l2c;
      uint32_t nrow = 1u << l2r;
      uint32_t n = ncol * nrow;
      uint16_t *tile = (uint16_t *)((uint8_t *)st->scratch_cpu
                                    + st->tile_offset);
      uint16_t cinfo[16], rinfo[16];
      uint32_t i, j, k = 0, acc;

      if (n > 16)
         n = 16;

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
      if (n > 1 && vp9_trace()) {
         for (i = 0; i < n; i++)
            fprintf(stderr, "VP9DBG tile%u: off=%u len=%u\n",
                    i, st->tile_off[i], st->tile_len[i]);
      }
   }
   __builtin___clear_cache((char *)st->scratch_cpu,
                           (char *)st->scratch_cpu + st->scratch_size);

   if (vp9_trace()) fprintf(stderr, "VP9DBG: buf=%p res=%p bo=%p scratch=%p cpu=%p dec=%p reg_buf=%p\n",
           (void *)buf, (void *)buf->res, buf->res ? (void *)buf->res->bo : NULL,
           (void *)st->scratch, st->scratch_cpu, (void *)dec, (void *)reg_buf);
   { const char *bd=getenv("MWV207_VP9_BD"); if (bd) { sh->bit_depth=strtoul(bd,NULL,0); } }
   dec->dec_param.bit_depth = sh->bit_depth;
   { const char *bdd=getenv("MWV207_VP9_BDDEC");
     if (bdd) dec->dec_param.bit_depth = strtoul(bdd,NULL,0); }
   dec->dec_param.pp_enabled = getenv("MWV207_VP9_NOPP") ? 0 : 1;
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
       * so one luma plane is stride * (height / 4) bytes. */
      uint32_t luma_size = out_w_y * (height / 4);
      vp9_luma_size = luma_size;
      uint32_t buff_size = NEXT_MULTIPLE(luma_size * 3 / 2, 16) + (getenv("MWV207_VP9_BIGBUF") ? 0x200000 : 0);

      buf->mwv207_offset_info.luma_size = luma_size;
      if (!buf->res)
         mwv207_video_buffer_alloc_buffer(decoder->base.context, buf,
                                          buff_size, PIPE_USAGE_STAGING);
      if (!buf->res) {
         MWV207_LOG(MWV207_LEVEL_ERROR, "vp9: output buffer allocation failed\n");
         reg_buf->set_reg(reg_buf, JMDEC_DEC_E, 0);
         return;
      }
      if (getenv("MWV207_VP9_CLEAR")) {
         struct mwv207_context *c = mwv207_context(decoder->base.context);
         void *m = c->ws->bo_map(c->ws, buf->res->bo);
         if (m) memset(m, 0, buff_size);
      }
      vp9_set_addr(reg_buf, JMDEC_DEC_OUT_YBASE_LSB, buf->res->bo, 0);
      vp9_set_addr(reg_buf, JMDEC_DEC_OUT_CBASE_LSB, buf->res->bo, luma_size);
      /* The motion vectors the entropy engine decodes are written to the
       * "direct MV" area so that later blocks can use them for spatial MV
       * prediction; without it every block after the first row predicts
       * from garbage.  The vendor programs DEC_OUT_DBASE for VP9 too. */
      vp9_set_addr(reg_buf, JMDEC_DEC_OUT_DBASE_LSB,
                   st->scratch->res->bo, st->mv_offset);
      if (getenv("MWV207_VP9_TYBASE")) {
         reg_buf->set_addr_reg_hi(reg_buf, JMDEC_DEC_OUT_TYBASE_MSB, buf->res->bo, 0, MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg(reg_buf, JMDEC_DEC_OUT_TYBASE_LSB, buf->res->bo, 0, MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg_hi(reg_buf, JMDEC_DEC_OUT_TCBASE_MSB, buf->res->bo, luma_size, MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg(reg_buf, JMDEC_DEC_OUT_TCBASE_LSB, buf->res->bo, luma_size, MWV207_BO_ACC_FLAG_WRITE);
      }
   }

   reg_buf->set_reg(reg_buf, JMDEC_DEC_MODE, DEC_MODE_VP9);
   mwv207_dec_set_common_config_regs(dec);
   /* Same single-core setup the HEVC path uses; defaults differ enough to
    * leave the VP9 core waiting on a second pipe. */
   reg_buf->set_reg(reg_buf, JMDEC_HDR_SKIP_LENGTH, 0);
   reg_buf->set_reg(reg_buf, JMDEC_APF_ONE_PID, 0);
   reg_buf->set_reg(reg_buf, JMDEC_DEC_MULTICORE_E, 0);
   reg_buf->set_reg(reg_buf, JMDEC_DEC_WRITESTAT_E, 0);
   if (getenv("MWV207_VP9_SWAP")) {
      uint32_t sw = strtoul(getenv("MWV207_VP9_SWAP"), NULL, 0);
      reg_buf->set_reg(reg_buf, JMDEC_DEC_STRM_SWAP, sw);
      reg_buf->set_reg(reg_buf, JMDEC_DEC_PIC_SWAP, sw);
   }

   if (getenv("MWV207_VP9_TABSWAP")) {
      reg_buf->set_reg(reg_buf, JMDEC_DEC_TAB_SWAP, strtoul(getenv("MWV207_VP9_TABSWAP"), NULL, 0));
   }
   {
      const char *wc = getenv("MWV207_VP9_WCBS"), *hc = getenv("MWV207_VP9_HCBS");
      reg_buf->set_reg(reg_buf, JMDEC_PIC_WIDTH_IN_CBS, wc ? strtoul(wc,NULL,0) : (width + 7) >> 3);
      reg_buf->set_reg(reg_buf, JMDEC_PIC_HEIGHT_IN_CBS, hc ? strtoul(hc,NULL,0) : (height + 7) >> 3);
      reg_buf->set_reg(reg_buf, JMDEC_PIC_WIDTH_4X4, ((width + 7) >> 2) & ~1u);
      reg_buf->set_reg(reg_buf, JMDEC_PIC_HEIGHT_4X4, ((height + 7) >> 2) & ~1u);
   }
   /* VP9 superblock is 64x64, minimum coding block 8x8 (vendor uses
    * MAX_CB_SIZE=6 / MIN_CB_SIZE=3). */
   reg_buf->set_reg(reg_buf, JMDEC_MAX_CB_SIZE, 6);
   reg_buf->set_reg(reg_buf, JMDEC_MIN_CB_SIZE, 3);

   reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_DIS, 0);
   /* no DEC400 compressor: decoder must write uncompressed output */
   { const char *ec=getenv("MWV207_VP9_EC");
     reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_EC_BYPASS, ec?strtoul(ec,NULL,0):1); }
   if (!getenv("MWV207_VP9_NOSTRIDE")) {
      uint32_t sv = getenv("MWV207_VP9_STRIDE") ? strtoul(getenv("MWV207_VP9_STRIDE"), NULL, 0) : out_w_y;
      reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_Y_STRIDE, sv);
      reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_C_STRIDE, sv);
   }

   { const char *bo=getenv("MWV207_VP9_OUTBD");
     reg_buf->set_reg(reg_buf, JMDEC_BIT_DEPTH_OUT_MINUS8,
                      bo ? strtoul(bo,NULL,0) : (sh->bit_depth - 8)); }
   reg_buf->set_reg(reg_buf, JMDEC_BIT_DEPTH_Y_MINUS8, sh->bit_depth - 8);
   reg_buf->set_reg(reg_buf, JMDEC_BIT_DEPTH_C_MINUS8, sh->bit_depth - 8);
   if (vp9_trace()) fprintf(stderr, "VP9QP: base=%u ydc=%u cdc=%u cac=%u lossless=%u seg=%u mode=%u q0=%d q1=%d q2=%d\n",
           sh->qp_yac, sh->qp_ydc, sh->qp_ch_dc, sh->qp_ch_ac, sh->lossless,
           sh->segment_enabled, sh->segment_feature_mode,
           sh->segment_feature_data[0][0], sh->segment_feature_data[1][0],
           sh->segment_feature_data[2][0]);
   {
      /* The ASIC derives the block quantiser from segment 0's feature word
       * even when segmentation is disabled; 294..296 are signed per-plane
       * deltas relative to that base. */
      int32_t base = sh->lossless ? 0 : (int32_t)sh->qp_yac;
      uint32_t dy = (uint32_t)(sh->lossless ? 0 : (int32_t)sh->qp_ydc) & 0x3f;
      uint32_t dc = (uint32_t)(sh->lossless ? 0 : (int32_t)sh->qp_ch_dc) & 0x3f;
      uint32_t da = (uint32_t)(sh->lossless ? 0 : (int32_t)sh->qp_ch_ac) & 0x3f;
      if (getenv("MWV207_VP9_RAWQP")) {
         base = 0;
      } else if (getenv("MWV207_VP9_ABSQP")) {
         dy = (uint32_t)((base + (int32_t)sh->qp_ydc) & 0x3f);
         dc = (uint32_t)((base + (int32_t)sh->qp_ch_dc) & 0x3f);
         da = (uint32_t)((base + (int32_t)sh->qp_ch_ac) & 0x3f);
      }
      reg_buf->set_reg(reg_buf, JMDEC_QUANT_SEG0, (uint32_t)base & 0xff);
      reg_buf->set_reg(reg_buf, JMDEC_QP_DELTA_Y_DC, dy);
      reg_buf->set_reg(reg_buf, JMDEC_QP_DELTA_CH_DC, dc);
      reg_buf->set_reg(reg_buf, JMDEC_QP_DELTA_CH_AC, da);
      {
         const char *sf = getenv("MWV207_VP9_SEGFILT");
         reg_buf->set_reg(reg_buf, JMDEC_FILT_LEVEL_SEG0,
                          sf ? (strtoul(sf, NULL, 0) & 0x3f)
                             : (sh->loop_filter_level & 0x3f));
      }
      reg_buf->set_reg(reg_buf, JMDEC_SKIP_SEG0, 0);
      reg_buf->set_reg(reg_buf, JMDEC_REFPIC_SEG0, 0);
   }
   reg_buf->set_reg(reg_buf, JMDEC_LOSSLESS_E, sh->lossless ? 1 : 0);
   reg_buf->set_reg(reg_buf, JMDEC_IDR_PIC_E,
                    (sh->key_frame || sh->intra_only) ? 1 : 0);
   { const char *tm=getenv("MWV207_VP9_TMODE");
     reg_buf->set_reg(reg_buf, JMDEC_TRANSFORM_MODE, tm?strtoul(tm,NULL,0):(sh->transform_mode & 0x7)); }
   if (vp9_trace()) fprintf(stderr, "VP9DBG inter: txmode=%u tm_reg=%u lossless=%d hp=%d interp=%u comp=%u bd=%d\n",
           sh->transform_mode, sh->transform_mode & 7, sh->lossless,
           sh->allow_high_precision_mv, sh->mcomp_filter_type,
           sh->comp_pred_mode, (int)sh->bit_depth);
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
                    getenv("MWV207_VP9_NOLF") ? 1
                    : (sh->loop_filter_level ? 0 : 1));
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

   reg_buf->set_reg(reg_buf, JMDEC_SEGMENT_E, 0);
   reg_buf->set_reg(reg_buf, JMDEC_SEGMENT_UPD_E, 0);
   reg_buf->set_reg(reg_buf, JMDEC_SEGMENT_TEMP_UPD_E, 0);

   /* JMDEC_REFER_LTERM_E is a 32-bit field at dword 38, the same dword that
    * holds AREF_HOR/VER_SCALE; writing it must happen before the reference
    * scales or it wipes them. */
   reg_buf->set_reg(reg_buf, JMDEC_NO_FWD_REF_E, 0);
   reg_buf->set_reg(reg_buf, JMDEC_REFER_VALID_E, 1);
   reg_buf->set_reg(reg_buf, JMDEC_REFER_LTERM_E, 0);
   vp9_write_ref_regs(st, reg_buf, sh, vp9_luma_size, out_w_y);

   vp9_set_addr(reg_buf, JMDEC_PROB_TAB_BASE_LSB, st->scratch->res->bo,
                VP9_PROB_OFFSET);
   vp9_set_addr(reg_buf, JMDEC_CTX_COUNTER_BASE_LSB, st->scratch->res->bo,
                VP9_COUNTER_OFFSET);
   vp9_set_addr(reg_buf, JMDEC_SEGMENT_READ_BASE_LSB, st->scratch->res->bo,
                st->seg_offset);
   vp9_set_addr(reg_buf, JMDEC_SEGMENT_WRITE_BASE_LSB, st->scratch->res->bo,
                st->seg_offset + st->seg_size);
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
      { const char *sh=getenv("MWV207_PP_SHAPER");
        if (sh) ppu_ext_cfg[0].shaper_enabled = strtoul(sh,NULL,0); }
      { const char *ry=getenv("MWV207_PP_RANGEMAP");
        if (ry) {
           unsigned v=strtoul(ry,NULL,0);
           reg_buf->set_reg(reg_buf, JMDEC_RANGEMAP_Y_E_U, (v>>8)&1);
           reg_buf->set_reg(reg_buf, JMDEC_RANGEMAP_COEF_Y_U, v & 0x1f);
        } }
      {
         const char *ru=getenv("MWV207_PP_RANGEU");
         if (ru) reg_buf->set_reg(reg_buf, JMDEC_YCBCR_RANGE_U,
                                  strtoul(ru,NULL,0));
      }
      mwv207_pp_set_regs(reg_buf, &dec->hw_feature, &dec->ppu_cfg[0], 0, 0);
      {
         const char *rg = getenv("MWV207_PP_RANGE");
         if (rg) reg_buf->set_reg(reg_buf, JMDEC_YCBCR_RANGE_U, strtoul(rg,NULL,0));
      }
      { const char *pi=getenv("MWV207_PP_INFMT");
      reg_buf->set_reg(reg_buf, JMDEC_PP_IN_FORMAT_U, pi?strtoul(pi,NULL,0):1); }
   }

   if (vp9_trace()) fprintf(stderr, "VP9DBG tiles: l2c=%u l2r=%u txmode=%u\n",
           sh->log2_tile_columns, sh->log2_tile_rows, sh->transform_mode);
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

   if (vp9_trace()) fprintf(stderr, "VP9DBG addr: out_bo=%p stream_bo=%p scratch_bo=%p tile_off=%u len=%u\n",
           (void *)buf->res->bo, (void *)bs_buf->res->bo,
           (void *)st->scratch->res->bo, tile_off, tile_len);
   if (vp9_trace()) {
      struct mwv207_context *t_ctx = mwv207_context(decoder->base.context);
      uint8_t *tp = t_ctx->ws->bo_map(t_ctx->ws, bs_buf->res->bo);
      uint32_t t1 = (uint32_t)tp[tile_off] << 24 | (uint32_t)tp[tile_off + 1] << 16 |
                    (uint32_t)tp[tile_off + 2] << 8 | tp[tile_off + 3];
      fprintf(stderr, "VP9DBG tilesz: tile0=%u off=%u next=%u\n",
              t1, tile_off, tile_off + 4 + t1);
   }
   if (vp9_trace()) {
      struct mwv207_context *dbg_ctx = mwv207_context(decoder->base.context);
      uint8_t *dp = dbg_ctx->ws->bo_map(dbg_ctx->ws, bs_buf->res->bo);
      fprintf(stderr, "VP9DBG stream bo size=%u off=%u data:", bs_buf->res->size, align_off);
      for (int i = 0; i < 32; i++) fprintf(stderr, " %02x", dp[i]);
      fprintf(stderr, " | bs_size=%u streams_len0=%u nstreams=%u\n", decoder->bs_size, decoder->streams_len[0], decoder->streams_num);
      fprintf(stderr, "VP9DBG streams0 len=%u data:", decoder->streams_len[0]);
      for (unsigned i = 0; i < (decoder->streams_len[0] < 32 ? decoder->streams_len[0] : 32); i++)
         fprintf(stderr, " %02x", ((uint8_t *)decoder->streams[0])[i]);
      fprintf(stderr, "\n");
   }
   if (getenv("MWV207_VP9_STREAM_FULL")) {
      reg_buf->set_reg(reg_buf, JMDEC_STRM_START_BIT, 0);
      vp9_set_addr(reg_buf, JMDEC_STREAM_BASE_LSB, bs_buf->res->bo, 0);
      reg_buf->set_reg(reg_buf, JMDEC_STRM_START_OFFSET, 0);
      reg_buf->set_reg(reg_buf, JMDEC_STREAM_LEN, stream_len);
      reg_buf->set_reg(reg_buf, JMDEC_STRM_BUFFER_LEN, bs_buf->res->size);
   } else {
      /* Multi-tile frames: the hardware walks the tile list handed over in
       * the tile table below and reads every tile from the original bitstream,
       * so no payload rebuild is needed.  MWV207_VP9_COPY=1 re-enables the old
       * "add a 4-byte size in front of every tile" copy for experiments, which
       * does not match what the ASIC expects with the 8K tile counters. */
      uint32_t t_off = tile_off, t_len = tile_len;
      uint32_t a_off, start_bit, space, slen;
      struct mwv207_video_buffer *win_buf = bs_buf;
      uint32_t win_base = 0;

      if (st->tile_n > 1 && tile_off < VP9_STREAM_COPY_SIZE &&
          getenv("MWV207_VP9_COPY")) {
         uint8_t *dst = (uint8_t *)st->scratch_cpu + st->stream_offset;
         uint32_t i, total;

         memcpy(dst, stream, tile_off);
         total = tile_off;
         for (i = 0; i < st->tile_n; i++) {
            uint32_t ln = st->tile_len[i];

            if (total + 4 + ln > VP9_STREAM_COPY_SIZE) {
               total = 0;
               break;
            }
            dst[total + 0] = (uint8_t)(ln >> 24);
            dst[total + 1] = (uint8_t)(ln >> 16);
            dst[total + 2] = (uint8_t)(ln >> 8);
            dst[total + 3] = (uint8_t)ln;
            memcpy(dst + total + 4, stream + st->tile_off[i], ln);
            total += 4 + ln;
         }
         if (total) {
            __builtin___clear_cache((char *)dst, (char *)dst + total);
            win_buf = st->scratch;
            win_base = st->stream_offset;
            t_off = tile_off;
            t_len = total - tile_off;
            if (vp9_trace())
               fprintf(stderr, "VP9DBG tiles copy: %u bytes, window=%u\n",
                       total, t_len);
         }
      }

      a_off = t_off & ~0xfu;
      start_bit = (t_off - a_off) * 8;
      {
         const char *sb = getenv("MWV207_VP9_SB");
         const char *so = getenv("MWV207_VP9_SOFF");
         if (sb && so) {
            start_bit = strtoul(sb, NULL, 0);
            a_off = strtoul(so, NULL, 0);
         }
      }
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
      {
         const char *so_slen = getenv("MWV207_VP9_SLEN");
         const char *so_add = getenv("MWV207_VP9_SLENADD");
         if (so_slen)
            slen = strtoul(so_slen, NULL, 0);
         else if (so_add)
            slen += (uint32_t)strtoul(so_add, NULL, 0);
      }
      reg_buf->set_reg(reg_buf, JMDEC_STREAM_LEN, slen);
      reg_buf->set_reg(reg_buf, JMDEC_STRM_BUFFER_LEN, space);
   }

   {
      const char *ot = getenv("MWV207_VP9_OUTTILE");
      if (ot) reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_TILED_E, strtoul(ot,NULL,0));
   }
   { const char *pp = getenv("MWV207_VP9_DUMPS_PRE");
     if (pp && st->scratch_cpu) {
        FILE *f = fopen(pp, "wb");
        if (f) { fwrite(st->scratch_cpu, 1, st->scratch_size, f); fclose(f); }
     } }
   { const char *rd = getenv("MWV207_VP9_REGD");
     if (rd) {
        FILE *f = fopen(rd, "a");
        if (f) {
           uint32_t i;
           fprintf(f, "=== FRAME key=%d\n", sh->key_frame);
           for (i = 0; i < JMDEC_LAST_REG; i++)
              fprintf(f, "REG %u %u\n", i, reg_buf->get_reg(reg_buf, i));
           fclose(f);
        }
     } }
   { const char *rs = getenv("MWV207_VP9_REGSET");
     if (rs) {
        char buf[512];
        char *tok, *save;
        strncpy(buf, rs, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;
        for (tok = strtok_r(buf, ",", &save); tok;
             tok = strtok_r(NULL, ",", &save)) {
           char *eq = strchr(tok, '=');
           if (!eq) continue;
           *eq = 0;
           reg_buf->set_reg(reg_buf, (uint32_t)strtoul(tok, NULL, 0),
                            (uint32_t)strtoul(eq + 1, NULL, 0));
        }
     }
   }
   reg_buf->set_reg(reg_buf, JMDEC_DEC_E, 1);
   st->pending = buf;
   st->pending_flags = sh->refresh_frame_flags;
   st->pending_luma = vp9_luma_size;
   if (vp9_trace()) fprintf(stderr, "VP9REF key=%u intra=%u refidx=%u,%u,%u refresh=0x%x\n",
           sh->key_frame, sh->intra_only, sh->active_ref_idx[0],
           sh->active_ref_idx[1], sh->active_ref_idx[2],
           sh->refresh_frame_flags);
}

void
mwv207_dec_vp9_submit_done(struct mwv207_video_decoder *decoder)
{
   struct mwv207_vp9_state *st = decoder->decoder->vp9_state;
   if (!st || !st->pending)
      return;
   vp9_refresh_slots(decoder, st, st->pending_flags, st->pending,
                     st->pending_luma);
   {
      const char *mp = getenv("MWV207_VP9_MVDUMP");
      if (mp && st->scratch_cpu && st->mv_size) {
         FILE *f = fopen(mp, "ab");
         if (f) {
            fwrite((uint8_t *)st->scratch_cpu + st->mv_offset, 1,
                   st->mv_size, f);
            fclose(f);
         }
      }
   }
   if (st->ref_store && st->ref_store->res) {
      uint8_t *q = decoder->ws->bo_map(decoder->ws, st->ref_store->res->bo);
      if (q) {
         uint32_t sl;
         for (sl = 0; sl < 16; sl++)
            if (st->pending_flags & (1u << sl))
               if (vp9_trace()) fprintf(stderr, "VP9REF slot%d first=%02x %02x %02x %02x\n", sl,
                       q[(size_t)sl * st->pending_luma * 3 / 2],
                       q[(size_t)sl * st->pending_luma * 3 / 2 + 1],
                       q[(size_t)sl * st->pending_luma * 3 / 2 + 2],
                       q[(size_t)sl * st->pending_luma * 3 / 2 + 3]);
      }
   }
   {
      const char *rp = getenv("MWV207_VP9_REFDUMP");
      if (rp && st->ref_store && st->ref_store->res) {
         uint8_t *q = decoder->ws->bo_map(decoder->ws, st->ref_store->res->bo);
         if (q) {
            FILE *f = fopen(rp, "ab");
            if (f) {
               fwrite(q, 1, (size_t)st->pending_luma * 3 / 2, f);
               fclose(f);
            }
         }
      }
   }
   {
      const char *dp = getenv("MWV207_VP9_DUMP");
      if (dp && st->pending->res && st->pending->res->bo) {
         uint8_t *p = decoder->ws->bo_map(decoder->ws, st->pending->res->bo);
         if (p) {
            FILE *f = fopen(dp, "ab");
            if (f) {
               fwrite(p, 1, (size_t)st->pending_luma * 3 / 2, f);
               fclose(f);
            }
         }
      }
   }
   {
      const char *dp = getenv("MWV207_VP9_DUMPS");
      if (dp && st->scratch_cpu) {
         FILE *f = fopen(dp, "wb");
         if (f) {
            fwrite(st->scratch_cpu, 1, st->scratch_size, f);
            fclose(f);
            if (vp9_trace()) fprintf(stderr, "VP9DBG scratch dumped size=%u\n", st->scratch_size);
         }
      }
   }
   st->pending = NULL;
}

void
mwv207_dec_vp9_destroy_state(void *state)
{
   vp9_destroy_state((struct mwv207_vp9_state *)state);
}
