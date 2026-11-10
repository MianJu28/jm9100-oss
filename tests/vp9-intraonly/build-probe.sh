#!/bin/sh
# 编译 intra-only 样片构造器需要的解析探针。
#
# 探针直接链接驱动自带的 VP9 切片头解析器（common/ 下的四个 .c），
# 这样“这段位流被解析成什么”永远与驱动自己的判断一致。
set -e

here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
common="$root/src/mwv207-umd/drivers/mwv207/video-codec/common"

out=${1:-$here/vp9-parse-probe}

cc -O1 -I"$common" -o "$out" "$here/vp9-parse-probe.c" \
   "$common/mwv207_decoder_vp9_slice_header.c" \
   "$common/mwv207_vp9hwd_bool.c" \
   "$common/mwv207_decoder_sw_stream.c" \
   "$common/mwv207_decoder_sw_util.c"

echo "已生成 $out"