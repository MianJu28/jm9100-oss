#!/bin/bash
# 遮挡查询 / 条件渲染 回归测试
# 用法: DRI_DIR=<dri 目录> ./run-query-tests.sh
#   DRI_DIR 既可以是安装后的 DRI 目录，也可以是 `tools/build-umd.sh sim` 造出来的
#   模拟目录（后者同时含 libGL/libEGL，需要一并走 LD_LIBRARY_PATH，否则加载器会
#   拿系统 libGL 来比版本戳 → DRI driver not from this Mesa build → 全部 FAIL）。
set -u
DRI_DIR=${DRI_DIR:-/tmp/mwv207-sim}
export LIBGL_DRIVERS_PATH="$DRI_DIR" MESA_LOADER_DRIVER_OVERRIDE=mwv207
if [ -f "$DRI_DIR/libGL.so.1" ]; then
   export LD_LIBRARY_PATH="$DRI_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
cd "$(dirname "$0")" || exit 1
pass=0; fail=0
chk(){ if [ "$2" = "$3" ]; then echo "  [PASS] $1 = $2"; pass=$((pass+1)); else echo "  [FAIL] $1 = $2 (期望 $3)"; fail=$((fail+1)); fi; }

# 编译
for f in occlusion occlusion-scale condrender; do
  cc -o "/tmp/qt-$f" "$f.c" -lGL -lX11 -lm 2>/dev/null || { echo "编译 $f 失败"; exit 1; }
done

echo "=== 遮挡查询 ==="
sp=$(/tmp/qt-occlusion 0 2>/dev/null | sed -n 's/.*GL_SAMPLES_PASSED=\([0-9]*\).*/\1/p' | head -1)
chk "GL_SAMPLES_PASSED" "$sp" "4096"
any=$(/tmp/qt-occlusion 1 2>/dev/null | sed -n 's/.*GL_ANY_SAMPLES_PASSED=\([0-9]*\).*/\1/p')
chk "GL_ANY_SAMPLES_PASSED" "$any" "1"

echo "=== 遮挡计数随几何面积变化（证明硬件真在计数）==="
s=$(/tmp/qt-occlusion-scale 2>/dev/null)
q=$(echo "$s"|sed -n 's/.*scale=0.25 *count=\([0-9]*\).*/\1/p')
h=$(echo "$s"|sed -n 's/.*scale=0.50 *count=\([0-9]*\).*/\1/p')
f=$(echo "$s"|sed -n 's/.*scale=1.00 *count=\([0-9]*\).*/\1/p')
chk "1/4 < 半 < 全" "$([ "$q" -lt "$h" ] 2>/dev/null && [ "$h" -lt "$f" ] 2>/dev/null && echo yes || echo no)" "yes"
echo "  实测: 1/4=$q 半=$h 全=$f"

echo "=== 条件渲染（此前 SIGSEGV）==="
/tmp/qt-condrender >/dev/null 2>&1; chk "condrender 不崩溃" "$?" "0"

echo
echo "PASS=$pass FAIL=$fail"
[ "$fail" -eq 0 ]
