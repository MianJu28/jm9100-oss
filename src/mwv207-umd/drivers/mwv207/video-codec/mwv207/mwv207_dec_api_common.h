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

#ifndef MWV207_9200_DEC_API_COMMON_H
#define MWV207_9200_DEC_API_COMMON_H

#include <stdint.h>
#include "pipe/p_video_codec.h"
/** Maximum number of cores supported in multi-core configuration */
/** For G2, multi-core currently not supported. */
#define MAX_NODE 4
#define MAX_CORE 4
#define MAX_ASIC_CORES MAX_CORE
#define MAX_ASIC_CORES_X2 (MAX_CORE << 1)
#define MAX_MEM_NUMS (MAX_CORE << 1)

#define HEVC_NOT_SUPPORTED (uint32_t)(0x00)
#define HEVC_MAIN_PROFILE (uint32_t)(0x01)
#define HEVC_MAIN10_PROFILE (uint32_t)(0x02)
#define HEVC_SUPPORTED (uint32_t)(0x01)
#define VP9_NOT_SUPPORTED (uint32_t)(0x00)
#define VP9_PROFILE0 (uint32_t)(0x01)
#define VP9_PROFILE2_10BITS (uint32_t)(0x02)
#define MPEG4_NOT_SUPPORTED (uint32_t)(0x00)
#define MPEG4_SIMPLE_PROFILE (uint32_t)(0x01)
#define MPEG4_ADVANCED_SIMPLE_PROFILE (uint32_t)(0x02)
#define MPEG4_CUSTOM_NOT_SUPPORTED (uint32_t)(0x00)
#define MPEG4_CUSTOM_FEATURE_1 (uint32_t)(0x01)
#define H264_NOT_SUPPORTED (uint32_t)(0x00)
#define H264_BASELINE_PROFILE (uint32_t)(0x01)
#define H264_MAIN_PROFILE (uint32_t)(0x02)
#define H264_HIGH_PROFILE (uint32_t)(0x03)
#define VC1_NOT_SUPPORTED (uint32_t)(0x00)
#define VC1_SIMPLE_PROFILE (uint32_t)(0x01)
#define VC1_MAIN_PROFILE (uint32_t)(0x02)
#define VC1_ADVANCED_PROFILE (uint32_t)(0x03)
#define MPEG2_NOT_SUPPORTED (uint32_t)(0x00)
#define MPEG2_MAIN_PROFILE (uint32_t)(0x01)
#define JPEG_NOT_SUPPORTED (uint32_t)(0x00)
#define JPEG_BASELINE (uint32_t)(0x01)
#define JPEG_PROGRESSIVE (uint32_t)(0x02)
#define PP_NOT_SUPPORTED (uint32_t)(0x00)
#define PP_SUPPORTED (uint32_t)(0x01)
#define PP_TILED_4X4 (uint32_t)(0x20000000)
#define PP_DITHERING (uint32_t)(0x10000000)
#define PP_SCALING (uint32_t)(0x0C000000)
#define PP_DEINTERLACING (uint32_t)(0x02000000)
#define PP_ALPHA_BLENDING (uint32_t)(0x01000000)
#define PP_OUTP_ENDIAN (uint32_t)(0x00040000)
#define PP_TILED_INPUT (uint32_t)(0x0000C000)
#define PP_PIX_ACC_OUTPUT (uint32_t)(0x40000000)
#define PP_ABLEND_CROP (uint32_t)(0x80000000)
#define SORENSON_SPARK_NOT_SUPPORTED (uint32_t)(0x00)
#define SORENSON_SPARK_SUPPORTED (uint32_t)(0x01)
#define VP6_NOT_SUPPORTED (uint32_t)(0x00)
#define VP6_SUPPORTED (uint32_t)(0x01)
#define VP7_NOT_SUPPORTED (uint32_t)(0x00)
#define VP7_SUPPORTED (uint32_t)(0x01)
#define VP8_NOT_SUPPORTED (uint32_t)(0x00)
#define VP8_SUPPORTED (uint32_t)(0x01)
#define REF_BUF_NOT_SUPPORTED (uint32_t)(0x00)
#define REF_BUF_SUPPORTED (uint32_t)(0x01)
#define REF_BUF_INTERLACED (uint32_t)(0x02)
#define REF_BUF_DOUBLE (uint32_t)(0x04)
#define TILED_NOT_SUPPORTED (uint32_t)(0x00)
#define TILED_8x4_SUPPORTED (uint32_t)(0x01)
#define AVS_NOT_SUPPORTED (uint32_t)(0x00)
#define AVS_SUPPORTED (uint32_t)(0x01)
#define JPEG_EXT_NOT_SUPPORTED (uint32_t)(0x00)
#define JPEG_EXT_SUPPORTED (uint32_t)(0x01)
#define RV_NOT_SUPPORTED (uint32_t)(0x00)
#define RV_SUPPORTED (uint32_t)(0x01)
#define MVC_NOT_SUPPORTED (uint32_t)(0x00)
#define MVC_SUPPORTED (uint32_t)(0x01)
#define WEBP_NOT_SUPPORTED (uint32_t)(0x00)
#define WEBP_SUPPORTED (uint32_t)(0x01)
#define EC_NOT_SUPPORTED (uint32_t)(0x00)
#define EC_SUPPORTED (uint32_t)(0x01)
#define STRIDE_NOT_SUPPORTED (uint32_t)(0x00)
#define STRIDE_SUPPORTED (uint32_t)(0x01)
#define DOUBLE_BUFFER_NOT_SUPPORTED (uint32_t)(0x00)
#define DOUBLE_BUFFER_SUPPORTED (uint32_t)(0x01)
#define FIELD_DPB_NOT_SUPPORTED (uint32_t)(0x00)
#define FIELD_DPB_SUPPORTED (uint32_t)(0x01)
#define AVS_PLUS_NOT_SUPPORTED (uint32_t)(0x00)
#define AVS_PLUS_SUPPORTED (uint32_t)(0x01)
#define ADDR64_ENV_NOT_SUPPORTED (uint32_t)(0x00)
#define ADDR64_ENV_SUPPORTED (uint32_t)(0x01)

