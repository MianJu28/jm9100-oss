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

#include "mwv207_decoder_h264_slice_header.h"
#include "mwv207_decoder_hevc_exp_golomb.h"
#include "mwv207_decoder_sw_util.h"
#include <string.h>

#define MAX_NUM_VIEWS 2

/*------------------------------------------------------------------------------
         2. External compiler flags
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
         3. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
         4. Local function prototypes
------------------------------------------------------------------------------*/

static uint32_t ref_pic_list_reordering(struct mwv207_strm_data *,
                                        mwv207_ref_pic_list_reordering *,
                                        uint32_t,
                                        uint32_t,
                                        uint32_t);

static uint32_t pred_weight_table(struct mwv207_strm_data *,
                                  mwv207_h264_slice_header *,
                                  uint32_t mono_chrome);

static uint32_t
dec_ref_pic_marking(struct mwv207_strm_data *stream,
                    mwv207_dec_ref_pic_marking *p_dec_ref_pic_marking,
                    uint32_t idr_pic_flag,
                    uint32_t num_ref_frames);

int static mwv207_h264_decode_ref_pic_marking(mwv207_h264_slice_header *sl,
                                              struct mwv207_strm_data *stream,
                                              uint32_t idr_pic_flag);

/*------------------------------------------------------------------------------

         Function name: mwv207_h264_bsd_decode_slice_header

                  Functional description:
                                Decode slice header data from the stream.

                  Inputs:
                                stream       pointer to stream data structure
                                p_seq_param_set    pointer to active sequence
parameter set p_pic_param_set    pointer to active picture parameter set
                                p_nal_unit        pointer to current NAL unit
structure

                  Outputs:
                                p_slice_header    decoded data is stored here

                  Returns:
                                MWV207_OK       success
                                MWV207_NOK      invalid stream data or end of
stream

------------------------------------------------------------------------------*/

