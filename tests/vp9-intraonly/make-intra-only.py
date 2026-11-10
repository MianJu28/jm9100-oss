#!/usr/bin/env python3
"""把 VP9 关键帧改写成内容等价的 intra-only 隐藏帧（B3 样片构造器）。

背景：本机 libvpx 编码器产不出 intra-only 非关键帧（`auto-alt-ref` 产生的隐藏帧
全是 inter 帧），UNRESOLVED B3 只能靠外部样片。既然关键帧的压缩头 + tile 数据与
intra-only 帧是同一套编码工具，就可以把“未压缩头”换掉：保留原关键帧的压缩头与
tile 负载，只重写前面的未压缩头，得到一个合法的 intra-only 隐藏帧。

改写规则（Profile 0，见 common/mwv207_decoder_vp9_slice_header.c 与
libvpx vp9_decodeframe.c read_uncompressed_header()）：

    关键帧       marker(2) ver(1) prof(1) show_existing(1) key_bit=0 show=1 er
                 sync_code(24) color(4) frame_size(33)
    intra-only   marker(2) ver(1) prof(1) show_existing(1) key_bit=1 show=0 er
                 intra_only(1) reset_frame_context(2) sync_code(24)
                 refresh_frame_flags(8) frame_size(33)

  * Profile 0 的 intra-only 未压缩头**不带** color config（默认 BT.601/studio/420/8bit）；
  * `reset_frame_context` 取 3（重置全部上下文），否则首帧会因熵上下文未初始化而被
    libvpx 拒绝（"Uninitialized entropy context"）。

未压缩头 + 压缩头的**位数**必须逐位对上，tile 数据才不会被错位。压缩头的位数由
驱动解析器给出（`frame_tag_size` 字节数减去未压缩头的 69 位），所以脚本用
`vp9-parse-probe` 先量一遍，再在 8 个候选位宽里挑出能被解析器确认的那一个。

用法：
    ./make-intra-only.py 输入关键帧.ivf 输出.ivf [--refresh 0x01] [--probe 路径]

输出文件里是两帧：改写后的 intra-only 隐藏帧 + 一个单字节 show_existing 帧
（`0x88`，指向参考槽 0），这样普通播放器/ffmpeg 才会把隐藏帧显示出来。
"""
import argparse
import os
import struct
import subprocess
import sys

UNCOMPRESSED_KEYFRAME_BITS = 69  # marker2+ver1+prof1+showex1+key1+show1+er1+sync24+color4+w16+h16+render1
UNCOMPRESSED_INTRAONLY_BITS = 76  # marker2+ver1+prof1+showex1+key1+show1+er1+intra1+rfc2+sync24+refresh8+w16+h16+render1
SYNC_CODE = 0x498342
SHOW_EXISTING_SLOT0 = 0x88  # marker=10, version=0, profile=0, show_existing=1, idx=0


def read_ivf(path):
    data = open(path, "rb").read()
    if data[:4] != b"DKIF":
        sys.exit("%s: 不是 IVF 文件" % path)
    width, height = struct.unpack_from("<HH", data, 12)
    size = struct.unpack_from("<I", data, 32)[0]
    return data[32 + 12:32 + 12 + size], width, height


def write_ivf(path, packets, width, height):
    out = bytearray(b"DKIF" + struct.pack("<HH", 0, 32) + b"VP90" +
                    struct.pack("<HH", width, height) +
                    struct.pack("<II", 30, 1) +
                    struct.pack("<I", len(packets)) + struct.pack("<I", 0))
    for index, packet in enumerate(packets):
        out += struct.pack("<IQ", len(packet), index) + packet
    open(path, "wb").write(out)


def to_bits(data):
    return [(byte >> (7 - i)) & 1 for byte in data for i in range(8)]


def to_bytes(bits):
    bits = bits + [0] * ((8 - len(bits) % 8) % 8)
    return bytes(int("".join(map(str, bits[i:i + 8])), 2)
                 for i in range(0, len(bits), 8))