#define H264_NOT_SUPPORTED_FUSE (uint32_t)(0x00)
#define H264_FUSE_ENABLED (uint32_t)(0x01)
#define MPEG4_NOT_SUPPORTED_FUSE (uint32_t)(0x00)
#define MPEG4_FUSE_ENABLED (uint32_t)(0x01)
#define MPEG2_NOT_SUPPORTED_FUSE (uint32_t)(0x00)
#define MPEG2_FUSE_ENABLED (uint32_t)(0x01)
#define SORENSON_SPARK_NOT_SUPPORTED_FUSE (uint32_t)(0x00)
#define SORENSON_SPARK_ENABLED (uint32_t)(0x01)
#define JPEG_NOT_SUPPORTED_FUSE (uint32_t)(0x00)
#define JPEG_FUSE_ENABLED (uint32_t)(0x01)
#define VP6_NOT_SUPPORTED_FUSE (uint32_t)(0x00)
#define VP6_FUSE_ENABLED (uint32_t)(0x01)
#define VP7_NOT_SUPPORTED_FUSE (uint32_t)(0x00)
#define VP7_FUSE_ENABLED (uint32_t)(0x01)
#define VP8_NOT_SUPPORTED_FUSE (uint32_t)(0x00)
#define VP8_FUSE_ENABLED (uint32_t)(0x01)
#define VC1_NOT_SUPPORTED_FUSE (uint32_t)(0x00)
#define VC1_FUSE_ENABLED (uint32_t)(0x01)
#define JPEG_PROGRESSIVE_NOT_SUPPORTED_FUSE (uint32_t)(0x00)
#define JPEG_PROGRESSIVE_FUSE_ENABLED (uint32_t)(0x01)
#define REF_BUF_NOT_SUPPORTED_FUSE (uint32_t)(0x00)
#define REF_BUF_FUSE_ENABLED (uint32_t)(0x01)
#define AVS_NOT_SUPPORTED_FUSE (uint32_t)(0x00)
#define AVS_FUSE_ENABLED (uint32_t)(0x01)
#define RV_NOT_SUPPORTED_FUSE (uint32_t)(0x00)
#define RV_FUSE_ENABLED (uint32_t)(0x01)
#define MVC_NOT_SUPPORTED_FUSE (uint32_t)(0x00)
#define MVC_FUSE_ENABLED (uint32_t)(0x01)

