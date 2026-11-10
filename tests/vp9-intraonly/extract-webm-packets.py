#!/usr/bin/env python3
"""从 WebM 里抽出 VP9 包并写成 IVF。

`ffmpeg -c copy -f ivf` 对含隐藏帧 / 单字节 show_existing 的 WebM 会产出空文件，
所以这里按 EBML 结构自己遍历 Segment → Cluster → SimpleBlock/BlockGroup。

用法：extract-webm-packets.py <input.webm> <work-dir> [width] [height]
产物：<work-dir>/pkt_<n>.bin、<work-dir>/full.ivf、<work-dir>/only0.ivf
"""

import os
import struct
import sys


def read_vint(d, i, keep_marker=False):
    b = d[i]
    length, mask = 1, 0x80
    while length <= 8 and not (b & mask):
        mask >>= 1
        length += 1
    if length > 8:
        raise ValueError('bad vint')
    val = b & (mask - 1) if not keep_marker else b
    for k in range(1, length):
        val = (val << 8) | d[i + k]
    return val, i + length


def elements(d, start, end):
    i = start
    while i < end:
        eid, i2 = read_vint(d, i, keep_marker=True)
        size, i3 = read_vint(d, i2)
        if size == 0 or size == (1 << 56) - 1:
            yield eid, i3, end, True
            return
        yield eid, i3, i3 + size, False
        i = i3 + size


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    src, work = sys.argv[1], sys.argv[2]
    width = int(sys.argv[3]) if len(sys.argv) > 3 else 352
    height = int(sys.argv[4]) if len(sys.argv) > 4 else 288

    d = open(src, 'rb').read()
    pkts = []
    for eid, s, e, _u in elements(d, 0, len(d)):
        if eid != 0x18538067:             # Segment
            continue
        for eid2, s2, e2, _u2 in elements(d, s, e):
            if eid2 != 0x1F43B675:        # Cluster
                continue
            for eid3, s3, e3, _u3 in elements(d, s2, e2):
                if eid3 == 0xA3:          # SimpleBlock
                    _trk, j = read_vint(d, s3)
                    pkts.append(d[j + 3:e3])
                elif eid3 == 0xA0:        # BlockGroup
                    for eid4, s4, e4, _u4 in elements(d, s3, e3):
                        if eid4 == 0xA1:  # Block
                            _trk, j = read_vint(d, s4)
                            pkts.append(d[j + 3:e4])

    if not pkts:
        sys.exit('未能抽出 VP9 包')

    for n, p in enumerate(pkts):
        open(os.path.join(work, 'pkt_%d.bin' % n), 'wb').write(p)
    print('抽出 %d 个包：%s' % (len(pkts), [len(p) for p in pkts]))

    def write_ivf(path, frames):
        out = bytearray(b'DKIF' + struct.pack('<HH', 0, 32) + b'VP90' +
                        struct.pack('<HH', width, height) +
                        struct.pack('<II', 30, 1) +
                        struct.pack('<I', len(frames)) + struct.pack('<I', 0))
        for n, p in enumerate(frames):
            out += struct.pack('<IQ', len(p), n) + p
        open(path, 'wb').write(out)

    write_ivf(os.path.join(work, 'full.ivf'), pkts)
    write_ivf(os.path.join(work, 'only0.ivf'), pkts[:1])


if __name__ == '__main__':
    main()