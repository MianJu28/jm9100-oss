#!/bin/bash
# VP9 跨尺寸参考（B2）诊断 —— UNRESOLVED §1.8 / I-6 候选
#
# 取两族官方测试向量：
#   vp90-2-14-resize-10frames-fp-tiles-1-2.webm   普通 resize + tile，应当正确
#   vp90-2-21-resize_inter_320x180_5_1-2.webm     参考尺寸变化，触发驱动的保护性跳过
# 各自用 libvpx 软件解码与 mwv207 硬件解码，逐字节比对，并收集驱动打印的
# "reference ... has no decoded picture; frame skipped" 行数。
#
# I-6 修复后（2026-11-10），被跳过的帧不再静默返回脏图：驱动会通过
# get_decoder_fence() 让 vaSyncSurface() 报错，ffmpeg 侧表现为
# "Failed to sync surface ..." 并停止输出。所以本脚本的判据是：
#   skipped=0 → 必须 IDENTICAL；
#   skipped>0 → 硬件日志里必须出现 "Failed to sync surface"（否则就是又退回
#                "跳过但仍出图" 的静默行为）。

set -u

BASE="https://storage.googleapis.com/downloads.webmproject.org/test_data/libvpx"
WORK="${TMPDIR:-/tmp}/vp9-resize"
SRC=""

while [ $# -gt 0 ]; do
    case "$1" in
        --local) SRC="$2"; shift 2 ;;
        *) echo "用法: $0 [--local DIR]" >&2; exit 2 ;;
    esac
done

mkdir -p "$WORK"
chmod 0777 "$WORK" 2>/dev/null

if ! command -v ffmpeg >/dev/null; then
    echo "需要 ffmpeg" >&2
    exit 1
fi

decode() { # <样本> <输出前缀>
    local sample=$1 prefix=$2
    ffmpeg -v error -y -c:v libvpx-vp9 -i "$sample" \
           -f rawvideo -pix_fmt yuv420p "$prefix-sw.yuv" >"$prefix-sw.log" 2>&1
    ffmpeg -v error -y -hwaccel vaapi -hwaccel_output_format vaapi -i "$sample" \
           -vf hwdownload,format=nv12 -f rawvideo -pix_fmt yuv420p \
           "$prefix-hw.yuv" >"$prefix-hw.log" 2>&1

    local sw hw verdict
    sw=$(stat -c%s "$prefix-sw.yuv" 2>/dev/null || echo 0)
    hw=$(stat -c%s "$prefix-hw.yuv" 2>/dev/null || echo 0)
    if [ "$sw" = "$hw" ] && [ "$sw" != 0 ] && cmp -s "$prefix-sw.yuv" "$prefix-hw.yuv"; then
        verdict=IDENTICAL
    else
        verdict=DIFF
    fi

    local skipped
    skipped=$(grep -c "has no decoded picture" "$prefix-hw.log" 2>/dev/null || true)

    if [ "${skipped:-0}" != 0 ]; then
        if grep -q "Failed to sync surface" "$prefix-hw.log"; then
            verdict=SKIPPED-AS-ERROR
        else
            verdict=SKIPPED-MUTE   # I-6 复发：跳过但应用毫无察觉
        fi
    fi

    printf '%-52s sw=%-9s hw=%-9s %-16s 跳过帧=%s\n' \
           "$(basename "$sample")" "$sw" "$hw" "$verdict" "${skipped:-0}"

    if [ "$skipped" != 0 ]; then
        echo "    驱动日志（前 2 行）："
        grep "has no decoded picture" "$prefix-hw.log" | head -2 | sed 's/^/      /'
    fi
    if [ -s "$prefix-sw.log" ]; then
        echo "    libvpx 告警（前 2 行）："
        head -2 "$prefix-sw.log" | sed 's/^/      /'
    fi
}

for sample in vp90-2-14-resize-10frames-fp-tiles-1-2.webm \
              vp90-2-21-resize_inter_320x180_5_1-2.webm; do
    path="$SRC/$sample"
    if [ ! -f "$path" ]; then
        path="$WORK/$sample"
        if [ ! -f "$path" ]; then
            echo "== 下载 $sample =="
            curl -sSLf -o "$path" "$BASE/$sample" || {
                echo "下载失败：$BASE/$sample" >&2
                exit 1
            }
        fi
    fi
    decode "$path" "$WORK/${sample%.webm}"
done

echo
echo "说明：SKIPPED-AS-ERROR 表示该向量里的跨尺寸参考帧被驱动按设计拒绝，"
echo "      并且失败已经传到应用（vaSyncSurface 报错）；SKIPPED-MUTE 则说明"
echo "      I-6（跳过却静默返回旧内容）又回来了。"