#define PP_NOT_SUPPORTED_FUSE (uint32_t)(0x00)
#define PP_FUSE_ENABLED (uint32_t)(0x01)
#define PP_FUSE_DEINTERLACING_ENABLED (uint32_t)(0x40000000)
#define PP_FUSE_ALPHA_BLENDING_ENABLED (uint32_t)(0x20000000)
#define MAX_PP_OUT_WIDHT_1920_FUSE_ENABLED (uint32_t)(0x00008000)
#define MAX_PP_OUT_WIDHT_1280_FUSE_ENABLED (uint32_t)(0x00004000)
#define MAX_PP_OUT_WIDHT_720_FUSE_ENABLED (uint32_t)(0x00002000)
#define MAX_PP_OUT_WIDHT_352_FUSE_ENABLED (uint32_t)(0x00001000)

/* Picture dimensions are checked
        currently in vp9 and hecv api code */
#if !defined(MODEL_SIMULATION) || defined(HW_PIC_DIMENSIONS)
#define MIN_PIC_WIDTH 144
#define MIN_PIC_HEIGHT 144

#define MIN_PIC_WIDTH_VP9 72
#define MIN_PIC_HEIGHT_VP9 72

/* DTRC minimum size = 96x8, So the minimum size for vp9 decoding
        should be redefined if DTRC is enabled */
#define MIN_PIC_WIDTH_VP9_EN_DTRC 96
#define MIN_PIC_HEIGHT_VP9_EN_DTRC 72

#else
#define MIN_PIC_WIDTH 8
#define MIN_PIC_HEIGHT 8

#define MIN_PIC_WIDTH_VP9 8
#define MIN_PIC_HEIGHT_VP9 8

#define MIN_PIC_WIDTH_VP9_EN_DTRC 8
#define MIN_PIC_HEIGHT_VP9_EN_DTRC 8

#endif

struct DecHwConfig {
   uint32_t mpeg4_support;
   uint32_t
      custom_mpeg4_support;
   uint32_t h264_support;
   uint32_t vc1_support;
   uint32_t mpeg2_support;
   uint32_t jpeg_support;
   uint32_t jpeg_prog_support; /* one of the Progressive JPEG values defined above
                           */
   uint32_t max_dec_pic_width;
   uint32_t max_dec_pic_height;
   uint32_t pp_support;
   uint32_t pp_config;
   uint32_t max_pp_out_pic_width;
   uint32_t sorenson_spark_support; /* one of the SORENSON_SPARK values defined
                                * above
                                */
   uint32_t ref_buf_support;
   uint32_t tiled_mode_support;
   uint32_t vp6_support;
   uint32_t vp7_support;
   uint32_t vp8_support;
   uint32_t vp9_support;
   uint32_t avs_support;
   uint32_t jpeg_esupport;
   uint32_t rv_support;
   uint32_t mvc_support;
   uint32_t webp_support;
   uint32_t ec_support;
   uint32_t stride_support;
   uint32_t field_dpb_support;
   uint32_t avs_plus_support;
   uint32_t addr64_support;
   uint32_t hevc_support;
   uint32_t
      double_buffer_support;

   uint32_t hevc_main10_support;
   uint32_t vp9_10bit_support;
   uint32_t ds_support;
   uint32_t rfc_support;
   uint32_t ring_buffer_support;

   uint32_t fmt_p010_support;
   uint32_t fmt_customer1_support;
   uint32_t mrb_prefetch;
};

struct DecSwHwBuild {
   uint32_t sw_build;
   uint32_t hw_build;
   struct DecHwConfig hw_config[MAX_ASIC_CORES];
};

enum DecDpbFlags {

   DEC_REF_FRM_RASTER_SCAN = 0x0,
   DEC_REF_FRM_TILED_DEFAULT = 0x1,

   DEC_DPB_ALLOW_FIELD_ORDERING = 0x40000000
};

#define DEC_REF_FRM_FMT_MASK 0x01
#define LOW_LATENCY_PACKET_SIZE 256

enum DecDpbMode { DEC_DPB_FRAME = 0, DEC_DPB_INTERLACED_FIELD = 1 };

enum DecDecoderMode {
   DEC_NORMAL = 0,
   DEC_LOW_LATENCY = 1,
   DEC_LOW_LATENCY_RTL = 2,
   DEC_SECURITY = 3,
   DEC_PARTIAL_DECODING = 4
};

