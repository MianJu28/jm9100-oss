# Copyright © 2018 Collabora Ltd

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:

# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

inc_mwv207 = include_directories('include')
inc_mwv207_nir = include_directories('nir')
inc_mwv207_video_codec = include_directories('video-codec','video-codec/common','video-codec/mwv207')
inc_mwv207_nir = include_directories('nir')

files_libmwv207 = files(
	'nir/mwv207_disasm.c',
	'nir/mwv207_opcode.c',
	'nir/mwv207_liveness.c',
	'nir/mwv207_ra.c',
	'nir/mwv207_emit.c',
	'nir/mwv207_lower.c',
	'nir/mwv207_common.c',
	'nir/mwv207_compiler.c',
	'nir/mwv207_nir_opt_peephole_fdiv.c',
	'nir/mwv207_nir_opt_peephole_ffma.c',
	'nir/mwv207_nir_opt_peephole_set.c',
	'nir/mwv207_lower_shared_atomic.c',
	'nir/mwv207_lower_64bit.c',
	'nir/mwv207_lower_tg4.c',
        'nir/mwv207_lower_dual.c',
	'mwv207_cmd.c',
	'mwv207_blit.c',
	'mwv207_shader.c',
	'mwv207_context.c',
	'mwv207_draw.c',
	'mwv207_compute.c',
	'mwv207_format.c',
	'mwv207_sampler.c',
	'mwv207_query.c',
	'mwv207_blend.c',
	'mwv207_resource.c',
	'mwv207_vertex.c',
	'mwv207_screen.c',
	'mwv207_state.c',
	'mwv207_surface.c',

        'video-codec/mwv207_video_buffer.c',
        'video-codec/mwv207_video_codec.c',
        'video-codec/mwv207_video_decoder.c',
        'video-codec/common/mwv207_log.c',
        'video-codec/common/mwv207_codec_time.c',
        'video-codec/common/mwv207_video_reg_buf.c',
        'video-codec/common/mwv207_video_cs.c',
        'video-codec/common/mwv207_decoder_sw_stream.c',
        'video-codec/common/mwv207_decoder_sw_util.c',
        'video-codec/mwv207/mwv207_video_dec.c',
        'video-codec/mwv207/mwv207_video_dec_reg.c',
        'video-codec/mwv207/mwv207_video_dec_vpufeature.c',
        'video-codec/mwv207/mwv207_dec_ppu.c',
        'video-codec/mwv207/mwv207_dec_h264.c',
        'video-codec/mwv207/mwv207_dec_hevc.c',
        'video-codec/mwv207/mwv207_dec_vp9.c',
        'video-codec/common/mwv207_decoder_h264_nal_unit.c',
        'video-codec/common/mwv207_decoder_h264_slice_header.c',
        'video-codec/common/mwv207_decoder_hevc_exp_golomb.c',
        'video-codec/common/mwv207_decoder_hevc_slice_header.c',
        'video-codec/common/mwv207_decoder_vp9_slice_header.c',
        'video-codec/common/mwv207_vp9hwd_bool.c',
)


libmwv207 = static_library(
  'mwv207',
  files_libmwv207,
  include_directories : [inc_include, inc_src, inc_gallium, inc_gallium_aux, inc_mwv207_drm, inc_mwv207, inc_mwv207_nir, inc_mwv207_video_codec],
  dependencies: [idep_nir_headers],
)

mwv207_compiler = executable(
  'mwv207_compiler',
  'nir/mwv207_cmdline.c',
  include_directories : [
    inc_include,
    inc_mwv207_drm,
    inc_src,
    inc_gallium,
    inc_gallium_aux,
    inc_mwv207_nir,
    inc_mwv207,
 ],
  dependencies : [
    idep_nir,
    idep_mesautil,
	dep_libdrm,
  ],
  link_with : [
    libglsl_standalone,
	libgallium,
	libmwv207,
  ],
  build_by_default : with_tools.contains('mwv207'),
)

driver_mwv207 = declare_dependency(
  compile_args : ['-DGALLIUM_MWV207'],
  dependencies: [],
  link_with : [libmwv207, libmwv207winsys],
)
