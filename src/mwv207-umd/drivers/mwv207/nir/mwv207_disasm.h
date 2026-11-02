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

#ifndef H_MWV207_ASM
#define H_MWV207_ASM

#define PRINT(format, ...) print_with_update_length(print_length, format, ##__VA_ARGS__)

enum dst_addr_reg_type {
   DST_ADDR_REG_TYPE_NONE,
   DST_ADDR_REG_TYPE_A0,
   DST_ADDR_REG_TYPE_B0,
};

enum kinds_of_inst {
   NO_OPND_INST = 0,
   TRI_SRCS_ALU_INST,
   TRI_SRCS_CC_ALU_INST,
   BIN_SRCS_SRC0_SRC1_ALU_INST,
   BIN_SRCS_SRC0_SRC1_ALU_CC_INST,
   BIN_SRCS_SRC0_SRC2_ALU_INST,
   BIN_SRCS_SRC1_SRC2_ALU_INST,
   UNA_SRC_SRC0_ALU_INST,
   UNA_SRC_SRC1_ALU_INST,
   UNA_SRC_SRC2_ALU_INST,
   PACK_INST,
   SAMPLE_INST,
   SAMPLE_EXT_INST,
   LOAD_INST,
   IMG_LOAD_INST,
   STORE_INST,
   IMG_STORE_INST,
   IMG_ATOM_INST,
   STORE_ATTR_INST,
   SELECT_MAP_INST,
   DIRECT_BRANCH0_INST,
   DIRECT_BRANCH1_INST,
   INDIRECT_BRANCH_INST,
   DIRECT_CALL_INST,
   INDIRECT_CALL_INST,
   LOOP_INST,
   EMIT_INST,
   CONV_INST,
   CMPLX_INST,
   SCATTER_INST
};

struct instr {
   union {

      struct {

         uint32_t opc                             : 6;
         uint32_t cond                            : 5;
         uint32_t sat                             : 1;
         uint32_t dst_use                         : 1;
         uint32_t dst_amode                       : 3;
         uint32_t dst_reg                         : 7;
         uint32_t dst_comps                       : 4;
         uint32_t tex_id                          : 5;

         uint32_t tex_amode                       : 3;
         uint32_t tex_swiz                        : 8;
         uint32_t src0_use                        : 1;
         uint32_t src0_reg                        : 9;
         uint32_t type_bit2                       : 1;
         uint32_t src0_swiz                       : 8;
         uint32_t src0_neg                        : 1;
         uint32_t src0_abs                        : 1;

         uint32_t src0_amode                      : 3;
         uint32_t src0_rgroup                     : 3;
         uint32_t src1_use                        : 1;
         uint32_t src1_reg                        : 9;
         uint32_t opcode_bit6                     : 1;
         uint32_t src1_swiz                       : 8;
         uint32_t src1_neg                        : 1;
         uint32_t src1_abs                        : 1;
         uint32_t src1_amode                      : 3;
         uint32_t type_bit01                      : 2;

         union {
            struct {
               uint32_t src1_rgroup               : 3;
               uint32_t src2_use                  : 1;
               uint32_t src2_reg                  : 9;
               uint32_t sel_0                     : 1;
               uint32_t src2_swiz                 : 8;
               uint32_t src2_neg                  : 1;
               uint32_t src2_abs                  : 1;
               uint32_t sel_1                     : 1;
               uint32_t src2_amode                : 3;
               uint32_t src2_rgroup               : 3;
               uint32_t dst_full                  : 1;
            };
            uint32_t dword3;
         };
      };

      struct {

         union {
            struct __attribute__((__packed__)) {
               uint32_t base_opcode_bit0_5                     : 6;
               uint32_t cond_opcode                            : 5;
               uint32_t is_result_sat                          : 1;
               uint32_t is_dst_valid                           : 1;
               uint32_t dst_rel_addr                           : 3;
               uint32_t dst_reg_no_bit0_6                      : 7;
               uint32_t write_mask                             : 4;
               uint32_t is_inf_x0_to_zero_or_mult_inf          : 1;
               uint32_t reserved0                              : 4;
            };
            struct __attribute__((__packed__)) {
               uint32_t pad0_for_normal_inst                   : 6;
               uint32_t is_big_endian                          : 1;
               uint32_t is_unallocate                          : 1;
               uint32_t is_end_of_BB                           : 1;
               uint32_t pad1_for_normal_inst                   : 18;
               uint32_t sampler_slot_for_sample_inst           : 5;
            };
         };

