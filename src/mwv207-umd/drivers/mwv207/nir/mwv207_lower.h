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
#ifndef H_MWV207_LOWER
#define H_MWV207_LOWER

#include "mwv207_common.h"
#include "mwv207_compiler.h"
#include "mwv207_context.h"
#include "mwv207_resource.h"

void mwv207_lower_shared_atomic(nir_shader *nir, struct mwv207_compile *c, struct mwv207_context *ctx);

void mwv207_nir_lower_tg4(nir_shader *nir, struct mwv207_shader *shader, struct mwv207_context *ctx);

bool mwv207_nir_split_64bit_var(nir_shader *shader);

bool mwv207_nir_64_to_vec2(nir_shader *shader);

bool mwv207_nir_lower_64bit_uniforms_and_ubo(nir_shader *shader);

bool mwv207_fp64_to_scalar_filter_cb(const nir_instr *instr, const void *data);

void mwv207_lower_dual_blend(nir_shader *nir, struct mwv207_context *ctx, struct mwv207_shader *shader);

#endif