uint32_t
mwv207_h264_bsd_decode_slice_header(
   struct mwv207_strm_data *stream,
   struct mwv207_h264_picture_desc *pic_param,
   //  VASliceParameterBufferH264 *slice_param,
   mwv207_h264_slice_header *p_slice_header,
   mwv207_h264_nal_unit *p_nal_unit)
{

   uint32_t tmp, i, value;
   int32_t itmp;
   uint32_t strm_pos;

   (void)memset(p_slice_header, 0, sizeof(mwv207_h264_slice_header));

   tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
   if (tmp != MWV207_OK)
      return (tmp);
   p_slice_header->first_mb_in_slice = value;

   tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
   if (tmp != MWV207_OK)
      return (tmp);
   p_slice_header->slice_type = value;

   tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
   if (tmp != MWV207_OK)
      return (tmp);
   p_slice_header->pic_parameter_set_id = value;

   i = 0;
   while ((1 << (pic_param->pps.sps.log2_max_frame_num_minus4 + 4)) >> i)
      i++;
   i--;
   SLIECE_DEBUG(
      "1 pic_param->pps.sps.log2_max_frame_num_minus4 :%d ,i : %d \n",
      pic_param->pps.sps.log2_max_frame_num_minus4,
      i);

   tmp = mwv207_sw_get_bits(stream, i);
   if (tmp == END_OF_STREAM)
      return (MWV207_NOK);
   p_slice_header->frame_num = tmp;

   SLIECE_DEBUG("frame_mbs_only_flag :%d \n",
                pic_param->pps.sps.frame_mbs_only_flag);
   if (!pic_param->pps.sps.frame_mbs_only_flag) {
      SLIECE_DEBUG("i : %d \n", i);
      tmp = mwv207_sw_get_bits(stream, 1);
      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);
      p_slice_header->field_pic_flag = tmp;
      if (p_slice_header->field_pic_flag) {
         SLIECE_DEBUG("i : %d \n", i);
         tmp = mwv207_sw_get_bits(stream, 1);
         if (tmp == END_OF_STREAM)
            return (MWV207_NOK);
         p_slice_header->bottom_field_flag = tmp;
         p_slice_header->picture_structure = PICT_TOP_FIELD + tmp;
      } else {
         p_slice_header->picture_structure = PICT_FRAME;
      }
   } else {
      p_slice_header->picture_structure = PICT_FRAME;
   }

   if (p_slice_header->picture_structure == PICT_FRAME) {
      p_slice_header->curr_pic_num = p_slice_header->frame_num;
      p_slice_header->max_pic_num = 1
         << (pic_param->pps.sps.log2_max_frame_num_minus4 + 4);
   } else {
      p_slice_header->curr_pic_num = 2 * p_slice_header->frame_num + 1;
      p_slice_header->max_pic_num = 1
         << (pic_param->pps.sps.log2_max_frame_num_minus4 + 4 + 1);
   }

   if (IS_IDR_NAL_UNIT(p_nal_unit)) {
      tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
      if (tmp != MWV207_OK)
         return (tmp);
      p_slice_header->idr_pic_id = value;
   }

   strm_pos = stream->strm_buff_read_bits;
   stream->emul_byte_count = 0;
   if (pic_param->pps.sps.pic_order_cnt_type == 0) {
      /* log2(max_pic_order_cnt_lsb) -> num bits to represent
       * pic_order_cnt_lsb */
      i = 0;
      while ((1 << (pic_param->pps.sps.log2_max_pic_order_cnt_lsb_minus4 + 4))
             >> i)
         i++;
      i--;
      SLIECE_DEBUG("i : %d \n", i);
      tmp = mwv207_sw_get_bits(stream, i);
      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);
      p_slice_header->pic_order_cnt_lsb = tmp;

      if (pic_param->pps.bottom_field_pic_order_in_frame_present_flag
          && !p_slice_header->field_pic_flag) {
         tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
         if (tmp != MWV207_OK)
            return (tmp);
         p_slice_header->delta_pic_order_cnt_bottom = itmp;
      }
   }

   if ((pic_param->pps.sps.pic_order_cnt_type == 1)
       && !pic_param->pps.sps.delta_pic_order_always_zero_flag) {
      tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
      if (tmp != MWV207_OK)
         return (tmp);
      p_slice_header->delta_pic_order_cnt[0] = itmp;

      if (pic_param->pps.bottom_field_pic_order_in_frame_present_flag
          && !p_slice_header->field_pic_flag) {
         tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
         if (tmp != MWV207_OK)
            return (tmp);
         p_slice_header->delta_pic_order_cnt[1] = itmp;
      }
   }

   p_slice_header->poc_length = stream->strm_buff_read_bits - strm_pos;
   p_slice_header->poc_length_hw =
      stream->strm_buff_read_bits - strm_pos - 8 * stream->emul_byte_count;
   if (pic_param->pps.redundant_pic_cnt_present_flag) {
      tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
      if (tmp != MWV207_OK)
         return (tmp);
      p_slice_header->redundant_pic_cnt = value;
   }

   if (IS_B_SLICE(p_slice_header->slice_type)) {
      SLIECE_DEBUG("i : %d \n", i);
      tmp = mwv207_sw_get_bits(stream, 1);
      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);
      p_slice_header->direct_spatial_mv_pred_flag = tmp;
   }
   if (IS_P_SLICE(p_slice_header->slice_type)
       || IS_B_SLICE(p_slice_header->slice_type)) {
      SLIECE_DEBUG("i : %d \n", i);
      tmp = mwv207_sw_get_bits(stream, 1);
      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);
      p_slice_header->num_ref_idx_active_override_flag = tmp;

      if (p_slice_header->num_ref_idx_active_override_flag) {
         tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
         if (tmp != MWV207_OK)
            return (tmp);
         p_slice_header->num_ref_idx_l0_active = value + 1;

         if (IS_B_SLICE(p_slice_header->slice_type)) {
            tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
            if (tmp != MWV207_OK)
               return (tmp);
            p_slice_header->num_ref_idx_l1_active = value + 1;
         }
      }

      else {
         /* if value (minus1) in picture parameter set exceeds 15 and
          * current picture is not field picture, it should have been
          * overridden here */
         p_slice_header->num_ref_idx_l0_active =
            pic_param->num_ref_idx_l0_active_minus1 + 1;
         p_slice_header->num_ref_idx_l1_active =
            pic_param->num_ref_idx_l1_active_minus1 + 1;
      }
   }

   if (!IS_I_SLICE(p_slice_header->slice_type)) {
      tmp = ref_pic_list_reordering(
         stream,
         &p_slice_header->ref_pic_list_reordering,
         p_slice_header->num_ref_idx_l0_active,
         p_slice_header->field_pic_flag
            ? 2 * (1 << (pic_param->pps.sps.log2_max_frame_num_minus4 + 4))
            : (1 << (pic_param->pps.sps.log2_max_frame_num_minus4 + 4)),
         p_nal_unit->nal_unit_type == NAL_CODED_SLICE_EXT);
      if (tmp != MWV207_OK)
         return (tmp);
   }
   if (IS_B_SLICE(p_slice_header->slice_type)) {
      tmp = ref_pic_list_reordering(
         stream,
         &p_slice_header->ref_pic_list_reordering_l1,
         p_slice_header->num_ref_idx_l1_active,
         p_slice_header->field_pic_flag
            ? 2 * (1 << (pic_param->pps.sps.log2_max_frame_num_minus4 + 4))
            : (1 << (pic_param->pps.sps.log2_max_frame_num_minus4 + 4)),
         p_nal_unit->nal_unit_type == NAL_CODED_SLICE_EXT);
      if (tmp != MWV207_OK)
         return (tmp);
   }

   if ((pic_param->pps.weighted_pred_flag
        && IS_P_SLICE(p_slice_header->slice_type))
       || (pic_param->pps.weighted_bipred_idc == 1
           && IS_B_SLICE(p_slice_header->slice_type))) {
      SLIECE_DEBUG("chroma_format_idc : %d \n",
                   pic_param->pps.sps.chroma_format_idc);
      tmp = pred_weight_table(
         stream, p_slice_header, pic_param->pps.sps.chroma_format_idc == 0);
      if (tmp != MWV207_OK)
         return (tmp);
   }

   if (p_nal_unit->nal_ref_idc != 0) {
      SLIECE_DEBUG("num_ref_frames : %d \n", pic_param->num_ref_frames);
      tmp = mwv207_h264_decode_ref_pic_marking(
         p_slice_header, stream, IS_IDR_NAL_UNIT(p_nal_unit));
      if (tmp != MWV207_OK)
         return (tmp);
   } else
      p_slice_header->dec_ref_pic_marking.strm_len = 0;
   SLIECE_DEBUG("entropy_coding_mode_flag: %d \n",
                pic_param->pps.entropy_coding_mode_flag);
   if (pic_param->pps.entropy_coding_mode_flag
       && !IS_I_SLICE(p_slice_header->slice_type)) {
      SLIECE_DEBUG("mwv207_hevc_decode_exp_golomb_unsigned \n");
      tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
      if (tmp != MWV207_OK) {
         return (MWV207_NOK);
      }
      p_slice_header->cabac_init_idc = value;
   }

   /* decode slice_qp_delta and check that initial QP for the slice will be on
    * the range [0, 51] */
   SLIECE_DEBUG("mwv207_hevc_decode_exp_golomb_unsigned \n");
   tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
   if (tmp != MWV207_OK)
      return (tmp);
   p_slice_header->slice_qp_delta = itmp;
   itmp += (int32_t)(pic_param->pps.pic_init_qp_minus26 + 26);
   SLIECE_DEBUG("deblocking_filter_control_present_flag: %d \n",
                pic_param->pps.deblocking_filter_control_present_flag);
   if (pic_param->pps.deblocking_filter_control_present_flag) {
      SLIECE_DEBUG("mwv207_hevc_decode_exp_golomb_unsigned \n");
      tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
      if (tmp != MWV207_OK)
         return (tmp);
      p_slice_header->disable_deblocking_filter_idc = value;

      if (p_slice_header->disable_deblocking_filter_idc != 1) {
         tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
         if (tmp != MWV207_OK)
            return (tmp);
         p_slice_header->slice_alpha_c0_offset = itmp /* * 2 */;

         tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
         if (tmp != MWV207_OK)
            return (tmp);
         p_slice_header->slice_beta_offset = itmp /* * 2 */;
      }
   }
