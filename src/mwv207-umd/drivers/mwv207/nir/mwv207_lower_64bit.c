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

struct variable_pair{
   nir_variable *xy;
   nir_variable *zw;
};

struct inst64{
   struct list_head link;
   nir_instr* inst;
};

bool
mwv207_fp64_to_scalar_filter_cb(const nir_instr *instr, const void *data)
{
   if (instr->type != nir_instr_type_alu)
      return false;

   nir_alu_instr *alu = nir_instr_as_alu(instr);

   switch (alu->op) {
   case nir_op_unpack_64_2x32_split_x:
   case nir_op_unpack_64_2x32_split_y:
   case nir_op_pack_64_2x32_split:
      return true;

   default:
      break;
   }
    return false;
}

static struct variable_pair *
get_var_pair(nir_builder *b, nir_variable *old_var,
             struct hash_table *split_vars)
{
   struct variable_pair *new_var = NULL;
   unsigned old_components = glsl_get_components(
      glsl_without_array_or_matrix(old_var->type));

   assert(old_components > 2 && old_components <= 4);

   struct hash_entry *entry = _mesa_hash_table_search(split_vars, old_var);
   if (!entry) {
      new_var = (struct variable_pair *)calloc(1, sizeof(struct variable_pair));
      new_var->xy = nir_variable_clone(old_var, b->shader);
      new_var->zw = nir_variable_clone(old_var, b->shader);
      new_var->xy->type = glsl_dvec_type(2);
      new_var->zw->type = glsl_dvec_type(old_components - 2);

      if (glsl_type_is_array(old_var->type)) {
         new_var->xy->type = glsl_array_type(new_var->xy->type, glsl_array_size(old_var->type), 0);
         new_var->zw->type = glsl_array_type(new_var->zw->type, glsl_array_size(old_var->type), 0);
      }

      if (old_var->data.mode == nir_var_shader_in ||
          old_var->data.mode == nir_var_shader_out) {
         new_var->zw->data.driver_location++;
         new_var->zw->data.location++;
         nir_shader_add_variable(b->shader, new_var->xy);
         nir_shader_add_variable(b->shader, new_var->zw);
      } else if (old_var->data.mode == nir_var_function_temp) {
         exec_list_push_tail(&b->impl->locals, &new_var->xy->node);
         exec_list_push_tail(&b->impl->locals, &new_var->zw->node);
      }

      _mesa_hash_table_insert(split_vars, old_var, new_var);
   } else
      new_var = (struct variable_pair *)entry->data;
   return new_var;
}

static bool
need_split_64bit_var(const nir_instr *instr, const void *data)
{
   switch (instr->type) {
   case nir_instr_type_intrinsic: {
      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
      switch(intr->intrinsic) {
      case nir_intrinsic_load_uniform:
      case nir_intrinsic_load_ubo:
      case nir_intrinsic_load_ssbo:
      case nir_intrinsic_load_global:
      case nir_intrinsic_load_deref:
         if (intr->def.bit_size != 64)
            return false;
         return (intr->def.num_components >= 3);
      case nir_intrinsic_store_deref:
         if (nir_src_bit_size(intr->src[1]) != 64)
            return false;
         return (nir_src_num_components(intr->src[1]) >= 3);
      default:
         return false;
      }
   }
   case nir_instr_type_load_const: {
      nir_load_const_instr *lc = nir_instr_as_load_const(instr);
      if (lc->def.bit_size != 64)
         return false;
      return lc->def.num_components >= 3;
   }
   default:
      return false;
   }
}

static nir_def *
merge_64bit_loads(nir_builder *b,
                  nir_def *load1,
                  nir_def *load2,
                  bool out_is_vec3)
{
   if (out_is_vec3)
      return nir_vec3(b,
                      nir_channel(b, load1, 0),
                      nir_channel(b, load1, 1),
                      nir_channel(b, load2, 0));
   else
      return nir_vec4(b,
                      nir_channel(b, load1, 0),
                      nir_channel(b, load1, 1),
                      nir_channel(b, load2, 0),
                      nir_channel(b, load2, 1));
}

