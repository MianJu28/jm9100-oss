#!/usr/bin/env bash
# GL/GLES 回归：把仓库里的图形探针串起来跑一遍，给出 PASS/FAIL 汇总。
# 覆盖：查询/条件渲染、FBO、纹理上传正确性与吞吐、buffer 读写、回读。
#
# 用法:
#   tools/gl-regression.sh [探针输出目录]
#
# 环境变量:
#   DRI_DIR     指定驱动目录（例如 /home/admin/dev-dri 跑开发构建，
#               或 /usr/local/lib/mwv207-oss 跑安装栈）；默认用系统安装的。
#   GL_BIN_DIR  探针二进制所在目录，默认 /home/admin/dev-bin。
#   GL_DURATION glmark2 每个场景秒数，默认 2（设为 0 跳过 glmark2）。
set -u

DIR=${DRI_DIR:-}
BIN=${GL_BIN_DIR:-/home/admin/dev-bin}
DUR=${GL_DURATION:-2}
PASS=0
FAIL=0
SKIP=0
FAILED=()

if [ -n "$DIR" ]; then
   export LIBGL_DRIVERS_PATH=$DIR
   export MESA_LOADER_DRIVER_OVERRIDE=mwv207
fi
export DISPLAY=${DISPLAY:-:0}
export XAUTHORITY=${XAUTHORITY:-/home/admin/.Xauthority}

report() { # $1=名称 $2=结果(0 通过)
   if [ "$2" = 0 ]; then echo "OK   $1"; PASS=$((PASS+1));
   else echo "FAIL $1"; FAIL=$((FAIL+1)); FAILED+=("$1"); fi
}

echo "== 驱动: ${DIR:-<系统安装>}  探针: $BIN"

# 1. GL 查询 / 条件渲染
if [ -x tests/query/run-query-tests.sh ]; then
   if out=$(DRI_DIR="$DIR" tests/query/run-query-tests.sh 2>&1); then
      echo "$out" | tail -2
      report "query" 0
   else
      echo "$out" | tail -5; report "query" 1
   fi
else
   echo "SKIP query（脚本不存在）"; SKIP=$((SKIP+1))
fi

# 2. FBO 完整性（首次运行时自动构建）
if [ ! -x tests/fbo-test ] && [ -f tests/fbo-test.c ] &&
   cc -O2 -o tests/fbo-test tests/fbo-test.c -lEGL -lGLESv2 2>/dev/null; then
   :
fi
if [ -x tests/fbo-test ]; then
   if out=$(tests/fbo-test 2>&1); then echo "$out" | tail -2; report "fbo" 0;
   else echo "$out" | tail -5; report "fbo" 1; fi
else
   echo "SKIP fbo（未构建）"; SKIP=$((SKIP+1))
fi

# 3. 纹理上传正确性（12 轮 + 8x4 压力）
if [ -x "$BIN/texcheck" ]; then
   if out=$("$BIN/texcheck" 2>&1) && echo "$out" | grep -q "RESULT: PASS"; then
      echo "$out" | tail -1; report "texcheck" 0
   else echo "$out" | tail -5; report "texcheck" 1; fi
else
   echo "SKIP texcheck（未构建）"; SKIP=$((SKIP+1))
fi

# 4. 纹理上传吞吐（固定开销应在 0.1 ms 量级）
if [ -x "$BIN/texbench" ]; then
   out=$("$BIN/texbench" 2048 64 40 8 2>&1 | tail -1)
   echo "$out"
   per=$(echo "$out" | sed -n 's/.*per=\([0-9.]*\) ms.*/\1/p')
   if [ -n "$per" ] && awk "BEGIN{exit !($per < 1.0)}"; then report "texbench" 0;
   else report "texbench(per=$per)" 1; fi
else
   echo "SKIP texbench（未构建）"; SKIP=$((SKIP+1))
fi

# 5. buffer CPU 侧读写（GTT 放置后应 >= 5 GB/s）
if [ -x "$BIN/bufbench" ]; then
   out=$("$BIN/bufbench" 20 2>&1 | tail -3)
   echo "$out"
   worst=$(echo "$out" | sed -n 's/.*( *\([0-9.]*\) GB\/s).*/\1/p' | sort -n | head -1)
   if [ -n "$worst" ] && awk "BEGIN{exit !($worst > 5.0)}"; then report "bufbench" 0;
   else report "bufbench(${worst:-?}GB/s)" 1; fi
else
   echo "SKIP bufbench（未构建）"; SKIP=$((SKIP+1))
fi

# 6. 回读（像素值与错码；带宽受内存限制，只查正确性）
# 双源混合正确性（火狐 LCD 次像素文字走这条路；硬件无 SRC1 因子，靠
# lower_dual 把混合降级进片元着色器，必须逐例与 GL 规格一致）
if [ -x "$BIN/dualblend" ]; then
   out=$("$BIN/dualblend" 2>&1 | grep -E "RESULT|FAIL" | tail -3)
   if echo "$out" | grep -q "RESULT: PASS"; then report "dualblend" 0;
   else echo "$out"; report "dualblend" 1; fi
else
   echo "SKIP dualblend（未构建: cc -O2 -o ~/dev-bin/dualblend tests/gl-probe/dualblend.c -lEGL -lGLESv2 -lm）"
   SKIP=$((SKIP+1))
fi

if [ -x "$BIN/rbbench" ]; then
   out=$("$BIN/rbbench" 10 2>&1 | grep "1024x1024" | tail -1)
   echo "$out"
   if echo "$out" | grep -q "err=0x0 px=26,51,76"; then report "rbbench" 0;
   else report "rbbench" 1; fi
else
   echo "SKIP rbbench（未构建）"; SKIP=$((SKIP+1))
fi

# 7. glmark2 全套（可选）
if [ "${DUR}" != "0" ] && command -v glmark2 >/dev/null 2>&1; then
   score=$(timeout 900 glmark2 -b ":duration=$DUR" 2>&1 | sed -n 's/^.*glmark2 Score: //p' | tail -1)
   echo "glmark2 score: ${score:-?}"
   if [ -n "$score" ]; then report "glmark2" 0; else report "glmark2" 1; fi
else
   echo "SKIP glmark2"; SKIP=$((SKIP+1))
fi

echo
echo "== 结果: pass=$PASS fail=$FAIL skip=$SKIP"
[ "$FAIL" = 0 ] || { echo "失败项: ${FAILED[*]}"; exit 1; }