         union {
            struct __attribute__((__packed__)) {
               uint32_t round_mode                             : 2;
               uint32_t pack_mode_or_vec_size_bit0             : 1;
               uint32_t sh_stage_client                        : 2;
               uint32_t reversed5                              : 1;
               uint32_t is_end_of_BB_cc_alu_or_attr_layout     : 1;
               uint32_t is_skip_for_helper_kickoff             : 1;
               uint32_t vec_size_bit1                          : 1;
               uint32_t inst_type_bit3_or_img_vec_size_bit0    : 1;
               uint32_t is_denorm_or_vec_size_bit1_for_ls_inst : 1;
               uint32_t is_src0_valid                          : 1;
               uint32_t src0_reg_no                            : 9;
               uint32_t inst_type_bit0                         : 1;
               uint32_t src0_swizzle                           : 8;
               uint32_t is_src0_mod_neg                        : 1;
               uint32_t is_src0_mod_abs                        : 1;
            };
            struct __attribute__((__packed__)) {
               uint32_t sampler_rel_addr                       : 3;
               uint32_t src_select_or_sampler_swizzle          : 8;
            };
            struct __attribute__((__packed__)) {
               uint32_t pad0_for_conv_inst                     : 5;
               uint32_t conv_inst_is_dst_vec_size_bit1         : 1;
               uint32_t conv_inst_is_src_vec_size_bit1         : 1;
               uint32_t conv_inst_is_dst_pack_or_vec_size_bit0 : 1;
               uint32_t conv_inst_is_src_pack_or_vec_size_bit0 : 1;
            };
            struct __attribute__((__packed__)) {
               uint32_t pad0_for_cmplx_inst                    : 3;
               uint32_t cmplx_inst_sub_opcode                  : 4;
            };
            struct __attribute__((__packed__)) {
               uint32_t pad0_for_select_map_inst               : 3;
               uint32_t range_to_match                         : 4;
               uint32_t pad1_for_select_map_inst               : 3;
               uint32_t is_component_select                    : 2;
            };
            struct __attribute__((__packed__)) {
               union {
                  struct __attribute__((__packed__)) {
                     uint32_t pad0_for_load_store_inst               : 3;
                     uint32_t offset_left_shift                      : 3;
                     uint32_t is_offset_x3                           : 1;
                  };
                  struct __attribute__((__packed__)) {
                     uint32_t pad0_for_img_atom_inst                 : 3;
                     uint32_t atomic_mode                            : 3;
                     uint32_t is_3d_img_mode                         : 1;
                  };
               };
               uint32_t is_access_local_storage                : 1;
            };
            struct __attribute__((__packed__)) {
               uint32_t pad2_for_emit_inst                     : 3;
               uint32_t is_need_restart_prim                   : 1;
               uint32_t is_no_jmp_to_end_on_max_vtx_cnt        : 1;
            };
         };

         uint32_t src0_rel_addr                          : 3;
         uint32_t src0_type                              : 3;
         uint32_t is_src1_valid                          : 1;
         uint32_t src1_reg_no                            : 9;
         uint32_t base_opcode_bit6                       : 1;
         uint32_t src1_swizzle                           : 8;
         uint32_t is_src1_mod_neg                        : 1;
         uint32_t is_src1_mod_abs                        : 1;
         uint32_t src1_rel_addr                          : 3;
         uint32_t inst_type_bit1_2                       : 2;

         union {
            struct __attribute__((__packed__)) {
               uint32_t src1_type                              : 3;
               uint32_t is_src2_valid                          : 1;
               uint32_t src2_reg_no                            : 9;
               uint32_t dst_reg_no_bit7_or_thread_type_bit0    : 1;
               uint32_t src2_swizzle                           : 8;
               uint32_t is_src2_mod_neg                        : 1;
               uint32_t is_src2_mod_abs                        : 1;
               uint32_t dst_reg_no_bit8_or_thread_type_bit1    : 1;
               uint32_t src2_rel_addr                          : 3;
               uint32_t src2_type                              : 3;
               uint32_t dst_type                               : 1;
            };

            struct __attribute__((__packed__)) {
               uint32_t pad0_for_branch_inst                    : 4;
               uint32_t loop_optype                             : 1;
               uint32_t pad1_for_branch_inst                    : 2;
               uint32_t branch_target_or_call_target            : 20;
            };

            struct __attribute__((__packed__)) {
               uint32_t pad0_for_branch1_inst                   : 4;
               uint32_t src2_branch_target_bit0_8               : 9;
               uint32_t pad1_for_branch1_inst                   : 1;
               uint32_t src2_branch_target_bit9_18              : 10;
               uint32_t pad2_for_branch1_inst                   : 1;
               uint32_t src2_branch_target_bit19                : 1;
               uint32_t imm_type                                : 2;
            };
            struct __attribute__((__packed__)) {
               uint32_t pad3_for_nomal_inst                     : 4;
               uint32_t ext_opcode                              : 8;
            };
         };

      };

   };
};

struct mwv207_inst_ctrl {
   uint32_t cond_opcode;
   uint32_t inst_type;
   uint32_t rounding_mode;
   bool is_has4_rounding_mode;
   bool pack_mode_or_vec_size_bit0;
   bool vec_size_bit1;
   bool is_denorm;
   uint32_t thread_type;
   bool is_skip_for_helper_kickoff;
   bool is_result_sat;
   bool is_end_of_BB;
   bool is_force_gen;
   union {
      struct {
         bool is_access_local_storage;
         bool is_under_evis_mode;
         bool is_big_endian;
         bool is_unallocate;

         union {
            struct {
               uint32_t        offset_left_shift;
               bool            is_offset_x3;
            } load_store_ctrl;

            struct {
               uint32_t        atomic_mode;
               bool            is_3d_img_mode;
            } img_atom_ctrl;
         } u;
      } memory_access_ctrl;

      struct {
         uint32_t                range_to_match;
         bool                    is_component_select;
      } select_map_ctrl;

      struct {
         uint32_t        sh_stage_client;
         uint32_t        attr_layout;
      } load_store_attr_ctrl;

      struct {
         bool            is_need_restart_prim;
         bool            is_no_jmp_to_end_on_max_vtx_cnt;
      } emit_ctrl;

      struct {
         bool            is_dst_pack_or_vec_size_bit0;
         bool            is_dst_vec_size_bit1;
         bool            is_src_pack_or_vec_size_bit0;
         bool            is_src_vec_size_bit1;
      } conv_ctrl;

      uint32_t            src_select;

      uint32_t            loop_optype;

      bool                is_inf_x0_to_zero;
   } u;
};

void
mwv207_disasm(uint32_t *dwords, int sizedwords);

#endif
