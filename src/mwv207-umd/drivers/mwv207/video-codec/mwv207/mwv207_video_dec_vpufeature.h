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

#ifndef MWV207_9200_VDIEO_DEC_VPU_FEATURES_H
#define MWV207_9200_VDIEO_DEC_VPU_FEATURES_H

#include <stdint.h>

enum mwv207_vpu_release_ver {
   G1_6_0,
   G1_8_1,
   G1_8_2,
   G2_4_1,
   G2_4_2,
   G2_8_1,
   JMDEC_6_1_CR0
};
enum {
   PP_OUT_FMT_YUV420PACKED = 0,
   PP_OUT_FMT_YUV420_P010 = 1,
   PP_OUT_FMT_YUV420_BIGE = 2,
   PP_OUT_FMT_YUV420_8BIT = 3,
   PP_OUT_FMT_YUV400 = 4,
   PP_OUT_FMT_YUV400_P010 = 5,
   PP_OUT_FMT_YUV400_8BIT = 6,
   PP_OUT_FMT_IYUV = 7,
   PP_OUT_FMT_IYUV_P010 = 8,
   PP_OUT_FMT_IYUV_8BIT = 9,
   PP_OUT_FMT_YUV420_10 = 10,
};
#define DEC_MAX_PPU_COUNT 5

struct mwv207_video_dec_hw_features {
   uint32_t id;
   uint32_t id_mask;

   uint32_t hevc_support;
   uint32_t hevc_main10_support;
   uint32_t vp9_support;
   uint32_t vp9_profile2_support;

   uint32_t h264_support;
   uint32_t h264_high10_support;
   uint32_t mvc_support;
   uint32_t mpeg4_support;
   uint32_t custom_mpeg4_support;
   uint32_t mpeg2_support;
   uint32_t sorenson_spark_support;
   uint32_t vc1_support;
   uint32_t jpeg_support;
   uint32_t jpeg_esupport;
   uint32_t jpeg_prog_support;
   uint32_t rv_support;
   uint32_t vp8_support;
   uint32_t webp_support;
   uint32_t vp7_support;
   uint32_t vp6_support;
   uint32_t avs_support;
   uint32_t avs_plus_support;
   uint32_t avs2_support;
   uint32_t avs2_main10_support;

   uint32_t hevc_max_dec_pic_width;
   uint32_t hevc_max_dec_pic_height;
   uint32_t vp9_max_dec_pic_width;
   uint32_t vp9_max_dec_pic_height;
   uint32_t h264_max_dec_pic_width;
   uint32_t h264_max_dec_pic_height;
   uint32_t mpeg4_max_dec_pic_width;
   uint32_t mpeg4_max_dec_pic_height;
   uint32_t mpeg2_max_dec_pic_width;
   uint32_t mpeg2_max_dec_pic_height;
   uint32_t sspark_max_dec_pic_width;
   uint32_t sspark_max_dec_pic_height;
   uint32_t vc1_max_dec_pic_width;
   uint32_t vc1_max_dec_pic_height;
   uint32_t rv_max_dec_pic_width;
   uint32_t rv_max_dec_pic_height;
   uint32_t vp8_max_dec_pic_width;
   uint32_t vp8_max_dec_pic_height;
   uint32_t vp7_max_dec_pic_width;
   uint32_t vp7_max_dec_pic_height;
   uint32_t vp6_max_dec_pic_width;
   uint32_t vp6_max_dec_pic_height;
   uint32_t avs_max_dec_pic_width;
   uint32_t avs_max_dec_pic_height;
   uint32_t divx_max_dec_pic_width;
   uint32_t divx_max_dec_pic_height;
   uint32_t img_max_dec_width;
   uint32_t img_max_dec_height;

   uint32_t g1_strm_128bit_align;
   uint32_t addr64_support;
   uint32_t ref_buf_support;
   uint32_t tiled_mode_support;
   uint32_t ref_frame_tiled_only;
   uint32_t ec_support;
   uint32_t dec_stride_support;
   uint32_t pp_stride_support;
   uint32_t field_dpb_support;
   uint32_t double_buffer_support;
   uint32_t rfc_support;
   uint32_t rfc_stride_support;
   uint32_t ring_buffer_support;
   uint32_t mrb_prefetch;
   uint32_t statistical_level;

   uint32_t h264_slice_ec_support;
   uint32_t tile_decoding_support;
   uint32_t jpeg_slice_decoding_support;
   uint32_t low_latency_mode_support;
   uint32_t has_2nd_pipeline;
   uint32_t has_2nd_h264_pipeline;
   uint32_t has_2nd_jpeg_pipeline;
   uint32_t partial_decoding_support;

   uint32_t is_legacy_dec_mode;
   uint32_t strm_len_32bits;
   uint32_t pic_size_reg_unified;

   uint32_t max_ppu_count;
   uint32_t pp_support;
   enum {
      NO_PP = 0,
      G1_NATIVE_PP,
      G2_NATIVE_PP,
      FIXED_DS_PP,
      UNFIED_PP
   } pp_version;
   uint32_t max_pp_out_pic_width[DEC_MAX_PPU_COUNT];
   uint32_t max_pp_out_pic_height[DEC_MAX_PPU_COUNT];
   /* down scaling is also supported when pp input size (i.e., cropping
    * output) exceeds pp max output size */
   uint32_t pp_dscaling_exceeding_max_size;
   uint32_t jpeg_pp_force_e;
   uint32_t dscale_support[DEC_MAX_PPU_COUNT];
   uint32_t uscale_support[DEC_MAX_PPU_COUNT];
   uint32_t flexible_scale_support;
   uint32_t crop_support;
   uint32_t crop_step_unit;
   uint32_t crop_step_rshift;
   uint32_t rotation_support;
   uint32_t dither_support;

   uint32_t fmt_tile_support[DEC_MAX_PPU_COUNT];
   uint32_t fmt_p010_support;
   uint32_t fmt_customer1_support;
   uint32_t fmt_rgb_support;
   uint32_t pp_pad_e;
   uint32_t pp_planar_support;
   uint32_t pp_yuv420_101010_support;
   uint32_t pp_dscale_with_less_linebuf;
   uint32_t pp_p010_msb;
   uint32_t pp_delogo;
   uint32_t second_crop_support;
   uint32_t pp_up_lanczos[DEC_MAX_PPU_COUNT];
   uint32_t pp_down_lanczos[DEC_MAX_PPU_COUNT];
   uint32_t rgb_line_stride_support;
   uint32_t support_less_bit_rgb;
   uint32_t uscale_support_big_3[DEC_MAX_PPU_COUNT];
};

void mwv207_video_dec_get_hw_features_by_id(
   uint32_t hw_build_id, struct mwv207_video_dec_hw_features *hw_feature);

#endif

