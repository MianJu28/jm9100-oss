#!/bin/bash
# VP9 intra-only 隐藏帧诊断（B3，UNRESOLVED I-5）
#
# 取 WebM 官方测试向量 vp90-2-16-intra-only.webm，抽出 VP9 包，分别用 libvpx
# 软件解码与 mwv207 硬件解码，逐帧比对，并留下首帧 PNG 供目视。
#
# 这是诊断工具：在 I-5 修好之前，"解码帧不一致"就是已知状态，脚本不判 PASS/FAIL。

set -u

SAMPLE="vp90-2-16-intra-only.webm"
URL="https://storage.googleapis.com/downloads.webmproject.org/test_data/libvpx/$SAMPLE"
MD5="70d4afd231a41dd0f0364fec8121fa09"

WORK="${TMPDIR:-/tmp}/vp9-intraonly"
EVID="$WORK/evidence"
SRC="$WORK"

while [ $# -gt 0 ]; do
    case "$1" in
        --local) SRC="$2"; shift 2 ;;
        *) echo "用法: $0 [--local DIR]" >&2; exit 2 ;;
    esac
done

mkdir -p "$WORK" "$EVID"
# 以前用 root 跑过会让普通用户无法再写
chmod 0777 "$WORK" "$EVID" 2>/dev/null

SAMPLE_PATH="$SRC/$SAMPLE"
if [ ! -f "$SAMPLE_PATH" ]; then
    echo "== 下载 $SAMPLE =="
    curl -sSL -o "$SAMPLE_PATH" "$URL" || { echo "下载失败：$URL"; exit 1; }
fi

got=$(md5sum "$SAMPLE_PATH" | cut -d' ' -f1)
if [ "$got" != "$MD5" ]; then
    echo "样片 md5 不符：期望 $MD5，实际 $got" >&2
    exit 1
fi
echo "样片：$(stat -c%s "$SAMPLE_PATH") 字节，md5 $got"

if ! command -v ffmpeg >/dev/null || ! command -v ffprobe >/dev/null; then
    echo "需要 ffmpeg/ffprobe" >&2
    exit 1
fi

# --- 抽包：ffmpeg -c copy -f ivf 对含隐藏帧/单字节 show_existing 的 WebM 会得到空文件，
#     这里按 EBML 解析 Cluster/SimpleBlock/BlockGroup。 ---
python3 "$(dirname "$0")/extract-webm-packets.py" "$SAMPLE_PATH" "$WORK" || exit 1

cd "$WORK" || exit 1

decode_sw() {   # decode_sw <label> <ivf>
    local label="$1" ivf="$2" out="$WORK/$1.yuv"
    if ! ffmpeg -v error -y -c:v libvpx-vp9 -i "$ivf" \
            -f rawvideo -pix_fmt yuv420p "$out"; then
        echo "  [$label] 解码失败"
        return 1
    fi
    echo "  [$label] $(stat -c%s "$out") 字节"
}

decode_hw() {   # decode_hw <label> <ivf>
    local label="$1" ivf="$2" out="$WORK/$1.yuv"
    if ! ffmpeg -v error -y -hwaccel vaapi -hwaccel_output_format vaapi \
            -i "$ivf" -vf hwdownload,format=nv12 \
            -f rawvideo -pix_fmt yuv420p "$out"; then
        echo "  [$label] 解码失败"
        return 1
    fi
    echo "  [$label] $(stat -c%s "$out") 字节"
}

compare() {  # compare <name> <sw.yuv> <hw.yuv>
    python3 - "$1" "$2" "$3" <<'PY'
import sys, os
name, swp, hwp = sys.argv[1], sys.argv[2], sys.argv[3]
w, h = 352, 288
fs = w * h * 3 // 2
sw = open(swp, 'rb').read(); hw = open(hwp, 'rb').read()
ns, nh = len(sw)//fs, len(hw)//fs
print("  %s: 软件 %d 帧 / 硬件 %d 帧" % (name, ns, nh))
for i in range(min(ns, nh)):
    a = sw[i*fs:(i+1)*fs]; b = hw[i*fs:(i+1)*fs]
    if a == b:
        print("    frame %d: IDENTICAL" % i)
    else:
        d = [abs(a[k]-b[k]) for k in range(0, fs, 11)]
        print("    frame %d: DIFF  Y≈max %d mean %.1f" % (i, max(d), sum(d)/len(d)))
PY
}

echo
echo "== 单帧 intra-only（最小复现） =="
decode_sw sw_only0 only0.ivf
decode_hw hw_only0 only0.ivf || true
compare "only0" "$WORK/sw_only0.yuv" "$WORK/hw_only0.yuv"

echo
echo "== 完整 7 包 =="
decode_sw sw_full full.ivf
decode_hw hw_full full.ivf || true
compare "full" "$WORK/sw_full.yuv" "$WORK/hw_full.yuv"

for t in sw_only0 hw_only0; do
    [ -f "$WORK/$t.yuv" ] || continue
    ffmpeg -v error -y -f rawvideo -pix_fmt yuv420p -s 352x288 -i "$WORK/$t.yuv" \
        -frames:v 1 "$EVID/${t}.png" 2>/dev/null
done

echo
echo "证据图片：$EVID（sw_only0.png / hw_only0.png 可直接目视对比）"
echo "说明见 docs/UNRESOLVED.md 的 I-5 与 $(dirname "$0")/README.md"