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

#include "mwv207_decoder_hevc_slice_header.h"
#include "mwv207_decoder_hevc_exp_golomb.h"
#include "mwv207_decoder_sw_util.h"
#include <string.h>

static uint32_t
hevc_decode_short_term_ref_pic_set(struct mwv207_strm_data *stream,
                             struct mwv207_st_ref_pic_set *st_rps,
                             uint32_t slice_header,
                             uint32_t st_rps_idx)
{
   uint32_t tmp, i, j, value;
   uint32_t delta_idx;
   int32_t delta_rps;
   int32_t delta_poc;
   uint32_t num_delta_pocs;
   uint32_t use_delta, used_by_curr_pic;

   
   

   if (st_rps_idx) {
      tmp = mwv207_sw_get_bits(stream, 1);
      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);
   } else
      tmp = 0;
   if (tmp) {
      if (slice_header) {
         tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
         if (tmp != MWV207_OK)
            return (tmp);
      } else
         value = 0;

      delta_idx = value + 1;
      if (delta_idx > st_rps_idx) {
         return (MWV207_NOK);
      }

      *st_rps = *(st_rps - delta_idx);

      tmp = mwv207_sw_get_bits(stream, 1);
      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);
      delta_rps = tmp;
      tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
      if (tmp != MWV207_OK)
         return (tmp);
      delta_rps = delta_rps ? -(int32_t)(value + 1) : (int32_t)(value + 1);
      num_delta_pocs = st_rps->num_positive_pics + st_rps->num_negative_pics;

      for (i = 0, j = 0; i <= num_delta_pocs; i++) {
         tmp = mwv207_sw_get_bits(stream, 1);
         if (tmp == END_OF_STREAM)
            return (MWV207_NOK);
         used_by_curr_pic = tmp;
         if (!used_by_curr_pic) {
            tmp = mwv207_sw_get_bits(stream, 1);
            if (tmp == END_OF_STREAM)
               return (MWV207_NOK);
            use_delta = tmp;
         } else
            use_delta = 1;
         if (use_delta) {
            delta_poc = (i < num_delta_pocs ? st_rps->elem[i].delta_poc : 0)
               + delta_rps;
            st_rps->elem[j].delta_poc = delta_poc;
            st_rps->elem[j].used_by_curr_pic = used_by_curr_pic;
            j++;
         }
      }
      num_delta_pocs = j;
      for (i = 0; i < num_delta_pocs; i++) {
         if (st_rps->elem[i].delta_poc > 0)
            break;
      }
      st_rps->num_negative_pics = i;
      st_rps->num_positive_pics = num_delta_pocs - i;

   } else {
      tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
      if (tmp != MWV207_OK)
         return (tmp);
      st_rps->num_negative_pics = value;

      if (st_rps->num_negative_pics > MAX_DPB_SIZE) {
         st_rps->num_negative_pics = 0;
         return MWV207_NOK;
      }

      tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
      if (tmp != MWV207_OK)
         return (tmp);
      st_rps->num_positive_pics = value;

      if (st_rps->num_negative_pics + st_rps->num_positive_pics
          > MAX_DPB_SIZE) {
         st_rps->num_negative_pics = 0;
         st_rps->num_positive_pics = 0;
         return (MWV207_NOK);
      }

      delta_poc = 0;
      for (i = 0, j = 0; i < st_rps->num_negative_pics; i++, j++) {
         tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
         if (tmp != MWV207_OK)
            return (tmp);
         delta_poc -= value + 1;
         st_rps->elem[j].delta_poc = delta_poc;
         tmp = mwv207_sw_get_bits(stream, 1);
         if (tmp == END_OF_STREAM)
            return (MWV207_NOK);
         st_rps->elem[j].used_by_curr_pic = tmp;
      }
      delta_poc = 0;
      for (i = 0; i < st_rps->num_positive_pics; i++, j++) {
         tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
         if (tmp != MWV207_OK)
            return (tmp);
         delta_poc += value + 1;
         st_rps->elem[j].delta_poc = delta_poc;
         tmp = mwv207_sw_get_bits(stream, 1);
         if (tmp == END_OF_STREAM)
            return (MWV207_NOK);
         st_rps->elem[j].used_by_curr_pic = tmp;
      }
   }
   return MWV207_OK;
}

