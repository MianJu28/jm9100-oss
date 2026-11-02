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

#include "mwv207_dec_h264.h"
#include "mwv207_dec_hevc.h"
#include "mwv207_video_decoder.h"
#include "util/u_memory.h"

void
mwv207_dec_set_common_config_regs(struct mwv207_video_dec *dec)
{
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;

   reg_buf->set_reg(reg_buf, JMDEC_DEC_STRM_SWAP, MWV207DEC_STREAM_SWAP);
   reg_buf->set_reg(reg_buf, JMDEC_DEC_PIC_SWAP, MWV207DEC_STREAM_SWAP);
   reg_buf->set_reg(reg_buf, JMDEC_DEC_DIRMV_SWAP, MWV207DEC_STREAM_SWAP);

   if (reg_buf->get_reg(reg_buf, JMDEC_DEC_MODE) == DEC_MODE_HEVC
       || reg_buf->get_reg(reg_buf, JMDEC_DEC_MODE) == DEC_MODE_VP9) {
      reg_buf->set_reg(reg_buf, JMDEC_DEC_TAB_SWAP, 0);
   } else {
      reg_buf->set_reg(reg_buf,
                       JMDEC_DEC_TAB_SWAP,
                       MWV207DEC_STREAM_SWAP_8 | MWV207DEC_STREAM_SWAP_16);
   }

   reg_buf->set_reg(reg_buf, JMDEC_DEC_BUSWIDTH, DEC_X170_BUS_WIDTH);
   reg_buf->set_reg(reg_buf, JMDEC_DEC_MAX_BURST, DEC_X170_BUS_BURST_LENGTH);

   
   
   
   reg_buf->set_reg(reg_buf, JMDEC_DEC_REFER_DOUBLEBUFFER_E, 0);
   

#if (JMDEC_APF_SINGLE_PU_MODE)
   reg_buf->set_reg(reg_buf, JMDEC_APF_SINGLE_PU_MODE, 1);
   reg_buf->set_reg(reg_buf, JMDEC_APF_DISABLE, 1);
   reg_buf->set_reg(reg_buf, JMDEC_APF_THRESHOLD, 0);
#elif (DEC_X170_APF_DISABLE)
   reg_buf->set_reg(reg_buf, JMDEC_APF_SINGLE_PU_MODE, 0);
   reg_buf->set_reg(reg_buf, JMDEC_APF_DISABLE, 1);
   reg_buf->set_reg(reg_buf, JMDEC_APF_THRESHOLD, 0);
#else
   {
      uint32_t apf_tmp_threshold = DEC_X170_REFBU_NONSEQ / DEC_X170_REFBU_SEQ;
      reg_buf->set_reg(reg_buf, JMDEC_APF_DISABLE, DEC_X170_APF_DISABLE);
      if (apf_tmp_threshold > 63)
         apf_tmp_threshold = 63;
      reg_buf->set_reg(reg_buf, JMDEC_APF_THRESHOLD, apf_tmp_threshold);
   }
#endif

#if (DEC_X170_USING_IRQ == 0)
   reg_buf->set_reg(reg_buf, JMDEC_DEC_IRQ_DIS, 1);
#else
   reg_buf->set_reg(reg_buf, JMDEC_DEC_IRQ_DIS, 0);
#endif

   reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_EC_BYPASS, 0);
   reg_buf->set_reg(reg_buf, JMDEC_APF_ONE_PID, 0);

   reg_buf->set_reg(
      reg_buf, JMDEC_DEC_CLK_GATE_E, DEC_X170_INTERNAL_CLOCK_GATING);

   reg_buf->set_reg(
      reg_buf, JMDEC_DEC_AXI_RD_ID_E, (DEC_X170_AXI_ID_R_E & 0x1));
   reg_buf->set_reg(
      reg_buf, JMDEC_DEC_AXI_WD_ID_E, (DEC_X170_AXI_ID_R_E & 0x1));
   reg_buf->set_reg(
      reg_buf, JMDEC_DEC_AXI_RD_ID, (DEC_X170_AXI_ID_R & 0xFFU));
   reg_buf->set_reg(
      reg_buf, JMDEC_DEC_AXI_WR_ID, (DEC_X170_AXI_ID_W & 0xFFU));

   reg_buf->set_reg(reg_buf,
                    JMDEC_EXT_TIMEOUT_OVERRIDE_E,
                    MWV207DEC_TIMEOUT_OVERRIDE ? 1 : 0);
   reg_buf->set_reg(
      reg_buf, JMDEC_EXT_TIMEOUT_CYCLES, MWV207DEC_TIMEOUT_OVERRIDE);

   reg_buf->set_reg(
      reg_buf, JMDEC_TIMEOUT_OVERRIDE_E, MWV207DEC_TIMEOUT_OVERRIDE ? 1 : 0);
   reg_buf->set_reg(reg_buf, JMDEC_TIMEOUT_CYCLES, MWV207DEC_TIMEOUT_OVERRIDE);
   reg_buf->set_reg(reg_buf,
                    JMDEC_AXI_RD_OSTD_THRESHOLD,
                    DEC_X170_MAX_WR_OUTSTANDING_THRESHOLD);
   reg_buf->set_reg(reg_buf,
                    JMDEC_AXI_WR_OSTD_THRESHOLD,
                    DEC_X170_MAX_WR_OUTSTANDING_THRESHOLD);
}

