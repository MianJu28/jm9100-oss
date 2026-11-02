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

#include "mwv207_lower.h"
#include "compiler/nir/nir_builder.h"

static bool
lower_instr(nir_builder *b, nir_instr *instr, struct mwv207_context *ctx, struct mwv207_shader *shader)
{
	nir_tex_instr *tg4 = nir_instr_as_tex(instr);
	b->cursor = nir_after_instr(instr);
	nir_tex_instr *new_tg4 = nir_tex_instr_create(b->shader, tg4->num_srcs);
	new_tg4->op = tg4->op;
	new_tg4->sampler_dim = tg4->sampler_dim;
	new_tg4->dest_type = tg4->dest_type;
	new_tg4->num_srcs = tg4->num_srcs;
	uint32_t sampler_index = tg4->sampler_index;
	switch(tg4->component) {
	case 0:
		tg4->component = 0;
		break;
	case 1:
		tg4->component = 2;
		break;
	case 2:
		tg4->component = 0;
		sampler_index = sampler_index + 1;
		break;
	case 3:
		tg4->component = 2;
		sampler_index = sampler_index + 1;
		break;
	default:
		break;
	}
	new_tg4->component = tg4->component + 1;
	new_tg4->coord_components = tg4->coord_components;
	new_tg4->is_array = tg4->is_array;
	new_tg4->is_shadow = tg4->is_shadow;
	new_tg4->is_new_style_shadow = tg4->is_new_style_shadow;
	new_tg4->texture_index = tg4->texture_index;
	new_tg4->texture_non_uniform = tg4->texture_non_uniform;
	new_tg4->sampler_index = sampler_index;
	tg4->sampler_index = sampler_index;
	new_tg4->sampler_non_uniform = tg4->sampler_non_uniform;
	new_tg4->tg4_offsets[0][0] = tg4->tg4_offsets[0][0];
	new_tg4->tg4_offsets[0][1] = tg4->tg4_offsets[0][1];
	new_tg4->tg4_offsets[1][0] = tg4->tg4_offsets[1][0];
	new_tg4->tg4_offsets[1][1] = tg4->tg4_offsets[1][1];
	new_tg4->tg4_offsets[2][0] = tg4->tg4_offsets[2][0];
	new_tg4->tg4_offsets[2][1] = tg4->tg4_offsets[2][1];
	new_tg4->tg4_offsets[3][0] = tg4->tg4_offsets[3][0];
	new_tg4->tg4_offsets[3][1] = tg4->tg4_offsets[3][1];
	for (int j = 0; j < tg4->num_srcs; j++) {
		new_tg4->src[j].src = tg4->src[j].src;
		new_tg4->src[j].src_type = tg4->src[j].src_type;
	}
	nir_def_init(&new_tg4->instr, &new_tg4->def, 4, 32);

	nir_builder_instr_insert(b, &new_tg4->instr);
	nir_def *new_tg4_dst = &new_tg4->def;
	nir_def *tg4_dst = &tg4->def;
	nir_def *temp = nir_ishl(b, new_tg4_dst, nir_imm_int(b, 16));
	nir_def *temp2 = nir_ior(b, temp, tg4_dst);

	nir_def_rewrite_uses_after(tg4_dst, temp2, temp2->parent_instr);
	return true;
}

void
mwv207_nir_lower_tg4(nir_shader *nir, struct mwv207_shader *shader, struct mwv207_context *ctx)
{
	if (shader->key.is_tg4_lower) {
		nir_foreach_function(function, nir) {
			if (function->impl) {
				nir_builder builder = nir_builder_create(function->impl);
				nir_foreach_block(block, function->impl) {
					nir_foreach_instr_safe(instr, block) {
						if (instr->type == nir_instr_type_tex &&
							nir_instr_as_tex(instr)->op == nir_texop_tg4)
							lower_instr(&builder, instr, ctx, shader);
					}
				}
			}
		}
	}
}