uint32_t
mwv207_hevc_decode_slice_header(struct mwv207_strm_data *stream,
                               struct mwv207_h265_picture_desc *pic_param,
                               uint32_t *hw_skip_bits,
                               struct mwv207_hevc_slice_header *slice_header,
                               uint32_t st_rps_bits /* ,
VASliceParameterBufferHEVC *slice_param */
)
{
   uint32_t tmp, i, value;
   uint32_t read_bits;
   uint32_t first_slice_in_pic;
   uint32_t tmp_count;
   uint32_t slice_type;
   uint32_t short_term_ref_pic_set_sps_flag;
   uint32_t num_long_term_sps = 0;
   uint32_t num_long_term_pics;

   tmp = mwv207_sw_get_bits(stream, 1);
   if (tmp == END_OF_STREAM)
      return (MWV207_NOK);
   first_slice_in_pic = tmp;

   if (pic_param->RAPPicFlag) {
      tmp = mwv207_sw_get_bits(stream, 1);
      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);
   }

   tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
   if (tmp != MWV207_OK)
      return (tmp);

   /* SW only decodes the first slice in the pic -> return error if not
    * first slice.
    * TODO: error handling, if first slice is missing, should we read
    * something (poc?) to properly handle missing stuff */
   

   

   slice_header->dependent_slice_flag = 0;
   if (!first_slice_in_pic) {
      if (pic_param->pps.dependent_slice_segments_enabled_flag) {
         tmp = mwv207_sw_get_bits(stream, 1);
         slice_header->dependent_slice_flag = tmp;
      }
      
      
      uint32_t ctb_size =
         (1
          << (pic_param->pps.sps.log2_min_luma_coding_block_size_minus3 + 3
              + pic_param->pps.sps.log2_diff_max_min_luma_coding_block_size));

      uint32_t ctb_width =
         NEXT_MULTIPLE(pic_param->pps.sps.pic_width_in_luma_samples, ctb_size)
         / ctb_size;
      uint32_t ctb_height =
         NEXT_MULTIPLE(pic_param->pps.sps.pic_height_in_luma_samples,
                       ctb_size)
         / ctb_size;
      tmp = mwv207_sw_num_bits(ctb_width * ctb_height);
      slice_header->slice_address = mwv207_sw_get_bits(stream, tmp);
   }

   if (!slice_header->dependent_slice_flag) {
      tmp = mwv207_sw_get_bits(stream,
                              pic_param->pps.num_extra_slice_header_bits);
      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);

      tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
      if (tmp != MWV207_OK)
         return (tmp);
      slice_type = value;
      slice_header->slice_type = value;
      /* slice type has to be either I, P or B slice. Only I slice is
       * allowed when current NAL unit is an RAP NAL */
      /* Some bitstreams have max_dec_pic_buffering = 0 while slice coded to P
       * slice.*/
      if (
         !IS_HEVC_I_SLICE(slice_type)
         && (pic_param
                ->RAPPicFlag /*||
                                                                  seq_param_set->max_dec_pic_buffering[seq_param_set->max_sub_layers
                                                                  - 1] == 0*/
             )) {
         
         
      }

      read_bits = stream->strm_buff_read_bits;
      tmp_count = stream->emul_byte_count;
      stream->emul_byte_count = 0;

      if (pic_param->pps.output_flag_present_flag) {
         tmp = mwv207_sw_get_bits(stream, 1);
         if (tmp == END_OF_STREAM)
            return (MWV207_NOK);
      }

      if (!pic_param->IDRPicFlag) {
         tmp = mwv207_sw_get_bits(
            stream,
            mwv207_sw_num_bits(
               (1 << (pic_param->pps.sps.log2_max_pic_order_cnt_lsb_minus4
                      + 4))
               - 1));
         if (tmp == END_OF_STREAM)
            return (MWV207_NOK);

         tmp = mwv207_sw_get_bits(stream, 1);
         if (tmp == END_OF_STREAM)
            return (MWV207_NOK);
         short_term_ref_pic_set_sps_flag = tmp;
         pic_param->st_rps_bits = st_rps_bits;

         if (!short_term_ref_pic_set_sps_flag) {
            tmp = hevc_decode_short_term_ref_pic_set(
               stream,
               /* decoded into last position in st_ref_pic_set array of sps
                */
               slice_header->st_ref_pic_set
                  + pic_param->pps.sps.num_short_term_ref_pic_sets,
               1,
               pic_param->pps.sps.num_short_term_ref_pic_sets);
         } else {
            if (pic_param->pps.sps.num_short_term_ref_pic_sets > 1) {
               i = mwv207_sw_num_bits(
                  pic_param->pps.sps.num_short_term_ref_pic_sets - 1);
               tmp = mwv207_sw_get_bits(stream, i);
               if (tmp == END_OF_STREAM)
                  return (MWV207_NOK);
            }
         }
         
         

         if (pic_param->pps.sps.long_term_ref_pics_present_flag) {
            uint32_t tot_long_term;
            uint32_t len;

            if (pic_param->pps.sps.num_long_term_ref_pics_sps) {
               tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
               if (tmp != MWV207_OK)
                  return (tmp);
               if (value >32) {
                  
                  return (MWV207_NOK);
               }
               slice_header->num_long_term_sps = value;
               num_long_term_sps = value;
            }
            tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
            if (tmp != MWV207_OK)
               return (tmp);
            if (value > 32) {
               
               return (MWV207_NOK);
            }
            num_long_term_pics = value;
            slice_header->num_long_term_pics = value;
            tot_long_term = num_long_term_sps + num_long_term_pics;
            if (tot_long_term > 32) {
               
               return (MWV207_NOK);
            }

            if (pic_param->pps.sps.num_long_term_ref_pics_sps > 1)
               len = mwv207_sw_num_bits(
                  pic_param->pps.sps.num_long_term_ref_pics_sps - 1);
            else
               len = 0;
            for (i = 0; i < tot_long_term; i++) {
               if (i < num_long_term_sps) {
                  tmp = mwv207_sw_get_bits(stream, len);
                  if (tmp == END_OF_STREAM)
                     return (MWV207_NOK);
               } else {
                  tmp = mwv207_sw_get_bits(
                     stream,
                     mwv207_sw_num_bits(
                        (1 << (pic_param->pps.sps
                                  .log2_max_pic_order_cnt_lsb_minus4
                               + 4))
                        - 1));
                  slice_header->poc_lsb_lt[i] = tmp;
                  if (tmp == END_OF_STREAM)
                     return (MWV207_NOK);
                  tmp = mwv207_sw_get_bits(stream, 1);
                  slice_header->used_by_curr_pic_lt[i] = tmp;
                  if (tmp == END_OF_STREAM)
                     return (MWV207_NOK);
               }

               tmp = mwv207_sw_get_bits(stream, 1);
               slice_header->delta_poc_msb_present_flag[i] = tmp;
               if (tmp == END_OF_STREAM)
                  return (MWV207_NOK);

               if (tmp) {
                  tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
                  if (tmp != MWV207_OK)
                     return (tmp);
               }
            }
         }

         *hw_skip_bits = stream->strm_buff_read_bits - read_bits;
         *hw_skip_bits -= stream->emul_byte_count * 8;
         stream->emul_byte_count += tmp_count;
         
         if (pic_param->pps.sps.sps_temporal_mvp_enabled_flag) {
            tmp = mwv207_sw_get_bits(stream, 1);
            if (tmp == END_OF_STREAM)
               return (MWV207_NOK);
         }
         if (pic_param->pps.sps.sample_adaptive_offset_enabled_flag) {
            tmp = mwv207_sw_get_bits(stream, 1);
            if (tmp == END_OF_STREAM)
               return (MWV207_NOK);
            tmp = mwv207_sw_get_bits(stream, 1);
            if (tmp == END_OF_STREAM)
               return (MWV207_NOK);
         }
         if (IS_HEVC_P_SLICE(slice_header->slice_type)
             || IS_HEVC_B_SLICE(slice_header->slice_type)) {
            tmp = mwv207_sw_get_bits(stream, 1);
            if (tmp == END_OF_STREAM)
               return (MWV207_NOK);

            if (tmp) { 
               tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
               if (tmp != MWV207_OK)
                  return (tmp);
               slice_header->num_ref_idx_l0_active =
                  value + 1;
               if (IS_HEVC_B_SLICE(slice_header->slice_type)) {
                  tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
                  if (tmp != MWV207_OK)
                     return (tmp);
                  slice_header->num_ref_idx_l1_active = value + 1;
               }
            } else {
               /* set num_ref_idx_l0_active and num_ref_idx_l1_active from
                * pic param set */
               slice_header->num_ref_idx_l0_active =
                  pic_param->pps.num_ref_idx_l0_default_active_minus1 + 1;
               slice_header->num_ref_idx_l1_active =
                  pic_param->pps.num_ref_idx_l1_default_active_minus1 + 1;
            }
            if (!IS_HEVC_B_SLICE(slice_header->slice_type))
               slice_header->num_ref_idx_l1_active = 0;
            uint32_t num_ref_frames = pic_param->NumPocStCurrBefore
               + pic_param->NumPocStCurrAfter + pic_param->NumPocLtCurr;
            
            
            
            
            //     flag = pic_param->ref[i].flags;
            
            
            
            
            

            if (pic_param->pps.lists_modification_present_flag
                && num_ref_frames > 1) {
               slice_header->rpl_modification_flag[0] =
                  mwv207_sw_get_bits(stream, 1);
               uint32_t len = mwv207_sw_num_bits(num_ref_frames - 1);
               if (slice_header->rpl_modification_flag[0]) {
                  for (i = 0; i < slice_header->num_ref_idx_l0_active; i++) {
                     tmp = mwv207_sw_get_bits(stream, len);
                     slice_header->list_entry_l0[i] = tmp;
                  }
               }
               if (IS_HEVC_B_SLICE(slice_header->slice_type)) {
                  slice_header->rpl_modification_flag[1] =
                     mwv207_sw_get_bits(stream, 1);
                  if (slice_header->rpl_modification_flag[1]) {
                     for (i = 0; i < slice_header->num_ref_idx_l1_active;
                          i++) {
                        tmp = mwv207_sw_get_bits(stream, len);
                        slice_header->list_entry_l1[i] = tmp;
                     }
                  }
               }
            }
         }
      } else {
         *hw_skip_bits = stream->strm_buff_read_bits - read_bits;
         *hw_skip_bits -= stream->emul_byte_count * 8;
         stream->emul_byte_count += tmp_count;
      }
   }
   return (MWV207_OK);
}