static nir_def *
split_double_load_uniform(nir_builder *b, nir_intrinsic_instr *intr)
{
   unsigned second_components = intr->def.num_components - 2;
   nir_intrinsic_instr *load2 =
      nir_intrinsic_instr_create(b->shader, nir_intrinsic_load_uniform);
   load2->src[0] = nir_src_for_ssa(nir_iadd_imm(b, intr->src[0].ssa, 1));
   nir_intrinsic_set_dest_type(load2, nir_intrinsic_dest_type(intr));
   nir_intrinsic_set_base(load2, nir_intrinsic_base(intr));
   nir_intrinsic_set_range(load2, nir_intrinsic_range(intr));
   load2->num_components = second_components;

   nir_def_init(&load2->instr, &load2->def, second_components, 64);
   nir_builder_instr_insert(b, &load2->instr);

   intr->def.num_components = intr->num_components = 2;

   return merge_64bit_loads(b, &intr->def, &load2->def, second_components == 1);
}

static nir_def *
split_double_load_ubo(nir_builder *b, nir_intrinsic_instr *intr)
{
   unsigned second_components = intr->def.num_components - 2;
   nir_intrinsic_instr *load2 =
      nir_instr_as_intrinsic(nir_instr_clone(b->shader, &intr->instr));
   load2->src[0] = intr->src[0];
   load2->src[1] = nir_src_for_ssa(nir_iadd_imm(b, intr->src[1].ssa, 16));
   nir_intrinsic_set_range_base(load2, nir_intrinsic_range_base(intr) + 16);
   nir_intrinsic_set_range(load2, nir_intrinsic_range(intr));
   nir_intrinsic_set_access(load2, nir_intrinsic_access(intr));
   nir_intrinsic_set_align_mul(load2, nir_intrinsic_align_mul(intr));
   nir_intrinsic_set_align_offset(load2, nir_intrinsic_align_offset(intr));

   load2->num_components = second_components;

   nir_def_init(&load2->instr, &load2->def, second_components, 64);
   nir_builder_instr_insert(b, &load2->instr);

   intr->def.num_components = intr->num_components = 2;
   return merge_64bit_loads(b, &intr->def, &load2->def, second_components == 1);
}

static nir_def *
split_double_load_ssbo(nir_builder *b, nir_intrinsic_instr *intr)
{
   unsigned second_components = intr->def.num_components - 2;
   nir_intrinsic_instr *load2 =
      nir_instr_as_intrinsic(nir_instr_clone(b->shader, &intr->instr));

   nir_src_rewrite(&load2->src[0], nir_iadd_imm(b, intr->src[0].ssa, 1));
   load2->num_components = second_components;
   nir_def_init(&load2->instr, &load2->def, second_components, 64);

   nir_intrinsic_set_dest_type(load2, nir_intrinsic_dest_type(intr));
   nir_builder_instr_insert(b, &load2->instr);

   intr->def.num_components = intr->num_components = 2;

   return merge_64bit_loads(b, &intr->def, &load2->def, second_components == 1);
}

static nir_def *
split_double_load_global(nir_builder *b, nir_intrinsic_instr *intr)
{
   unsigned second_components = intr->def.num_components - 2;
   nir_intrinsic_instr *load2 = nir_intrinsic_instr_create(b->shader, nir_intrinsic_load_global);
   nir_def_init(&load2->instr, &load2->def, second_components, 64);
   load2->src[0] = nir_src_for_ssa(nir_iadd_imm(b, intr->src[0].ssa, 16));
   nir_intrinsic_set_align_mul(load2, nir_intrinsic_align_mul(intr));
   load2->num_components = second_components;
   nir_builder_instr_insert(b, &load2->instr);
   intr->def.num_components = intr->num_components = 2;

   return merge_64bit_loads(b, &intr->def, &load2->def, second_components == 1);
}

static nir_def *
split_store_deref_var(nir_builder *b,
                      nir_intrinsic_instr *intr,
                      UNUSED nir_deref_instr *deref,
                      struct hash_table *split_vars)
{
   nir_variable *old_var = nir_intrinsic_get_var(intr, 0);
   unsigned old_components = glsl_get_components(glsl_without_array(old_var->type));

