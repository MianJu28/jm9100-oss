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

#ifndef MWV207_9200_VDIEO_DEC_CS_H
#define MWV207_9200_VDIEO_DEC_CS_H

#define DWL_OK 0
#define DWL_ERROR -1

#define DWL_CACHE_OK 0
#define DWL_CACHE_FATAL_RECOVERY 1
#define DWL_CACHE_FATAL_UNRECOVERY 2

#define DWL_HW_WAIT_OK DWL_OK
#define DWL_HW_WAIT_ERROR DWL_ERROR
#define DWL_HW_WAIT_TIMEOUT 1

#define DWL_CLIENT_TYPE_H264_DEC 1U
#define DWL_CLIENT_TYPE_MPEG4_DEC 2U
#define DWL_CLIENT_TYPE_JPEG_DEC 3U
#define DWL_CLIENT_TYPE_PP 4U
#define DWL_CLIENT_TYPE_VC1_DEC 5U
#define DWL_CLIENT_TYPE_MPEG2_DEC 6U
#define DWL_CLIENT_TYPE_VP6_DEC 7U
#define DWL_CLIENT_TYPE_AVS_DEC 8U
#define DWL_CLIENT_TYPE_RV_DEC 9U
#define DWL_CLIENT_TYPE_VP8_DEC 10U
#define DWL_CLIENT_TYPE_VP9_DEC 11U
#define DWL_CLIENT_TYPE_HEVC_DEC 12U
#define DWL_CLIENT_TYPE_ST_PP 14U
#define DWL_CLIENT_TYPE_H264_MAIN10 15U
#define DWL_CLIENT_TYPE_AVS2_DEC 16U
#define DWL_CLIENT_TYPE_AV1_DEC 17U

#define DEC_HW_IRQ_BUFFER 0x08
#define DEC_HW_IRQ_EXT_TIMEOUT 0x400

#define MWV207DEC_IRQ_STAT_DEC 1

#endif