#if 0
    if ((pic_param->num_slice_groups_minus1 > 0) &&
        (pic_param->slice_group_map_type >= 3) &&
        (pic_param->slice_group_map_type <= 5)) {
        /* set tmp to number of bits used to represent slice_group_change_cycle
         * in the stream */
        tmp = NumSliceGroupChangeCycleBits(pic_size_in_mbs,
                                           pic_param->slice_group_change_rate_minus1 + 1);
        value = mwv207_sw_get_bits(stream, tmp);
        if (value == END_OF_STREAM)
            return (MWV207_NOK);
        p_slice_header->slice_group_change_cycle = value;

    }
#endif
   return (MWV207_OK);
}

/*------------------------------------------------------------------------------

         Function: ref_pic_list_reordering

                  Functional description:
                                Decode reference picture list reordering
syntax elements from the stream. Max number of reordering commands is
num_ref_idx_active.

                  Inputs:
                                stream       pointer to stream data structure
                                num_ref_idx_active number of active reference
indices to be used for current slice max_pic_num       max_frame_num from the
active SPS

                  Outputs:
                                p_ref_pic_list_reordering   decoded data is
stored here

                  Returns:
                                MWV207_OK       success
                                MWV207_NOK      invalid stream data

------------------------------------------------------------------------------*/