   assert(old_components > 2 && old_components <= 4);

   nir_def *src_xy = nir_trim_vector(b, intr->src[1].ssa, 2);

   struct variable_pair *vars = get_var_pair(b, old_var, split_vars);

   nir_deref_instr *deref1 = nir_build_deref_var(b, vars->xy);
   nir_build_store_deref(b, &deref1->def, src_xy, 3);

   nir_deref_instr *deref2 = nir_build_deref_var(b, vars->zw);
   if (old_components == 3)
      nir_build_store_deref(b, &deref2->def,
                            nir_channel(b, intr->src[1].ssa, 0x2), 1);
   else
      nir_build_store_deref(b, &deref2->def,
                            nir_channels(b, intr->src[1].ssa, 0xc), 3);

   return NIR_LOWER_INSTR_PROGRESS_REPLACE;
}

static nir_def *
split_store_deref_array(nir_builder *b,
                        nir_intrinsic_instr *intr,
                        nir_deref_instr *deref,
                        struct hash_table *split_vars)
{
   nir_variable *old_var = nir_intrinsic_get_var(intr, 0);
   unsigned old_components = glsl_get_components(glsl_without_array(old_var->type));

   assert(old_components > 2 && old_components <= 4);

   nir_def *src_xy = nir_trim_vector(b, intr->src[1].ssa, 2);

   struct variable_pair *vars = get_var_pair(b, old_var, split_vars);

   nir_deref_instr *deref1 = nir_build_deref_var(b, vars->xy);
   nir_deref_instr *deref_array1 =
      nir_build_deref_array(b, deref1, deref->arr.index.ssa);

   nir_build_store_deref(b, &deref_array1->def, src_xy, 3);

   nir_deref_instr *deref2 = nir_build_deref_var(b, vars->zw);
   nir_deref_instr *deref_array2 =
      nir_build_deref_array(b, deref2, deref->arr.index.ssa);

   if (old_components == 3)
      nir_build_store_deref(b, &deref_array2->def,
                            nir_channel(b, intr->src[1].ssa, 0x2), 1);
   else
      nir_build_store_deref(b, &deref_array2->def,
                            nir_channels(b, intr->src[1].ssa, 0xc), 3);

   return NIR_LOWER_INSTR_PROGRESS_REPLACE;
}

static nir_def *
split_double_store_deref(nir_builder *b,
                         nir_intrinsic_instr *intr,
                         struct hash_table *split_vars)
{
   nir_deref_instr *deref = nir_instr_as_deref(intr->src[0].ssa->parent_instr);
   if (deref->deref_type == nir_deref_type_var)
      return split_store_deref_var(b, intr, deref, split_vars);
   else if (deref->deref_type == nir_deref_type_array)
      return split_store_deref_array(b, intr, deref, split_vars);
   else
      unreachable("only splitting of stores to vars and arrays is supported");
}

static nir_def *
split_load_deref_var(nir_builder *b,
                     nir_intrinsic_instr *intr,
                     struct hash_table *split_vars)
{
   nir_variable *old_var = nir_intrinsic_get_var(intr, 0);
   struct variable_pair *vars = get_var_pair(b, old_var, split_vars);
   unsigned old_components = glsl_get_components(old_var->type);

   nir_deref_instr *deref1 = nir_build_deref_var(b, vars->xy);
   nir_def *load1 = nir_load_deref(b, deref1);

   nir_deref_instr *deref2 = nir_build_deref_var(b, vars->zw);
   deref2->type = vars->zw->type;

   nir_def *load2 = nir_load_deref(b, deref2);

   return merge_64bit_loads(b, load1, load2, old_components == 3);
}

static nir_def *
split_load_deref_array(nir_builder *b,
                       nir_intrinsic_instr *intr,
                       nir_src *index,
                       struct hash_table *split_vars)
{
   nir_variable *old_var = nir_intrinsic_get_var(intr, 0);
   unsigned old_components = glsl_get_components(glsl_without_array(old_var->type));