enum mwv207_delogo_mode {
   PIXEL_NO_DELOGO = 0,
   PIXEL_REPLACE = 1,
   PIXEL_INTERPOLATION = 2
};

#define DEC_DPB_DEFAULT DEC_DPB_FRAME

enum mwv207_dec_picture_format {
   DEC_OUT_FRM_TILED_4X4 = 0,
   DEC_OUT_FRM_TILED_8X4 = 1,
   DEC_OUT_FRM_RASTER_SCAN = 2,
   DEC_OUT_FRM_PLANAR_420 = 3,
   
   DEC_OUT_FRM_MONOCHROME,
   DEC_OUT_FRM_RFC,
   DEC_OUT_FRM_RGB,

   DEC_OUT_FRM_YUV420TILE,
   DEC_OUT_FRM_YUV420TILE_PACKED,
   DEC_OUT_FRM_YUV420TILE_P010,
   DEC_OUT_FRM_YUV420TILE_1010,
   DEC_OUT_FRM_YUV420SP,
   DEC_OUT_FRM_YUV420SP_PACKED,
   DEC_OUT_FRM_YUV420SP_P010,
   DEC_OUT_FRM_YUV420SP_1010,
   DEC_OUT_FRM_YUV420P,
   DEC_OUT_FRM_YUV420P_PACKED,
   DEC_OUT_FRM_YUV420P_P010,
   DEC_OUT_FRM_YUV420P_1010,

   DEC_OUT_FRM_YUV400TILE,
   DEC_OUT_FRM_YUV400TILE_P010,
   DEC_OUT_FRM_YUV400TILE_1010,
   DEC_OUT_FRM_YUV400,
   DEC_OUT_FRM_YUV400_P010,
   DEC_OUT_FRM_YUV400_1010,

   DEC_OUT_FRM_NV21TILE,
   DEC_OUT_FRM_NV21TILE_PACKED,
   DEC_OUT_FRM_NV21TILE_P010,
   DEC_OUT_FRM_NV21TILE_1010,
   DEC_OUT_FRM_NV21SP,
   DEC_OUT_FRM_NV21SP_PACKED,
   DEC_OUT_FRM_NV21SP_P010,
   DEC_OUT_FRM_NV21SP_1010,
   DEC_OUT_FRM_NV21P,
   DEC_OUT_FRM_NV21P_PACKED,
   DEC_OUT_FRM_NV21P_P010,
   DEC_OUT_FRM_NV21P_1010,

   DEC_OUT_FRM_RGB888,
   DEC_OUT_FRM_BGR888,
   DEC_OUT_FRM_R16G16B16,
   DEC_OUT_FRM_B16G16R16,
   DEC_OUT_FRM_RGB888_P,
   DEC_OUT_FRM_BGR888_P,
   DEC_OUT_FRM_R16G16B16_P,
   DEC_OUT_FRM_B16G16R16_P,
   DEC_OUT_FRM_ARGB888,
   DEC_OUT_FRM_ABGR888,
   DEC_OUT_FRM_A2R10G10B10,
   DEC_OUT_FRM_A2B10G10R10,
   DEC_OUT_FRM_XRGB888,
   DEC_OUT_FRM_XBGR888
};

#define IS_PIC_PACKED_RGB(fmt)                                               \
   ((fmt) == DEC_OUT_FRM_RGB888 || (fmt) == DEC_OUT_FRM_BGR888               \
    || (fmt) == DEC_OUT_FRM_R16G16B16 || (fmt) == DEC_OUT_FRM_B16G16R16      \
    || (fmt) == DEC_OUT_FRM_ARGB888 || (fmt) == DEC_OUT_FRM_ABGR888          \
    || (fmt) == DEC_OUT_FRM_A2R10G10B10 || (fmt) == DEC_OUT_FRM_A2B10G10R10  \
    || (fmt) == DEC_OUT_FRM_XRGB888 || (fmt) == DEC_OUT_FRM_XBGR888)

#define IS_PIC_PLANAR_RGB(fmt)                                               \
   ((fmt) == DEC_OUT_FRM_RGB888_P || (fmt) == DEC_OUT_FRM_BGR888_P           \
    || (fmt) == DEC_OUT_FRM_R16G16B16_P || (fmt) == DEC_OUT_FRM_B16G16R16_P)