uint32_t
ref_pic_list_reordering(
   struct mwv207_strm_data *stream,
   mwv207_ref_pic_list_reordering *p_ref_pic_list_reordering,
   uint32_t num_ref_idx_active,
   uint32_t max_pic_num,
   uint32_t mvc)
{

   uint32_t tmp, value, i;
   uint32_t command;

   SLIECE_DEBUG("i : %d \n", 1);
   tmp = mwv207_sw_get_bits(stream, 1);
   if (tmp == END_OF_STREAM)
      return (MWV207_NOK);

   p_ref_pic_list_reordering->ref_pic_list_reordering_flag_l0 = tmp;

   if (p_ref_pic_list_reordering->ref_pic_list_reordering_flag_l0) {
      i = 0;

      do {
         if (i > num_ref_idx_active) {
            return (MWV207_NOK);
         }

         tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &command);
         if (tmp != MWV207_OK)
            return (tmp);
         if (command > (mvc ? 5 : 3)) {
            return (MWV207_NOK);
         }

         p_ref_pic_list_reordering->command[i].reordering_of_pic_nums_idc =
            command;

         if ((command == 0) || (command == 1)) {
            tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
            if (tmp != MWV207_OK)
               return (tmp);
            if (value >= max_pic_num) {
               return (MWV207_NOK);
            }
            p_ref_pic_list_reordering->command[i].abs_diff_pic_num =
               value + 1;
         } else if (command == 2) {
            tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
            if (tmp != MWV207_OK)
               return (tmp);
            p_ref_pic_list_reordering->command[i].long_term_pic_num = value;
         } else if (command == 4 || command == 5) {
            tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
            if (tmp != MWV207_OK)
               return (tmp);
            p_ref_pic_list_reordering->command[i].abs_diff_view_idx =
               value + 1;
         }
         i++;
      } while (command != 3);

      /* there shall be at least one reordering command if
       * ref_pic_list_reordering_flag_l0 was set */
      if (i == 1) {
         return (MWV207_NOK);
      }
   }

   return (MWV207_OK);
}

/*------------------------------------------------------------------------------

         Function: dec_ref_pic_marking

                  Functional description:
                                Decode decoded reference picture marking
syntax elements from the stream.

                  Inputs:
                                stream       pointer to stream data structure
                                nal_unit_type     type of the current NAL unit
                                num_ref_frames    max number of reference
frames from the active SPS

                  Outputs:
                                p_dec_ref_pic_marking   decoded data is stored
here

                  Returns:
                                MWV207_OK       success
                                MWV207_NOK      invalid stream data

------------------------------------------------------------------------------*/

