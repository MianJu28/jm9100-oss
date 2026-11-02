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
#include "mwv207_emit.h"
#include "compiler/nir/nir_worklist.h"

static void
range_include(struct live_def *def, uint32_t index)
{
   if (def->live_start > index)
      def->live_start = index;

   if (def->live_end < index)
      def->live_end = index;
}

static bool
init_liveness_block(nir_block *block,
                    struct live_defs_state *state)
{
   block->live_in = reralloc(block, block->live_in, BITSET_WORD,
                             state->bitset_words);
   memset(block->live_in, 0, state->bitset_words * sizeof(BITSET_WORD));

   block->live_out = reralloc(block, block->live_out, BITSET_WORD,
                              state->bitset_words);
   memset(block->live_out, 0, state->bitset_words * sizeof(BITSET_WORD));

   nir_block_worklist_push_head(&state->worklist, block);

   return true;
}

static bool
set_src_live(nir_src *src, void *void_state)
{
   struct live_defs_state *state = void_state;

   nir_instr *instr = src->ssa->parent_instr;

   if (is_sysval(instr))
      return true;

   switch (instr->type) {
   case nir_instr_type_deref:
   case nir_instr_type_load_const:
   case nir_instr_type_undef:
      return true;
   case nir_instr_type_alu: {

      nir_alu_instr *alu = nir_instr_as_alu(instr);
      if (instr->pass_flags & BYPASS_SRC) {
         for (uint32_t i = 0; i < nir_op_infos[alu->op].num_inputs; i++)
            set_src_live(&alu->src[i].src, state);
         return true;
      }
   } break;
   default:
      break;
   }

   uint32_t i = state->live_map[src_index(state->impl, src)];
   assert(i != ~0u);

   BITSET_SET(state->block->live_in, i);
   range_include(&state->defs[i], state->index);

   return true;
}

static bool
set_ssa_def_dead(nir_def *def, void *void_state)
{
   struct live_defs_state *state = void_state;

   uint32_t i = state->live_map[dest_index(state->impl, def)];

   if (i == ~0u)
      return true;

   BITSET_CLEAR(state->block->live_in, i);

   return true;
}

static bool
propagate_across_edge(nir_block *pred, nir_block *succ,
                      struct live_defs_state *state)
{
   BITSET_WORD progress = 0;

   for (uint32_t i = 0; i < state->bitset_words; ++i) {
      progress |= succ->live_in[i] & ~pred->live_out[i];
      pred->live_out[i] |= succ->live_in[i];
   }

   return progress != 0;
}

bool
is_decl_reg(nir_instr *instr)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intrin = nir_instr_as_intrinsic(instr);
   return intrin->intrinsic == nir_intrinsic_decl_reg;
}

uint32_t
live_defs(nir_function_impl *impl, struct live_def *defs, uint32_t *live_map, bool has_store)
{
   struct live_defs_state state;
   uint32_t block_live_index[impl->num_blocks + 1];

   state.impl = impl;
   state.defs = defs;
   state.live_map = live_map;

   state.num_defs = 0;
   nir_foreach_block(block, impl) {
      block_live_index[block->index] = state.num_defs;
      nir_foreach_instr(instr, block) {
         nir_def *dest = dest_for_instr(instr);

         if (!dest || is_decl_reg(instr))
            continue;

         uint32_t idx = dest_index(impl, dest);

         if (live_map[idx] != ~0u)
            continue;

         defs[state.num_defs] = (struct live_def) {instr, dest, state.num_defs, state.num_defs};

         if (instr->type == nir_instr_type_intrinsic) {
            nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

            if (intr->intrinsic == nir_intrinsic_load_input ||
                intr->intrinsic == nir_intrinsic_load_vertex_id ||
                intr->intrinsic == nir_intrinsic_load_instance_id ||
                intr->intrinsic == nir_intrinsic_load_primitive_id ||
                intr->intrinsic == nir_intrinsic_load_invocation_id ||
                intr->intrinsic == nir_intrinsic_load_tess_coord ||
                intr->intrinsic == nir_intrinsic_load_frag_coord)
               defs[state.num_defs].live_start = 0;
         }

         live_map[idx] = state.num_defs;
         state.num_defs++;
      }
   }
   block_live_index[impl->num_blocks] = state.num_defs;

   nir_block_worklist_init(&state.worklist, impl->num_blocks, NULL);

   state.bitset_words = BITSET_WORDS(state.num_defs);
   nir_foreach_block(block, impl) {
      init_liveness_block(block, &state);
   }

   while (!nir_block_worklist_is_empty(&state.worklist)) {
      nir_block *block = nir_block_worklist_pop_head(&state.worklist);
      state.block = block;

      memcpy(block->live_in, block->live_out,
             state.bitset_words * sizeof(BITSET_WORD));

      state.index = block_live_index[block->index + 1];

      nir_if *following_if = nir_block_get_following_if(block);

      if (following_if)
         set_src_live(&following_if->condition, &state);

      nir_foreach_instr_reverse(instr, block) {

         if (state.index && instr == defs[state.index - 1].instr) {
            state.index--;
            BITSET_CLEAR(block->live_in, state.index);
         }

         if (is_dead_instruction(instr))
            continue;

         uint32_t index = state.index;

         if (instr->type == nir_instr_type_intrinsic) {
            nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

            if (intr->intrinsic == nir_intrinsic_store_deref && !has_store)
               state.index = ~0u;
         }

         bool processed = false;
         if (instr->type == nir_instr_type_intrinsic) {
            nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
            if (intr->intrinsic == nir_intrinsic_decl_reg ||
               intr->intrinsic == nir_intrinsic_store_reg)
               processed = true;
         }

         nir_foreach_def(instr, set_ssa_def_dead, &state);

         if (!processed)
            nir_foreach_src(instr, set_src_live, &state);

         state.index = index;
      }
      assert(state.index == block_live_index[block->index]);

      set_foreach(block->predecessors, entry) {
         nir_block *pred = (nir_block *)entry->key;

         if (propagate_across_edge(pred, block, &state))
            nir_block_worklist_push_tail(&state.worklist, pred);
      }
   }

   nir_block_worklist_fini(&state.worklist);

   nir_foreach_block(block, impl) {
      int i;

      BITSET_FOREACH_SET(i, block->live_in, state.num_defs)
         range_include(&state.defs[i], block_live_index[block->index]);

      BITSET_FOREACH_SET(i, block->live_out, state.num_defs)
         range_include(&state.defs[i], block_live_index[block->index + 1]);
   }

   return state.num_defs;
}