   assert(old_components > 2 && old_components <= 4);

   struct variable_pair *vars = get_var_pair(b, old_var, split_vars);

   nir_deref_instr *deref1 = nir_build_deref_var(b, vars->xy);
   nir_deref_instr *deref_array1 = nir_build_deref_array(b, deref1, index->ssa);
   nir_def *load1 =
      nir_build_load_deref(b, 2, 64, &deref_array1->def, (enum gl_access_qualifier)0);

   nir_deref_instr *deref2 = nir_build_deref_var(b, vars->zw);
   nir_deref_instr *deref_array2 = nir_build_deref_array(b, deref2, index->ssa);

   nir_def *load2 = nir_build_load_deref(
      b, old_components - 2, 64, &deref_array2->def, (enum gl_access_qualifier)0);

   return merge_64bit_loads(b, load1, load2, old_components == 3);
}

static nir_def *
split_double_load_deref(nir_builder *b,
                        nir_intrinsic_instr *intr,
                        struct hash_table *split_vars)
{
   nir_deref_instr *deref = nir_instr_as_deref(intr->src[0].ssa->parent_instr);
   if (deref->deref_type == nir_deref_type_var)
      return split_load_deref_var(b, intr, split_vars);
   else if (deref->deref_type == nir_deref_type_array)
      return split_load_deref_array(b, intr, &deref->arr.index, split_vars);
   else
      unreachable("only splitting of stores to vars and arrays is supported");
}

static nir_def *
split_load_const(nir_builder *b, nir_load_const_instr *lc)
{
   int comp = lc->def.num_components;
   nir_def *ir[comp];
   for (unsigned i = 0; i < comp; ++i)
      ir[i] = nir_imm_double(b, lc->value[i].f64);
   return nir_vec(b, ir, comp);
}

static nir_def *
split_64bit_var(nir_builder *b, nir_instr *instr, void *data)
{
   switch (instr->type) {
   case nir_instr_type_intrinsic: {
      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
      struct hash_table *split_vars = (struct hash_table *)data;
      switch (intr->intrinsic) {
      case nir_intrinsic_load_uniform:
         return split_double_load_uniform(b, intr);
      case nir_intrinsic_load_ubo:
         return split_double_load_ubo(b, intr);
      case nir_intrinsic_load_ssbo:
         return split_double_load_ssbo(b, intr);
      case nir_intrinsic_load_global:
         return split_double_load_global(b, intr);
      case nir_intrinsic_store_deref:
         return split_double_store_deref(b, intr, split_vars);
      case nir_intrinsic_load_deref:
         return split_double_load_deref(b, intr, split_vars);
      default:
         unreachable("Not support intrinsic");
         return NULL;
      }
   }
   case nir_instr_type_load_const: {
      nir_load_const_instr *lc = nir_instr_as_load_const(instr);
      return split_load_const(b, lc);
   }
   default:
      unreachable("Not support instr type");
   }
   return NULL;
}

bool
mwv207_nir_split_64bit_var(nir_shader *shader)
{
   struct hash_table *split_vars = _mesa_pointer_hash_table_create(NULL);
   bool result = nir_shader_lower_instructions(shader,
                                               need_split_64bit_var,
                                               split_64bit_var,
                                               split_vars);

   _mesa_hash_table_destroy(split_vars, NULL);

   return result;
}

static bool
is_64bit_uniforms_and_ubo(const nir_instr *instr, const void *data)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
   if (intr->intrinsic != nir_intrinsic_load_uniform &&
       intr->intrinsic != nir_intrinsic_load_ubo &&
       intr->intrinsic != nir_intrinsic_load_ubo_vec4)
      return false;

   return (intr->def.bit_size == 64);
}