uint32_t
dec_ref_pic_marking(struct mwv207_strm_data *stream,
                    mwv207_dec_ref_pic_marking *p_dec_ref_pic_marking,
                    uint32_t idr_pic_flag,
                    uint32_t num_ref_frames)
{

   uint32_t tmp, value;
   uint32_t i;
   uint32_t operation;
   uint32_t strm_pos;

   /* variables for error checking purposes, store number of memory
    * management operations of certain type */
   uint32_t num4 = 0, num5 = 0, num6 = 0, num1to3 = 0;

   strm_pos = stream->strm_buff_read_bits;
   stream->emul_byte_count = 0;

   if (idr_pic_flag) {
      SLIECE_DEBUG("i : %d \n", 1);
      tmp = mwv207_sw_get_bits(stream, 1);
      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);
      p_dec_ref_pic_marking->no_output_of_prior_pics_flag = tmp;
      SLIECE_DEBUG("i : %d \n", 1);
      tmp = mwv207_sw_get_bits(stream, 1);
      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);
      p_dec_ref_pic_marking->long_term_reference_flag = tmp;
      if (!num_ref_frames
          && p_dec_ref_pic_marking->long_term_reference_flag) {
         return (MWV207_NOK);
      }
   } else {
      SLIECE_DEBUG("i : %d \n", 1);
      tmp = mwv207_sw_get_bits(stream, 1);
      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);
      p_dec_ref_pic_marking->adaptive_ref_pic_marking_mode_flag = tmp;
      if (p_dec_ref_pic_marking->adaptive_ref_pic_marking_mode_flag) {
         i = 0;
         do {
            /* see explanation of the MAX_NUM_MMC_OPERATIONS in
             * slice_header.h */
            if (i > (2 * num_ref_frames + 2)) {
               return (MWV207_NOK);
            }

            tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &operation);
            if (tmp != MWV207_OK)
               return (tmp);
            if (operation > 6) {
               return (MWV207_NOK);
            }

            p_dec_ref_pic_marking->operation[i]
               .memory_management_control_operation = operation;
            if ((operation == 1) || (operation == 3)) {
               tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
               if (tmp != MWV207_OK)
                  return (tmp);
               p_dec_ref_pic_marking->operation[i].difference_of_pic_nums =
                  value + 1;
            }
            if (operation == 2) {
               tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
               if (tmp != MWV207_OK)
                  return (tmp);
               p_dec_ref_pic_marking->operation[i].long_term_pic_num = value;
            }
            if ((operation == 3) || (operation == 6)) {
               tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
               if (tmp != MWV207_OK)
                  return (tmp);
               p_dec_ref_pic_marking->operation[i].long_term_frame_idx =
                  value;
            }
            if (operation == 4) {
               tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
               if (tmp != MWV207_OK)
                  return (tmp);

               if (value > num_ref_frames) {
                  return (MWV207_NOK);
               }
               if (value == 0) {
                  p_dec_ref_pic_marking->operation[i]
                     .max_long_term_frame_idx = NO_LONG_TERM_FRAME_INDICES;
               } else {
                  p_dec_ref_pic_marking->operation[i]
                     .max_long_term_frame_idx = value - 1;
               }
               num4++;
            }
            if (operation == 5) {
               num5++;
            }
            if (operation && operation <= 3)
               num1to3++;
            if (operation == 6)
               num6++;

            i++;
         } while (operation != 0);

         if (num4 > 1 || num5 > 1 || num6 > 1 || (num1to3 && num5))
            return (MWV207_NOK);
      }
   }
   SLIECE_DEBUG(
      "strm_buff_read_bits : %d, strm_pos :%d ,emul_byte_count: %d \n",
      stream->strm_buff_read_bits,
      strm_pos,
      stream->emul_byte_count);
   p_dec_ref_pic_marking->strm_len =
      stream->strm_buff_read_bits - strm_pos - 8 * stream->emul_byte_count;

   return (MWV207_OK);
}