def parse(probe, packet_path):
    out = subprocess.run([probe, packet_path], capture_output=True, text=True)
    fields = {}
    for token in out.stdout.split():
        if "=" in token:
            key, value = token.split("=", 1)
            fields[key] = int(value, 0)
    if not fields:
        sys.exit("解析探针没有输出可用的字段：%s" % out.stderr.strip())
    return fields


def build(keyframe, compressed_bits, refresh_bits, total_compressed_bits):
    bits = to_bits(keyframe)
    header = [1, 0]                 # frame_marker = 10
    header += [0, 0, 0]             # version, profile, show_existing_frame
    header += [1]                   # key_bit = 1 → 非关键帧
    header += [0]                   # show_frame = 0
    header += [0]                   # error_resilient = 0
    header += [1]                   # intra_only = 1
    header += [1, 1]                # reset_frame_context = 3（重置全部上下文）
    header += [(SYNC_CODE >> (23 - i)) & 1 for i in range(24)]
    header += refresh_bits
    header += bits[36:69]           # frame_size + render_size，与原关键帧一致
    assert len(header) == UNCOMPRESSED_INTRAONLY_BITS, len(header)

    body = header + bits[UNCOMPRESSED_KEYFRAME_BITS:
                         UNCOMPRESSED_KEYFRAME_BITS + total_compressed_bits]
    body += [0] * ((8 - len(body) % 8) % 8)
    tile_offset = (UNCOMPRESSED_KEYFRAME_BITS + total_compressed_bits + 7) // 8
    return to_bytes(body) + keyframe[tile_offset:]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input", help="只含关键帧的 .ivf（或 -f ivf 编出来的单帧文件）")
    ap.add_argument("output", help="输出 .ivf")
    ap.add_argument("--refresh", default="0xff",
                    help="refresh_frame_flags（默认 0xff；官方样片用 0x01）")
    ap.add_argument("--probe", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                    "vp9-parse-probe"))
    args = ap.parse_args()

    if not os.access(args.probe, os.X_OK):
        sys.exit("找不到解析探针 %s，先跑 build-probe.sh" % args.probe)

    keyframe, width, height = read_ivf(args.input)
    tmp = args.output + ".src.bin"
    open(tmp, "wb").write(keyframe)
    fields = parse(args.probe, tmp)
    os.unlink(tmp)

    if not fields.get("key"):
        sys.exit("输入帧不是关键帧（key=%s），无法改写" % fields.get("key"))
    if fields.get("prof"):
        sys.exit("只支持 Profile 0（该帧 profile=%s）" % fields.get("prof"))

    tag_bytes = fields["tag"]
    # 压缩头位数 = 头总字节 * 8 - 未压缩头 69 位 - 尾部填充（0..7 位）。
    candidates = [tag_bytes * 8 - UNCOMPRESSED_KEYFRAME_BITS - pad for pad in range(8)]

    refresh_value = int(args.refresh, 0)
    refresh_bits = [(refresh_value >> (7 - i)) & 1 for i in range(8)]

    for compressed_bits in candidates:
        packet = build(keyframe, None, refresh_bits, compressed_bits)
        open(tmp, "wb").write(packet)
        result = parse(args.probe, tmp)
        os.unlink(tmp)
        expected_tag = (UNCOMPRESSED_INTRAONLY_BITS + compressed_bits + 7) // 8
        if (result.get("intra") == 1 and result.get("key") == 0 and
                result.get("w") == width and result.get("h") == height and
                result.get("refresh") == refresh_value and
                result.get("tag") == expected_tag):
            write_ivf(args.output, [packet, bytes([SHOW_EXISTING_SLOT0])], width, height)
            print("%s: %d×%d, refresh=0x%02x, 压缩头 %d 位, 共 %d 字节"
                  % (args.output, width, height, refresh_value,
                     compressed_bits, len(packet)))
            return

    sys.exit("8 个候选位宽都没构造出可被解析器确认的 intra-only 帧")


if __name__ == "__main__":
    main()