static nir_def *
lower_64bit_uniforms_and_ubo(nir_builder *b, nir_instr *instr, void *data)
{
   nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
   int old_components = intr->def.num_components;
   assert(old_components <= 2);
   intr->def.num_components *= 2;
   intr->def.bit_size = 32;
   intr->num_components *= 2;

   if ((intr->intrinsic == nir_intrinsic_load_ubo ||
       intr->intrinsic == nir_intrinsic_load_ubo_vec4) &&
       nir_intrinsic_has_component(intr))
      nir_intrinsic_set_component(intr, 2 * nir_intrinsic_component(intr));

   nir_def *result_vec[2] = {NULL, NULL};

   for (int i = 0; i < old_components; i++)
      result_vec[i] = nir_pack_64_2x32_split(b,
                                             nir_channel(b, &intr->def, 2 * i),
                                             nir_channel(b, &intr->def, 2 * i + 1));

   if (old_components == 1)
      return result_vec[0];
   else
      return nir_vec2(b, result_vec[0], result_vec[1]);
}

bool
mwv207_nir_lower_64bit_uniforms_and_ubo(nir_shader *shader)
{
   return nir_shader_lower_instructions(shader,
                                        is_64bit_uniforms_and_ubo,
                                        lower_64bit_uniforms_and_ubo,
                                        NULL);
}

static nir_def *
load_64_to_vec2(nir_intrinsic_instr *intr)
{
   intr->num_components *= 2;
   intr->def.bit_size = 32;
   intr->def.num_components *= 2;
   if (nir_intrinsic_has_component(intr))
      nir_intrinsic_set_component(intr, nir_intrinsic_component(intr) * 2);
   return NIR_LOWER_INSTR_PROGRESS;
}

static nir_def *
store_64_to_vec2(nir_builder *b, nir_intrinsic_instr *intr)
{
   nir_deref_instr *deref = nir_instr_as_deref(intr->src[0].ssa->parent_instr);
   nir_variable *var = nir_intrinsic_get_var(intr, 0);

   unsigned components = glsl_get_components(glsl_without_array(var->type));
   unsigned wrmask = nir_intrinsic_write_mask(intr);
   if (glsl_type_is_64bit(glsl_without_array(var->type))) {
      components *= 2;
      if (deref->deref_type == nir_deref_type_var)
         var->type = glsl_vec_type(components);
      else if (deref->deref_type == nir_deref_type_array)
         var->type =
            glsl_array_type(glsl_vec_type(components), glsl_array_size(var->type), 0);
      else {
         nir_print_shader(b->shader, stderr);
         unreachable("Only lowring of var and array derefs supported");
      }
   }
   deref->type = var->type;
   if (deref->deref_type == nir_deref_type_array) {
      nir_deref_instr *deref_array = nir_instr_as_deref(deref->parent.ssa->parent_instr);
      deref_array->type = var->type;
      deref->type = glsl_without_array(deref_array->type);
   }
   intr->num_components = components;
   nir_intrinsic_set_write_mask(intr, wrmask == 1 ? 0x3 : 0xf);
   return NIR_LOWER_INSTR_PROGRESS;
}

static bool
store_64bit_intr(nir_src *src, void *state)
{
   bool *s = (bool *)state;
   *s = (nir_src_bit_size(*src) == 64);
   return !*s;
}

static bool
double2vec2(nir_src *src, UNUSED void *state)
{
   if (nir_src_bit_size(*src) != 64)
      return true;

   src->ssa->bit_size = 32;
   src->ssa->num_components *= 2;
   return true;
}

static bool
need_lower_to_vec2(const nir_instr *instr, const void *data)
{
   switch (instr->type) {
   case nir_instr_type_intrinsic: {
      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

      switch (intr->intrinsic) {
      case nir_intrinsic_store_deref: {
         if (nir_src_bit_size(intr->src[1]) == 64)
            return true;
         nir_variable *var = nir_intrinsic_get_var(intr, 0);
         if (glsl_type_is_struct(glsl_without_array(var->type)))
            return false;
         if (glsl_type_is_64bit(glsl_without_array(var->type)))
            return true;
         return false;
      }
      case nir_intrinsic_store_global:
         return nir_src_bit_size(intr->src[0]) == 64;
      case nir_intrinsic_load_deref:
      case nir_intrinsic_load_global:
      case nir_intrinsic_load_global_constant:
         return (intr->def.bit_size == 64);
      default:
         return false;
      }
   }
   case nir_instr_type_alu: {
      nir_alu_instr *alu = nir_instr_as_alu(instr);
      return (alu->def.bit_size == 64);
   }
   case nir_instr_type_load_const: {
      nir_load_const_instr *lc = nir_instr_as_load_const(instr);
      return (lc->def.bit_size == 64);
   }
   case nir_instr_type_undef: {
      nir_undef_instr *undef = nir_instr_as_undef(instr);
      return (undef->def.bit_size == 64);
   }
   default:
      return false;
   }
}