#define IS_PIC_MONOCHROME(fmt)                                               \
   ((fmt) == DEC_OUT_FRM_MONOCHROME || (fmt) == DEC_OUT_FRM_YUV400TILE       \
    || (fmt) == DEC_OUT_FRM_YUV400TILE_P010 || (fmt) == DEC_OUT_FRM_YUV400   \
    || (fmt) == DEC_OUT_FRM_YUV400_1010 || (fmt) == DEC_OUT_FRM_YUV400_P010)

#define IS_PIC_PLANAR(fmt)                                                   \
   ((fmt) == DEC_OUT_FRM_PLANAR_420 || (fmt) == DEC_OUT_FRM_YUV420P          \
    || (fmt) == DEC_OUT_FRM_YUV420P_PACKED                                   \
    || (fmt) == DEC_OUT_FRM_YUV420P_P010 || (fmt) == DEC_OUT_FRM_NV21P       \
    || (fmt) == DEC_OUT_FRM_NV21P_PACKED || (fmt) == DEC_OUT_FRM_NV21P_P010  \
    || IS_PIC_PLANAR_RGB(fmt))

#define IS_PIC_NV21(fmt)                                                     \
   ((fmt) == DEC_OUT_FRM_NV21TILE || (fmt) == DEC_OUT_FRM_NV21TILE_PACKED    \
    || (fmt) == DEC_OUT_FRM_NV21TILE_P010 || (fmt) == DEC_OUT_FRM_NV21SP     \
    || (fmt) == DEC_OUT_FRM_NV21SP_PACKED                                    \
    || (fmt) == DEC_OUT_FRM_NV21SP_P010 || (fmt) == DEC_OUT_FRM_NV21P        \
    || (fmt) == DEC_OUT_FRM_NV21P_PACKED || (fmt) == DEC_OUT_FRM_NV21P_P010)

#define IS_PIC_SEMIPLANAR(fmt)                                               \
   ((fmt) == DEC_OUT_FRM_YUV420SP || (fmt) == DEC_OUT_FRM_YUV420SP_PACKED    \
    || (fmt) == DEC_OUT_FRM_YUV420SP_P010 || (fmt) == DEC_OUT_FRM_NV21SP     \
    || (fmt) == DEC_OUT_FRM_NV21SP_PACKED                                    \
    || (fmt) == DEC_OUT_FRM_NV21SP_P010 || (fmt) == DEC_OUT_FRM_RASTER_SCAN)

#define IS_PIC_8BIT(fmt)                                                     \
   ((fmt) == DEC_OUT_FRM_YUV420TILE || (fmt) == DEC_OUT_FRM_YUV420SP         \
    || (fmt) == DEC_OUT_FRM_YUV420P || (fmt) == DEC_OUT_FRM_YUV400TILE       \
    || (fmt) == DEC_OUT_FRM_YUV400 || (fmt) == DEC_OUT_FRM_NV21TILE          \
    || (fmt) == DEC_OUT_FRM_NV21SP || (fmt) == DEC_OUT_FRM_NV21P             \
    || (fmt) == DEC_OUT_FRM_RGB888_P || (fmt) == DEC_OUT_FRM_BGR888_P)

#define IS_PIC_10BIT(fmt)                                                    \
   ((fmt) == DEC_OUT_FRM_YUV420TILE_1010                                     \
    || (fmt) == DEC_OUT_FRM_YUV420SP_1010                                    \
    || (fmt) == DEC_OUT_FRM_YUV420P_1010                                     \
    || (fmt) == DEC_OUT_FRM_YUV400TILE_1010                                  \
    || (fmt) == DEC_OUT_FRM_YUV400_1010                                      \
    || (fmt) == DEC_OUT_FRM_NV21TILE_1010                                    \
    || (fmt) == DEC_OUT_FRM_NV21SP_1010 || (fmt) == DEC_OUT_FRM_NV21P_1010)

