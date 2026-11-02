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

#include "mwv207_video_dec_vpufeature.h"
#include <stddef.h>
#include <string.h>

struct mwv207_video_dec_hw_features feature_list[] = {
#include "mwv207_video_dec_vpu_features_list.h"
};

void
mwv207_video_dec_get_hw_features_by_id(
   uint32_t hw_build_id, struct mwv207_video_dec_hw_features *hw_feature)
{
   struct mwv207_video_dec_hw_features *cfg = feature_list;
   uint32_t i;

   if (hw_feature == NULL)
      return;

   for (i = 0;
        i < sizeof(feature_list) / sizeof(struct mwv207_video_dec_hw_features)
           - 1;
        i++) {
      if ((hw_build_id & cfg->id_mask) == cfg->id)
         break;

      cfg++;
   }
   memcpy(hw_feature, cfg, sizeof(struct mwv207_video_dec_hw_features));
}

#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif


