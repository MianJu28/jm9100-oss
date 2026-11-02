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

#include "mwv207_decoder_h264_nal_unit.h"
#include "mwv207_decoder_sw_util.h"
#include <string.h>

/*------------------------------------------------------------------------------
         2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
         3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
         4. Local function prototypes
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------

         Function name: mwv207_h264_bsd_decode_nal_unit

                  Functional description:
                                Decode NAL unit header information

                  Inputs:
                                stream       pointer to stream data structure

                  Outputs:
                                p_nal_unit        NAL unit header information
is stored here

                  Returns:
                                MWV207_OK       success
                                MWV207_NOK      invalid NAL unit header
information

------------------------------------------------------------------------------*/

uint32_t
mwv207_h264_bsd_decode_nal_unit(struct mwv207_strm_data *stream,
                               mwv207_h264_nal_unit *p_nal_unit)
{

   uint32_t tmp;

   (void)memset(p_nal_unit, 0, sizeof(mwv207_h264_nal_unit));

   tmp = mwv207_sw_get_bits(stream, 1);
   /* Assuming that NAL unit starts from byte boundary => don't have to check
    * following 7 bits for END_OF_STREAM */
   if (tmp == END_OF_STREAM)
      return (MWV207_NOK);

   tmp = mwv207_sw_get_bits(stream, 2);
   p_nal_unit->nal_ref_idc = tmp;

   tmp = mwv207_sw_get_bits(stream, 5);
   p_nal_unit->nal_unit_type = (mwv207_h264_nal_unit_type)tmp;

   if ((tmp == NAL_CODED_SLICE_DP_A) || (tmp == NAL_CODED_SLICE_DP_B)
       || (tmp == NAL_CODED_SLICE_DP_C)) {
      return (MWV207_NOK);
   }

   if (((tmp == NAL_SEQ_PARAM_SET) || (tmp == NAL_PIC_PARAM_SET)
        || (tmp == NAL_CODED_SLICE_IDR))
       && (p_nal_unit->nal_ref_idc == 0)) {
      return (MWV207_NOK);
   }

   else if (((tmp == NAL_SEI) || (tmp == NAL_ACCESS_UNIT_DELIMITER)
             || (tmp == NAL_END_OF_SEQUENCE) || (tmp == NAL_END_OF_STREAM)
             || (tmp == NAL_FILLER_DATA))
            && (p_nal_unit->nal_ref_idc != 0)) {
      return (MWV207_NOK);
   }

   if (p_nal_unit->nal_unit_type == NAL_PREFIX
       || p_nal_unit->nal_unit_type == NAL_CODED_SLICE_EXT) {
      tmp = mwv207_sw_get_bits(stream, 1);

      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);

      p_nal_unit->svc_extension_flag = tmp;

      if (tmp == 0) {

         tmp = mwv207_sw_get_bits(stream, 1);
         p_nal_unit->non_idr_flag = tmp;
         tmp = mwv207_sw_get_bits(stream, 6);
         p_nal_unit->priority_id = tmp;
         tmp = mwv207_sw_get_bits(stream, 10);
         p_nal_unit->view_id = tmp;
         tmp = mwv207_sw_get_bits(stream, 3);
         p_nal_unit->temporal_id = tmp;
         tmp = mwv207_sw_get_bits(stream, 1);
         p_nal_unit->anchor_pic_flag = tmp;
         tmp = mwv207_sw_get_bits(stream, 1);
         p_nal_unit->inter_view_flag = tmp;

         tmp = mwv207_sw_get_bits(stream, 1);
      } else {

         tmp = mwv207_sw_get_bits(stream, 1);
         tmp = mwv207_sw_get_bits(stream, 6);
         tmp = mwv207_sw_get_bits(stream, 1);
         tmp = mwv207_sw_get_bits(stream, 3);
         tmp = mwv207_sw_get_bits(stream, 4);
         tmp = mwv207_sw_get_bits(stream, 3);
         tmp = mwv207_sw_get_bits(stream, 1);
         tmp = mwv207_sw_get_bits(stream, 1);
         tmp = mwv207_sw_get_bits(stream, 1);
         tmp = mwv207_sw_get_bits(stream, 2);
      }

      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);
   }

   return (MWV207_OK);
}