static nir_def *
load_deref_64_to_vec2(nir_builder *b, nir_intrinsic_instr *intr)
{
   nir_deref_instr *deref = nir_instr_as_deref(intr->src[0].ssa->parent_instr);
   nir_variable *var = nir_intrinsic_get_var(intr, 0);
   unsigned components = glsl_get_components(glsl_without_array(var->type));
   if (glsl_get_bit_size(glsl_without_array(var->type)) == 64) {
      components *= 2;
      if (deref->deref_type == nir_deref_type_var)
         var->type = glsl_vec_type(components);
      else if (deref->deref_type == nir_deref_type_array)
         var->type =
            glsl_array_type(glsl_vec_type(components), glsl_array_size(var->type), 0);
      else {
         nir_print_shader(b->shader, stderr);
         unreachable("Only lowring of var and array derefs supported");
      }
   }
   deref->type = var->type;
   if (deref->deref_type == nir_deref_type_array) {
      nir_deref_instr *deref_array = nir_instr_as_deref(deref->parent.ssa->parent_instr);
      deref_array->type = var->type;
      deref->type = glsl_without_array(deref_array->type);
   }

   intr->num_components = components;
   intr->def.bit_size = 32;
   intr->def.num_components = components;
   return NIR_LOWER_INSTR_PROGRESS;
}

static nir_def *
lower_64bit_to_vec2(nir_builder *b, nir_instr *instr, void *data)
{
   switch (instr->type) {
   case nir_instr_type_intrinsic: {
      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
      switch (intr->intrinsic) {
      case nir_intrinsic_store_deref:
         return store_64_to_vec2(b, intr);
      case nir_intrinsic_load_deref:
         return load_deref_64_to_vec2(b, intr);
      case nir_intrinsic_load_global:
      case nir_intrinsic_load_global_constant:
         return load_64_to_vec2(intr);
      default:
         return NULL;
      }
   }
   case nir_instr_type_alu: {
      nir_alu_instr *alu = nir_instr_as_alu(instr);
      alu->def.bit_size = 32;
      alu->def.num_components *= 2;
      switch (alu->op) {
      case nir_op_pack_64_2x32_split:
         alu->op = nir_op_vec2;
         break;
      case nir_op_pack_64_2x32:
         alu->op = nir_op_mov;
         break;
      case nir_op_vec2:
         return nir_vec4(b,
                         nir_channel(b, alu->src[0].src.ssa, 0),
                         nir_channel(b, alu->src[0].src.ssa, 1),
                         nir_channel(b, alu->src[1].src.ssa, 0),
                         nir_channel(b, alu->src[1].src.ssa, 1));
      case nir_op_vec4:
         assert(b->shader->info.stage == MESA_SHADER_KERNEL);
         return nir_vec8(b,
                         nir_channel(b, alu->src[0].src.ssa, 0),
                         nir_channel(b, alu->src[0].src.ssa, 1),
                         nir_channel(b, alu->src[1].src.ssa, 0),
                         nir_channel(b, alu->src[1].src.ssa, 1),
                         nir_channel(b, alu->src[2].src.ssa, 0),
                         nir_channel(b, alu->src[2].src.ssa, 1),
                         nir_channel(b, alu->src[3].src.ssa, 0),
                         nir_channel(b, alu->src[3].src.ssa, 1));
      default:
         return NULL;
      }
      return NIR_LOWER_INSTR_PROGRESS;
   }
   case nir_instr_type_load_const: {
      nir_load_const_instr *lc = nir_instr_as_load_const(instr);
      assert(lc->def.num_components <= 2);
      nir_const_value val[4];
      for (uint i = 0; i < lc->def.num_components; i++) {
         uint64_t v = lc->value[i].u64;
         val[i * 2 + 0] = nir_const_value_for_uint(v & 0xffffffff, 32);
         val[i * 2 + 1] = nir_const_value_for_uint(v >> 32, 32);
      }

      return nir_build_imm(b, 2 * lc->def.num_components, 32, val);
   }
   case nir_instr_type_undef: {
      nir_undef_instr *undef = nir_instr_as_undef(instr);
      undef->def.num_components *= 2;
      undef->def.bit_size = 32;
      return NIR_LOWER_INSTR_PROGRESS;
   }
   default:
      return NULL;
   }
}