#define IS_PIC_16BIT(fmt)                                                    \
   ((fmt) == DEC_OUT_FRM_YUV420TILE_P010                                     \
    || (fmt) == DEC_OUT_FRM_YUV420SP_P010                                    \
    || (fmt) == DEC_OUT_FRM_YUV420P_P010                                     \
    || (fmt) == DEC_OUT_FRM_YUV400TILE_P010                                  \
    || (fmt) == DEC_OUT_FRM_YUV400_P010                                      \
    || (fmt) == DEC_OUT_FRM_NV21TILE_P010                                    \
    || (fmt) == DEC_OUT_FRM_NV21SP_P010 || (fmt) == DEC_OUT_FRM_NV21P_P010   \
    || (fmt) == DEC_OUT_FRM_R16G16B16_P || (fmt) == DEC_OUT_FRM_B16G16R16_P)

#define IS_PIC_24BIT(fmt)                                                    \
   ((fmt) == DEC_OUT_FRM_RGB888 || (fmt) == DEC_OUT_FRM_BGR888)

#define IS_PIC_32BIT(fmt)                                                    \
   ((fmt) == DEC_OUT_FRM_ARGB888 || (fmt) == DEC_OUT_FRM_ABGR888             \
    || (fmt) == DEC_OUT_FRM_A2R10G10B10 || (fmt) == DEC_OUT_FRM_A2B10G10R10  \
    || (fmt) == DEC_OUT_FRM_XRGB888 || (fmt) == DEC_OUT_FRM_XBGR888)

#define IS_PIC_48BIT(fmt)                                                    \
   ((fmt) == DEC_OUT_FRM_R16G16B16 || (fmt) == DEC_OUT_FRM_B16G16R16)

#define IS_PIC_TILE(fmt)                                                     \
   ((fmt) == DEC_OUT_FRM_TILED_4X4 || (fmt) == DEC_OUT_FRM_TILED_8X4         \
    || (fmt) == DEC_OUT_FRM_YUV420TILE                                       \
    || (fmt) == DEC_OUT_FRM_YUV420TILE_PACKED                                \
    || (fmt) == DEC_OUT_FRM_YUV420TILE_P010                                  \
    || (fmt) == DEC_OUT_FRM_YUV420TILE_1010                                  \
    || (fmt) == DEC_OUT_FRM_YUV400TILE                                       \
    || (fmt) == DEC_OUT_FRM_YUV400TILE_P010                                  \
    || (fmt) == DEC_OUT_FRM_YUV400TILE_1010 || (fmt) == DEC_OUT_FRM_NV21TILE \
    || (fmt) == DEC_OUT_FRM_NV21TILE_PACKED                                  \
    || (fmt) == DEC_OUT_FRM_NV21TILE_P010                                    \
    || (fmt) == DEC_OUT_FRM_NV21TILE_1010)

#define IS_PIC_RFC(fmt) ((fmt) == DEC_OUT_FRM_RFC)

/** Picture coding type */
enum DecPicCodingType {
   DEC_PIC_TYPE_I = 0,
   DEC_PIC_TYPE_P = 1,
   DEC_PIC_TYPE_B = 2,
   DEC_PIC_TYPE_D = 3,
   DEC_PIC_TYPE_FI = 4,
   DEC_PIC_TYPE_BI = 5
};

enum DecPicturePixelFormat {
   DEC_OUT_PIXEL_DEFAULT =
      0,
   DEC_OUT_PIXEL_P010 = 1,
   DEC_OUT_PIXEL_CUSTOMER1 =
      2, /* customer format: a 128-bit burst output in
                                                                   packed
            little endian format */
   DEC_OUT_PIXEL_CUT_8BIT = 3,
   DEC_OUT_PIXEL_RFC = 4,
   DEC_OUT_PIXEL_1010 = 5,
};

enum DecErrorHandling {
   DEC_EC_PIC_COPY_REF = 0x1,
   DEC_EC_PIC_PARTIAL = 0x2,
   DEC_EC_PIC_PARTIAL_IGNORE = 0x4,
   DEC_EC_PIC_ALL_IGNORE =
      0x8,
   DEC_EC_REF_REPLACE = 0x100,
   DEC_EC_REF_NEXT_IDR = 0x200,
   DEC_EC_REF_NEXT_I = 0x400,
   DEC_EC_OUT_ALL = 0x10000,
   DEC_EC_OUT_NO_ERROR = 0x20000,
   DEC_EC_OUT_FIRST_FIELD_OK = 0x40000,
   DEC_EC_PICTURE_FREEZE = DEC_EC_PIC_COPY_REF,
   DEC_EC_VIDEO_FREEZE = (DEC_EC_PIC_COPY_REF | DEC_EC_REF_NEXT_IDR),
   DEC_EC_PARTIAL_FREEZE = DEC_EC_PIC_PARTIAL,
   DEC_EC_PARTIAL_IGNORE = DEC_EC_PIC_PARTIAL_IGNORE,
   DEC_EC_FAST_FREEZE =
      (DEC_EC_PIC_ALL_IGNORE | DEC_EC_REF_NEXT_I
       | DEC_EC_OUT_NO_ERROR) /* If current picture is erroneous, mark all
                                                                         pictures
                                 as erroneous until a new I picture
                                                                         encountered,
                                 and discard erroneous pictures */
};