static int
mwv207_h264_decode_ref_pic_marking(mwv207_h264_slice_header *sl,
                                   struct mwv207_strm_data *stream,
                                   uint32_t idr_pic_flag)
{
   int i;
   MMCO *mmco = sl->mmco;
   int nb_mmco = 0;

   uint32_t strm_pos;
   strm_pos = stream->strm_buff_read_bits;
   stream->emul_byte_count = 0;

   if (idr_pic_flag) { 
      mwv207_sw_get_bits(stream, 1); 
      if (mwv207_sw_get_bits(stream, 1)) {
         mmco[0].opcode = MMCO_LONG;
         mmco[0].long_arg = 0;
         nb_mmco = 1;
      }
      sl->explicit_ref_marking = 1;
   } else {
      uint32_t value;
      sl->explicit_ref_marking = mwv207_sw_get_bits(stream, 1);
      if (sl->explicit_ref_marking) {
         for (i = 0; i < MAX_MMCO_COUNT; i++) {
            MMCOOpcode opcode;
            mwv207_hevc_decode_exp_golomb_unsigned(stream, &opcode);

            mmco[i].opcode = opcode;
            if (opcode == MMCO_SHORT2UNUSED || opcode == MMCO_SHORT2LONG) {
               mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
               mmco[i].short_pic_num =
                  (sl->curr_pic_num - value - 1) & (sl->max_pic_num - 1);
            }
            if (opcode == MMCO_SHORT2LONG || opcode == MMCO_LONG2UNUSED
                || opcode == MMCO_LONG || opcode == MMCO_SET_MAX_LONG) {
               mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
               unsigned int long_arg = value;
               
               
               
               
               
               
               
               
               
               
               
               
               mmco[i].long_arg = long_arg;
            }

            
            
            
            
            
            
            
            if (opcode == MMCO_END)
               break;
         }
         nb_mmco = i;
      }
   }

   sl->nb_mmco = nb_mmco;

   sl->dec_ref_pic_marking.strm_len =
      stream->strm_buff_read_bits - strm_pos - 8 * stream->emul_byte_count;

   return 0;
}

/*------------------------------------------------------------------------------

         Function: pred_weight_table

                  Functional description:

                  Inputs:

                  Outputs:

                  Returns:

------------------------------------------------------------------------------*/

uint32_t
pred_weight_table(struct mwv207_strm_data *stream,
                  mwv207_h264_slice_header *p_slice_header,
                  uint32_t mono_chrome)
{

   uint32_t tmp, value, i;
   int32_t itmp;

   tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
   if (!mono_chrome)
      tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
   for (i = 0; i < p_slice_header->num_ref_idx_l0_active; i++) {
      SLIECE_DEBUG("i : %d \n", 1);
      tmp = mwv207_sw_get_bits(stream, 1);
      if (tmp == 1) {
         tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
         tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
      }
      if (!mono_chrome) {
         SLIECE_DEBUG("i : %d \n", 1);
         tmp = mwv207_sw_get_bits(stream, 1);
         if (tmp == 1) {
            tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
            tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
            tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
            tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
         }
      }
   }

   if (IS_B_SLICE(p_slice_header->slice_type)) {
      for (i = 0; i < p_slice_header->num_ref_idx_l1_active; i++) {
         SLIECE_DEBUG("i : %d \n", 1);
         tmp = mwv207_sw_get_bits(stream, 1);
         if (tmp == 1) {
            tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
            tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
         }
         if (!mono_chrome) {
            SLIECE_DEBUG("i : %d \n", 1);
            tmp = mwv207_sw_get_bits(stream, 1);
            if (tmp == 1) {
               tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
               tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
               tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
               tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
            }
         }
      }
   }

   return (MWV207_OK);
}

