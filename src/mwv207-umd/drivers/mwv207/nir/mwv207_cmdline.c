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
#include <err.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include "main/mtypes.h"
#include "compiler/glsl/standalone.h"
#include "compiler/glsl/glsl_to_nir.h"
#include "compiler/glsl/gl_nir.h"
#include "compiler/nir_types.h"
#include "util/u_dynarray.h"
#include "util/u_memory.h"
#include "tgsi/tgsi_from_mesa.h"

#include "mwv207_cmd.h"
#include "mwv207_context.h"
#include "mwv207_compiler.h"
#include "mwv207_shader.h"

static const struct nir_shader_compiler_options mwv207_nir_options_VSGSFS = {
   .lower_all_io_to_temps = true,
   .lower_ffma = true,
   .lower_flrp32 = true,
   .lower_fpow = true,
   .lower_fsat = true,
   .lower_sub = true,
   .lower_pack_half_2x16 = true,
   .lower_pack_half_2x16_split = true,
   .lower_pack_snorm_4x8 = true,
   .lower_pack_snorm_2x16 = true,
   .lower_pack_unorm_4x8 = true,
   .lower_pack_unorm_2x16 = true,
   .lower_unpack_half_2x16 = true,
   .lower_unpack_half_2x16_split = true,
   .lower_unpack_snorm_4x8 = true,
   .lower_unpack_snorm_2x16 = true,
   .lower_unpack_unorm_4x8 = true,
   .lower_unpack_unorm_2x16 = true,
   .lower_ftrunc = true,
   .lower_fdph = true,
   .lower_fmod = true,
};

static const struct nir_shader_compiler_options mwv207_nir_options_TCSTES = {
   .lower_ffma = true,
   .lower_flrp32 = true,
   .lower_fpow = true,
   .lower_fsat = true,
   .lower_sub = true,
   .lower_pack_half_2x16 = true,
   .lower_pack_half_2x16_split = true,
   .lower_pack_snorm_4x8 = true,
   .lower_pack_snorm_2x16 = true,
   .lower_pack_unorm_4x8 = true,
   .lower_pack_unorm_2x16 = true,
   .lower_unpack_half_2x16 = true,
   .lower_unpack_half_2x16_split = true,
   .lower_unpack_snorm_4x8 = true,
   .lower_unpack_snorm_2x16 = true,
   .lower_unpack_unorm_4x8 = true,
   .lower_unpack_unorm_2x16 = true,
   .lower_ftrunc = true,
   .lower_fdph = true,
   .lower_fmod = true,
};

static void
compile_shader(int file_num, char **argv)
{

   struct standalone_options options = {
      .glsl_version = 400,
      .do_link = true,
   };

   struct mwv207_context *ctx = CALLOC_STRUCT(mwv207_context);
   ctx->ws = CALLOC_STRUCT(mwv207_winsys);

   struct mwv207_shader *shader = CALLOC_STRUCT(mwv207_shader);
   shader->info = CALLOC_STRUCT(mwv207_shader_state);
   shader->info->current = shader;

   static struct gl_context local_ctx;
   struct gl_shader_program *prog = standalone_compile_shader(&options, file_num, argv, &local_ctx);

   struct nir_shader *nir;

   for (unsigned i = MESA_SHADER_VERTEX; i < MESA_SHADER_STAGES; ++i) {
      if (!prog->_LinkedShaders[i])
         continue;

      if (i == MESA_SHADER_TESS_CTRL || i == MESA_SHADER_TESS_EVAL)
         nir = glsl_to_nir(&local_ctx, prog, i, &mwv207_nir_options_TCSTES);
      else
         nir = glsl_to_nir(&local_ctx, prog, i, &mwv207_nir_options_VSGSFS);

      shader->stage = pipe_shader_type_from_mesa(i);;
      shader->info->nir = nir;
      NIR_PASS_V(nir, nir_lower_system_values);
      NIR_PASS_V(nir, nir_lower_global_vars_to_local);
      NIR_PASS_V(nir, nir_split_var_copies);
      NIR_PASS_V(nir, nir_lower_var_copies);

      NIR_PASS_V(nir, nir_lower_alu_to_scalar, NULL, NULL);

      NIR_PASS_V(nir, gl_nir_lower_bindless_images);

      NIR_PASS_V(nir, gl_nir_lower_buffers, prog);
      NIR_PASS_V(nir, nir_opt_constant_folding);

      mwv207_compile_shader(ctx, shader->info, true);

      nir_print_shader(nir, stderr);
      mwv207_dump_shader_nir(shader);

      FREE(shader->code);
      FREE(shader->uniforms.imm_data);
      FREE(shader->uniforms.imm_contents);
   }

   FREE(shader->info);
   FREE(shader);

   FREE(ctx);
   FREE(ctx->ws);
}

int
main(int argc, char **argv)
{
   if (argc < 2) {
      printf("Pass a command\n");
      exit(1);
   }

   if (strcmp(argv[1], "compile") == 0)
      compile_shader(argc - 2, &argv[2]);
   else if (strcmp(argv[1], "disasm") == 0)
      unreachable("Unknown command. Valid: compile");
   else
      unreachable("Unknown command. Valid: compile");

   return 0;
}