static void
avc_init_ref(struct mwv207_video_dec *dec)
{
   int i = 0;
   for (i = 0; i < 16; i++) {
      dec->ref_frames[i].picture = NULL;
      dec->ref_frames[i].frame_num = 0;
      dec->ref_frames[i].is_long_term = false;
      dec->ref_frames[i].top_is_reference = false;
      dec->ref_frames[i].bottom_is_reference = false;
      dec->ref_frames[i].top_field_order_cnt = 0;
      dec->ref_frames[i].bottom_field_order_cnt = 0;
   }
}

struct mwv207_video_dec *
mwv207_video_dec_create(struct mwv207_winsys *ws)
{
   struct mwv207_video_dec *dec = CALLOC_STRUCT(mwv207_video_dec);
   if (!dec)
      return NULL;

   dec->reg_buf = mwv207_video_reg_buf_create(ws);
   avc_init_ref(dec);
   dec->poc.prev_poc_msb = (1 << 16);
   dec->poc.prev_frame_num = -1;
   return dec;
}

void
mwv207_video_dec_destroy_buffer(struct mwv207_video_dec *dec)
{
   if (dec) {
      
      
      //       (struct pipe_video_buffer *)dec->cabac_buffer);
      
      
      
      if (dec->tile_edge) {
         mwv207_video_buffer_destroy(
            (struct pipe_video_buffer *)dec->tile_edge);
         dec->tile_edge = NULL;
      }
      if (dec->misc_buffer) {
         mwv207_video_buffer_destroy(
            (struct pipe_video_buffer *)dec->misc_buffer);
         dec->misc_buffer = NULL;
         dec->misc_buf = NULL;
      }
   }
}

void
mwv207_video_dec_destroy(struct mwv207_video_dec *dec)
{
   if (dec) {
      mwv207_video_dec_destroy_buffer(dec);
      mwv207_video_reg_buf_destroy(dec->reg_buf);
      dec->reg_buf = NULL;
   }
   FREE(dec);
}

void
mwv207_9200_video_dec_picture(struct mwv207_video_decoder *decoder,
                             struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;

   if (decoder->video_format == MWV207_VIDEO_FORMAT_MPEG4_AVC) {
      mwv207_dec_h264_set_regs(decoder, target);
   } else if (decoder->video_format == MWV207_VIDEO_FORMAT_HEVC) {
      mwv207_dec_hevc_set_regs(decoder, target);
   }
   reg_buf->bos_sort(reg_buf);
}

int
mwv207_dec_pp_params_init(struct mwv207_video_dec *dec,
                         mwv207_pp_unit_config *ppu_ext_cfg,
                         uint32_t bit_depth,
                         struct pipe_video_buffer *target)
{
   uint32_t has_shaper = 0;
   uint32_t pp_extern_buffer_nums = 0;
   ppu_ext_cfg->enabled =
      1; 
   ppu_ext_cfg->planar = 0;
   ppu_ext_cfg->tiled_e = 0;
   ppu_ext_cfg->cr_first = 0;
   ppu_ext_cfg->shaper_enabled = 1;
   ppu_ext_cfg->align = DEC_ALIGN_64B;

   if (has_shaper) {
      ppu_ext_cfg->shaper_enabled = 1;
      ppu_ext_cfg->align = DEC_ALIGN_256B;
      MWV207_LOG(MWV207_LEVEL_INFO, "has shaper, set pp align 256B \n");
   }

   if (bit_depth > 8) {
      /* FIX(jm9100-oss): 原实现位深 >8 时无条件 out_p010=1, 却不看
       * outsurface 实际是什么 buffer——target 为 NV12 时 PPU 会按
       * P010(pixel_width=16, 双倍 stride)写入 8bit buffer, 越界/错位。
       * 现在按目标 buffer 的实际格式分流:
       *  - P010/P016 目标: 维持 out_p010=1(硬件 fmt_p010_support=1);
       *  - 8bit 目标(NV12): 走 PPU 硬件降位输出 out_cut_8bits=1
       *    (mwv207_pp_unit_set_int_config: out_format=
       *     PP_OUT_FMT_YUV420_8BIT, 输出 pixel_width=8, NV12 半平面),
       *    解码核心仍按 10bit 输出到 staging, 仅输出级降到 8bit。
       * 配套 is_video_format_supported 的 AND 语义修复后, ffmpeg/mpv
       * 对 10bit 流会协商出 NV12 surface, 走到 cut_8bits 分支。 */
      if (target &&
          (target->buffer_format == PIPE_FORMAT_P010 ||
           target->buffer_format == PIPE_FORMAT_P016)) {
         ppu_ext_cfg->out_p010 = 1;
      } else {
         ppu_ext_cfg->out_cut_8bits = 1;
      }
   }
   ppu_ext_cfg->outsurface = target;
   

   return (pp_extern_buffer_nums > 0) ? 1 : 0;
}

