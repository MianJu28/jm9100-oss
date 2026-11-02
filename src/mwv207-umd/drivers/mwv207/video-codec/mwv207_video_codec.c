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

#include "mwv207_video_codec.h"
#include "mwv207_log.h"
#include "mwv207_video_decoder.h"
#include "vl/vl_decoder.h"
#include "vl/vl_video_buffer.h"

static bool
mwv207_profile_supported(struct pipe_screen *screen,
                        enum pipe_video_profile profile,
                        enum pipe_video_entrypoint entrypoint)
{
   assert(screen);
   switch (profile) {
   case PIPE_VIDEO_PROFILE_MPEG4_AVC_BASELINE:
   case PIPE_VIDEO_PROFILE_MPEG4_AVC_CONSTRAINED_BASELINE:
   case PIPE_VIDEO_PROFILE_MPEG4_AVC_MAIN:
   case PIPE_VIDEO_PROFILE_MPEG4_AVC_EXTENDED:
   case PIPE_VIDEO_PROFILE_MPEG4_AVC_HIGH:
   case PIPE_VIDEO_PROFILE_MPEG4_AVC_HIGH10:
   case PIPE_VIDEO_PROFILE_MPEG4_AVC_HIGH422:
   case PIPE_VIDEO_PROFILE_MPEG4_AVC_HIGH444:
   case PIPE_VIDEO_PROFILE_HEVC_MAIN:
   case PIPE_VIDEO_PROFILE_HEVC_MAIN_10:
      return entrypoint != PIPE_VIDEO_ENTRYPOINT_ENCODE;
   default:
      return false;
   }
}

static int
mwv207_get_video_param(struct pipe_screen *pscreen,
                      enum pipe_video_profile profile,
                      enum pipe_video_entrypoint entrypoint,
                      enum pipe_video_cap param)
{
   switch (param) {
   case PIPE_VIDEO_CAP_SUPPORTED:
      return mwv207_profile_supported(pscreen, profile, entrypoint);
   case PIPE_VIDEO_CAP_NPOT_TEXTURES:
      return 1;
   case PIPE_VIDEO_CAP_MAX_WIDTH:
   case PIPE_VIDEO_CAP_MAX_HEIGHT:
      return vl_video_buffer_max_size(pscreen);
   case PIPE_VIDEO_CAP_PREFERED_FORMAT:
      
      return PIPE_FORMAT_NV12;
   case PIPE_VIDEO_CAP_PREFERS_INTERLACED:
      return false;
   case PIPE_VIDEO_CAP_SUPPORTS_INTERLACED:
      return false;
   case PIPE_VIDEO_CAP_SUPPORTS_PROGRESSIVE:
      return true;
   case PIPE_VIDEO_CAP_MAX_LEVEL:
      return vl_level_supported(pscreen, profile);
   default:
      return 0;
   }
}

static bool
mwv207_is_video_format_supported(struct pipe_screen *pscreen,
                                enum pipe_format format,
                                enum pipe_video_profile profile,
                                enum pipe_video_entrypoint entrypoint)
{
   
   
   bool ret = false;
   switch (format) {
   case PIPE_FORMAT_NV12:
      ret = true;
      break;
   
   
   default:
      break;
   }

   /* FIX(jm9100-oss): 原实现是两个独立 switch, 第二个按 profile 的 switch
    * 无条件 ret = true, 把第一个按 format 的判定结果整体覆盖(OR 语义)。
    * 后果: P010/P016/YV12/IYUV/UYVY/YUYV/BGRA 等一律被谎报支持——
    * VA 前端 config.c 据此广播 VA_RT_FORMAT_YUV420_10BPP, ffmpeg/mpv
    * 为 10bit 流创建 P010 surface, 而纹理格式表(texture_infos)并无
    * R16_UNORM/R16G16_UNORM, P010 平面建不出 sampler view, 渲染端
    * blit 全拒 → 10bit HEVC 硬解黑屏(mwv207_blit.c:947)。
    * 正确语义是 AND: format 支持 && 该 profile 的解码链路可用。
    * 例外: VPP 配置传入 PIPE_VIDEO_PROFILE_UNKNOWN, 不做 profile 过滤。 */
   if (ret && profile != PIPE_VIDEO_PROFILE_UNKNOWN) {
      switch (profile) {
      case PIPE_VIDEO_PROFILE_MPEG4_AVC_BASELINE:
      case PIPE_VIDEO_PROFILE_MPEG4_AVC_CONSTRAINED_BASELINE:
      case PIPE_VIDEO_PROFILE_MPEG4_AVC_MAIN:
      case PIPE_VIDEO_PROFILE_MPEG4_AVC_EXTENDED:
      case PIPE_VIDEO_PROFILE_MPEG4_AVC_HIGH:
      case PIPE_VIDEO_PROFILE_MPEG4_AVC_HIGH10:
      case PIPE_VIDEO_PROFILE_MPEG4_AVC_HIGH422:
      case PIPE_VIDEO_PROFILE_MPEG4_AVC_HIGH444:
      case PIPE_VIDEO_PROFILE_HEVC_MAIN:
      case PIPE_VIDEO_PROFILE_HEVC_MAIN_10:
         break;
      default:
         ret = false;
         break;
      }
   }
   return ret;
}

void
mwv207_context_video_codec_init(struct pipe_context *context)
{
   mwv207_log_init();
   MWV207_LOG(MWV207_LEVEL_INFO, "\n");
   context->screen->get_video_param = mwv207_get_video_param;
   context->screen->is_video_format_supported =
      mwv207_is_video_format_supported;
   context->create_video_codec = mwv207_create_codec;
   context->create_video_buffer = mwv207_pipe_video_buffer_create;
}

struct pipe_video_codec *
mwv207_create_codec(struct pipe_context *context,
                   const struct pipe_video_codec *codec)
{
   return mwv207_create_decoder(context, codec);
}