bool
mwv207_nir_64_to_vec2(nir_shader *shader)
{
   struct inst64 *intr64bit = rzalloc(NULL, struct inst64);
   list_inithead(&intr64bit->link);
   intr64bit->inst = NULL;

   nir_foreach_function_impl(impl, shader) {
      nir_foreach_block(block, impl) {
         nir_foreach_instr_safe(instr, block) {
            switch (instr->type) {
            case nir_instr_type_alu: {
               bool success = false;
               nir_foreach_src(instr, store_64bit_intr, &success);
               if (success) {
                  struct inst64 *new = rzalloc(intr64bit, struct inst64);
                  list_inithead(&new->link);
                  new->inst = instr;
                  list_addtail(&new->link, &intr64bit->link);
               }
               break;
            }
            case nir_instr_type_intrinsic: {
               nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
               switch (intr->intrinsic) {
               case nir_intrinsic_store_global: {
                  bool success = false;
                  nir_foreach_src(instr, store_64bit_intr, &success);
                  if (success) {
                     intr->num_components *= 2;
                     nir_intrinsic_set_write_mask(intr, (1<<intr->num_components) - 1);
                  }
                  break;
               }
               default:
                  break;
               }
            }
            default:
               break;
            }
         }
      }
   }
   bool result =  nir_shader_lower_instructions(shader,
                                                need_lower_to_vec2,
                                                lower_64bit_to_vec2,
                                                NULL);

   if (result || intr64bit->inst) {
      list_for_each_entry_safe(struct inst64, each_instr, &intr64bit->link, link) {
         if (each_instr->inst->type == nir_instr_type_alu) {
            nir_alu_instr *alu = nir_instr_as_alu(each_instr->inst);
            nir_op_info alu_info = nir_op_infos[alu->op];
            for (uint32_t i = 0; i < alu_info.num_inputs; i++) {
               int swizzle[NIR_MAX_VEC_COMPONENTS] = {0};
               for (unsigned k = 0; k < NIR_MAX_VEC_COMPONENTS / 2; k++) {
                  if (!nir_alu_instr_channel_used(alu, i, k))
                     continue;

                  switch (alu->op) {
                  case nir_op_unpack_64_2x32_split_x:
                     swizzle[2 * k] = alu->src[i].swizzle[k] * 2;
                     alu->op = nir_op_mov;
                     break;
                  case nir_op_unpack_64_2x32_split_y:
                     swizzle[2 * k] = alu->src[i].swizzle[k] * 2 + 1;
                     alu->op = nir_op_mov;
                     break;
                  case nir_op_unpack_64_2x32:
                     alu->op = nir_op_mov;
                     break;
                  case nir_op_bcsel:
                     if (i == 0) {
                        swizzle[2 * k] = swizzle[2 * k + 1] = alu->src[i].swizzle[k] * 2;
                        break;
                     }
                     FALLTHROUGH;
                  default:
                     swizzle[2 * k] = alu->src[i].swizzle[k] * 2;
                     swizzle[2 * k + 1] = alu->src[i].swizzle[k] * 2 + 1;
                     break;
                  }
               }
               for (unsigned k = 0; k < NIR_MAX_VEC_COMPONENTS; k++)
                  alu->src[i].swizzle[k] = swizzle[k];
            }
         }
      }
      result = true;
   }

   ralloc_free(intr64bit);
   return result;
}
