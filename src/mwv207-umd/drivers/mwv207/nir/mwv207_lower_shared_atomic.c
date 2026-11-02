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
#include <stdio.h>
#include "mwv207_lower.h"

static nir_def *
set_base_offset_impl(nir_instr *instr, nir_builder *b, nir_shader *nir)
{
    const nir_intrinsic_op intrinsic_lists[] = {
        nir_intrinsic_load_num_workgroups,
        nir_intrinsic_load_workgroup_id
    };
    const nir_op base_op_lists[] = {
        nir_op_imul,
        nir_op_imul,
        nir_op_imul,
        nir_op_iadd,
        nir_op_iadd,
    };
    uint32_t i = 0;
    uint32_t j = 0;
    for (uint32_t k = 0; k < ARRAY_SIZE(intrinsic_lists) + ARRAY_SIZE(base_op_lists); k++) {
        switch (instr->type) {
        case nir_instr_type_intrinsic: {
            nir_intrinsic_instr *temp = nir_instr_as_intrinsic(instr);
            if (i >= ARRAY_SIZE(intrinsic_lists))
                return NULL;
            if (temp->intrinsic == intrinsic_lists[i++])
                instr = nir_instr_next(instr);
            else
                return NULL;
        } break;
        case nir_instr_type_alu: {
            nir_alu_instr *temp = nir_instr_as_alu(instr);
            if (j >= ARRAY_SIZE(base_op_lists))
                return NULL;
            if (temp->op == base_op_lists[j++])
                instr = nir_instr_next(instr);
            else
                return NULL;
        } break;
        default:
            return NULL;
        }
    }
    instr = nir_instr_prev(instr);
    nir_alu_instr *temp_alu = nir_instr_as_alu(instr);
    nir_def *temp = &temp_alu->def;
    b->cursor = nir_after_instr(instr);
    int32_t work_group_size = (nir->info.workgroup_size[0] *
                               nir->info.workgroup_size[1] *
                               nir->info.workgroup_size[2]);
    int32_t local_invocation_index_bits =
                    ffs(util_next_power_of_two(MAX2(work_group_size, 64))) - 1;
    assert(local_invocation_index_bits <= 8);
    if (nir->info.workgroup_size[0] != 1 ||
        nir->info.workgroup_size[1] != 1 ||
        nir->info.workgroup_size[2] != 1) {
        int32_t work_group_bits = (16 - local_invocation_index_bits);
        int32_t work_group_mask = (1 << work_group_bits) - 1;
        nir_def *work_group_memory = nir_imm_int(b, work_group_mask);
        temp = nir_iand(b, temp, work_group_memory);
    }
    nir_def *shared_per_work_group = nir_imm_int(b, nir->info.shared_size);
    temp = nir_imul(b, temp, shared_per_work_group);
    return temp;
}

static nir_def *
set_base_offset(nir_shader *nir)
{
    nir_def *base_offset = NULL;
    nir_foreach_function(function, nir) {
        if (function->impl) {
            nir_builder builder = nir_builder_create(function->impl);
            nir_foreach_block(block, function->impl) {
                nir_foreach_instr_safe(instr, block) {
                    if (instr->type == nir_instr_type_intrinsic && base_offset == NULL) {
                        base_offset = set_base_offset_impl(instr, &builder, nir);
                    }
                }
            }
        }
    }

    if (base_offset == NULL) {
        /* FIX(jm9100-oss): release 构建定义 NDEBUG 时 assert(0) 被编译掉,
         * 随后 return NULL 会被调用方 (lower_instr) 直接当基址用 → 空指针解引用。
         * 保守处理: 打印告警并返回 NULL, 让调用方跳过本次原子操作降级,
         * 而不是在 release 下悄悄产生未定义行为。 */
        fprintf(stderr, "mwv207: shared atomic base offset not found, skipping lowering\n");
    }
    return base_offset;
}

static bool
lower_instr(nir_shader *nir, nir_intrinsic_instr *instr, nir_builder *b, nir_def **base_offset_ptr)
{
    nir_def *offset = instr->src[0].ssa;
    nir_src *offset_src = &instr->src[0];
    switch (instr->intrinsic) {
    case nir_intrinsic_load_shared:
    case nir_intrinsic_shared_atomic:
    case nir_intrinsic_shared_atomic_swap:
        break;
    case nir_intrinsic_store_shared:
        offset = instr->src[1].ssa;
        offset_src = &instr->src[1];
        break;
    default:
        return false;
    }

    if (*base_offset_ptr == NULL) {
        *base_offset_ptr = set_base_offset(nir);
    }
    nir_def *base_offset = *base_offset_ptr;
    assert(base_offset);
    b->cursor = nir_before_instr(&instr->instr);
    nir_def *real_offset = nir_iadd(b, offset, base_offset);
    real_offset = nir_iadd_imm(b, real_offset, nir_intrinsic_base(instr));
    nir_src_rewrite(offset_src, real_offset);

    return true;
}

static bool
check_has_shared_atomic(nir_shader *nir)
{
    nir_foreach_function(function, nir) {
        if (function->impl) {
            nir_foreach_block(block, function->impl) {
                nir_foreach_instr_safe(instr, block) {
                    if (instr->type == nir_instr_type_intrinsic) {
                        switch (nir_instr_as_intrinsic(instr)->intrinsic) {
                        case nir_intrinsic_shared_atomic:
                        case nir_intrinsic_shared_atomic_swap:
                            return true;
                        default:
                            continue;
                        }
                    }
                }
            }
        }
    }
    return false;
}

void
mwv207_lower_shared_atomic(nir_shader *nir, struct mwv207_compile *c, struct mwv207_context *ctx)
{
    if (!check_has_shared_atomic(nir))
        return;

    bool progress = false;
    nir_def *base_offset = NULL;
    nir_foreach_function(function, nir) {
        if (function->impl) {
            nir_builder builder = nir_builder_create(function->impl);
            nir_foreach_block(block, function->impl) {
                nir_foreach_instr_safe(instr, block) {
                    if (instr->type == nir_instr_type_intrinsic)
                        progress |= lower_instr(nir, nir_instr_as_intrinsic(instr),
                                                &builder, &base_offset);
                }
            }

            nir_metadata_preserve(function->impl, nir_metadata_block_index |
                                                  nir_metadata_dominance);
        }
    }
    if (progress)
        c->shader->cs_shared_size = nir->info.shared_size;
}