struct mwv207_dec_crop_cfg {
   uint32_t crop_x;
   uint32_t crop_y;
   uint32_t crop_w;
   uint32_t crop_h;
   uint32_t crop_enabled;
};

enum mwv207_scale_mode { NON_SCALE, FIXED_DOWNSCALE, FLEXIBLE_SCALE };

struct mwv207_dec_fixed_scale_cfg {
   uint32_t down_scale_x;
   uint32_t down_scale_y;
   uint32_t fixed_scale_enabled;
};

/* Stride alignment: aligned to 8/16/.../512 bytes */
typedef enum {
   DEC_ALIGN_1B = 0,
   DEC_ALIGN_8B = 3,
   DEC_ALIGN_16B,
   DEC_ALIGN_32B,
   DEC_ALIGN_64B,
   DEC_ALIGN_128B,
   DEC_ALIGN_256B,
   DEC_ALIGN_512B,
   DEC_ALIGN_1024B,
   DEC_ALIGN_2048B,
} mwv207_dec_pic_alignment;

typedef struct _DelogoConfig {
   uint32_t enabled;
   uint32_t x;
   uint32_t y;
   uint32_t w;
   uint32_t h;
   uint32_t show;
   enum mwv207_delogo_mode mode;
   uint32_t Y;
   uint32_t U;
   uint32_t V;
} mwv207_delogo_config;

typedef struct _PpUnitConfig {
   uint32_t enabled;
   uint32_t tiled_e;
   uint32_t rgb;
   uint32_t rgb_planar;
   uint32_t cr_first;
   uint32_t shaper_enabled;
   uint32_t shaper_no_pad;
   uint32_t dec400_enabled;
   uint32_t planar;
   mwv207_dec_pic_alignment align;
   /* Stride for Y/C plane. SW should use the stride calculated from SW if
      it's
            set to 0. When not 0, SW should check the validation of the value.
    */
   uint32_t ystride;
   uint32_t cstride;
   struct {
      uint32_t enabled;
      uint32_t set_by_user; /* cropping set by user, use this variable to record
                        * whether user set crop.*/
      uint32_t x;
      uint32_t y;
      uint32_t width;
      uint32_t height;
   } crop;
   struct {
      uint32_t enabled;
      uint32_t x;
      uint32_t y;
      uint32_t width;
      uint32_t height;
   } crop2;
   struct {
      uint32_t enabled;
      uint32_t set_by_user; /* scaling set by user, use this variable to record
                        * whether user set scale.*/
      uint32_t x;
      uint32_t y;
      uint32_t ratio_x; /* 0 indicate flexiable mode, or 1/2/4/8 indicate ratio */
      uint32_t ratio_y;
      uint32_t width;
      uint32_t height;
   } scale;
   uint32_t monochrome;
   uint32_t out_p010;
   uint32_t out_1010;
   uint32_t out_I010;
   uint32_t out_L010;
   uint32_t out_be;
   uint32_t out_cut_8bits;
   uint32_t video_range;
   uint32_t range_max;
   uint32_t range_min;
   uint32_t out_format;
   uint32_t rgb_format; /* RGB output format: RGB888/BGR888/R16G16B16/... */
   uint32_t rgb_stan;
   uint32_t rgb_alpha;
   uint32_t pp_filter;
   uint32_t x_filter_param;
   uint32_t y_filter_param;
   struct pipe_video_buffer *outsurface;
   uint32_t outsurface_origin_width;
   uint32_t outsurface_origin_height;
   uint64_t pp_out_extern_addr;
   uint32_t pp_lanczos_enable;
} mwv207_pp_unit_config;

#endif