uint32_t
mwv207_h264_bsd_Ref_idx_override(struct mwv207_strm_data *stream,
                                 struct mwv207_h264_picture_desc *pic_param,
                                 uint32_t *num_ref_idx_active_override_flag)
{

   uint32_t tmp, i, value;
   int32_t itmp;
   uint32_t strm_pos;
   mwv207_h264_slice_header slice_header = {0};
   mwv207_h264_slice_header *p_slice_header;

   p_slice_header = &slice_header;

   tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
   if (tmp != MWV207_OK)
      return (tmp);
   p_slice_header->first_mb_in_slice = value;

   tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
   if (tmp != MWV207_OK)
      return (tmp);
   p_slice_header->slice_type = value;

   if (IS_I_SLICE(p_slice_header->slice_type)) {
      *num_ref_idx_active_override_flag = 0;
      return (MWV207_OK);
   }

   tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
   if (tmp != MWV207_OK)
      return (tmp);
   p_slice_header->pic_parameter_set_id = value;

   i = 0;
   while ((1 << (pic_param->pps.sps.log2_max_frame_num_minus4 + 4)) >> i)
      i++;
   i--;
   SLIECE_DEBUG("pic_param->pps.sps.log2_max_frame_num_minus4 :%d ,i : %d \n",
                pic_param->pps.sps.log2_max_frame_num_minus4,
                i);

   tmp = mwv207_sw_get_bits(stream, i);
   if (tmp == END_OF_STREAM)
      return (MWV207_NOK);
   p_slice_header->frame_num = tmp;

   if (!pic_param->pps.sps.frame_mbs_only_flag) {
      SLIECE_DEBUG("i : %d \n", 1);
      tmp = mwv207_sw_get_bits(stream, 1);
      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);
      p_slice_header->field_pic_flag = tmp;
      if (p_slice_header->field_pic_flag) {
         SLIECE_DEBUG("i : %d \n", 1);
         tmp = mwv207_sw_get_bits(stream, 1);
         if (tmp == END_OF_STREAM)
            return (MWV207_NOK);
         p_slice_header->bottom_field_flag = tmp;
      }
   }

   strm_pos = stream->strm_buff_read_bits;
   stream->emul_byte_count = 0;
   if (pic_param->pps.sps.pic_order_cnt_type == 0) {
      /* log2(max_pic_order_cnt_lsb) -> num bits to represent
       * pic_order_cnt_lsb */
      i = 0;
      while ((1 << (pic_param->pps.sps.log2_max_pic_order_cnt_lsb_minus4 + 4))
             >> i)
         i++;
      i--;
      SLIECE_DEBUG("i : %d \n", 1);
      tmp = mwv207_sw_get_bits(stream, i);
      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);
      p_slice_header->pic_order_cnt_lsb = tmp;

      if (pic_param->pps.bottom_field_pic_order_in_frame_present_flag
          && !p_slice_header->field_pic_flag) {
         tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
         if (tmp != MWV207_OK)
            return (tmp);
         p_slice_header->delta_pic_order_cnt_bottom = itmp;
      }
   }

   if ((pic_param->pps.sps.pic_order_cnt_type == 1)
       && !pic_param->pps.sps.delta_pic_order_always_zero_flag) {
      tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
      if (tmp != MWV207_OK)
         return (tmp);
      p_slice_header->delta_pic_order_cnt[0] = itmp;

      if (pic_param->pps.bottom_field_pic_order_in_frame_present_flag
          && !p_slice_header->field_pic_flag) {
         tmp = mwv207_hevc_decode_exp_golomb_signed(stream, &itmp);
         if (tmp != MWV207_OK)
            return (tmp);
         p_slice_header->delta_pic_order_cnt[1] = itmp;
      }
   }

   p_slice_header->poc_length = stream->strm_buff_read_bits - strm_pos;
   p_slice_header->poc_length_hw =
      stream->strm_buff_read_bits - strm_pos - 8 * stream->emul_byte_count;
   if (pic_param->pps.redundant_pic_cnt_present_flag) {
      tmp = mwv207_hevc_decode_exp_golomb_unsigned(stream, &value);
      if (tmp != MWV207_OK)
         return (tmp);
      p_slice_header->redundant_pic_cnt = value;
   }

   if (IS_B_SLICE(p_slice_header->slice_type)) {
      SLIECE_DEBUG("i : %d \n", 1);
      tmp = mwv207_sw_get_bits(stream, 1);
      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);
      p_slice_header->direct_spatial_mv_pred_flag = tmp;
   }
   if (IS_P_SLICE(p_slice_header->slice_type)
       || IS_B_SLICE(p_slice_header->slice_type)) {
      SLIECE_DEBUG("i : %d \n", 1);
      tmp = mwv207_sw_get_bits(stream, 1);
      if (tmp == END_OF_STREAM)
         return (MWV207_NOK);
      *num_ref_idx_active_override_flag = tmp;

   }

   return (MWV207_OK);
}

