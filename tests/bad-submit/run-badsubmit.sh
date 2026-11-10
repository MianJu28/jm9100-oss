#!/bin/bash
# 畸形提交防御测试。会读 /dev/dri/renderD128 并调用 submit ioctl（全部用例都
# 在拿硬件前被拒）。建议在重启启用 53b3229 之后跑；在更早的模块上跑同样安全，
# 因为这些用例走的都是既有的参数校验分支。
set -u
cd "$(dirname "$0")"
bin=${TMPDIR:-/tmp}/mwv207-badsubmit
cc -O2 -I/usr/include/libdrm -o "$bin" badsubmit.c -ldrm || exit 1
"$bin" "$@"
rc=$?
rm -f "$bin"
exit $rc
