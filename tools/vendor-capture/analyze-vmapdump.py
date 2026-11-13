#!/usr/bin/env python3
"""分析 vmapdump 抓到的映射内容，找出厂商 JPEG 解码用的辅助表与 ECS 缓冲。

用法：
  tools/vendor-capture/analyze-vmapdump.py <dumpdir> [jpeg文件]

判据：
  1) 辅助表：544 字节区域，与"从 JPEG 的 DQT/DHT 段重建的 ST 布局表"逐字节
     相同（会尝试 4 种字节序/排列变体，报告命中的变体）。
  2) ECS：包含 JPEG 熵编码数据起始若干字节（默认取 ECS 头 32 字节）的区域。
"""
import re
import sys
import struct

ZIGZAG = [0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
          12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
          35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
          58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63]


def parse_jpeg(path):
    data = open(path, 'rb').read()
    i, dqt, dht, sof, sos = 2, {}, {}, None, None
    while i < len(data):
        if data[i] != 0xFF:
            i += 1
            continue
        m = data[i + 1]
        i += 2
        if m in (0xD8, 0xD9) or m == 0x01 or 0xD0 <= m <= 0xD7:
            continue
        L = (data[i] << 8) | data[i + 1]
        seg = data[i + 2:i + L]
        if m == 0xDB:
            p = 0
            while p < len(seg):
                prec = seg[p] >> 4
                tq = seg[p] & 15
                p += 1
                n = 64 * (2 if prec else 1)
                dqt[tq] = list(seg[p:p + n])
                p += n
        elif m == 0xC4:
            p = 0
            while p < len(seg):
                tc, th = seg[p] >> 4, seg[p] & 15
                p += 1
                bits = list(seg[p:p + 16])
                p += 16
                n = sum(bits)
                dht[(tc, th)] = (bits, list(seg[p:p + n]))
                p += n
        elif m in (0xC0, 0xC1, 0xC2):
            h = (seg[1] << 8) | seg[2]
            w = (seg[3] << 8) | seg[4]
            nc = seg[5]
            comps = []
            for c in range(nc):
                comps.append((seg[6 + c * 3], seg[7 + c * 3] >> 4,
                              seg[7 + c * 3] & 15, seg[8 + c * 3]))
            sof = (w, h, comps)
        elif m == 0xDA:
            ns = seg[0]
            sos = [(seg[1 + c * 2], seg[2 + c * 2] >> 4, seg[2 + c * 2] & 15)
                   for c in range(ns)]
            ecs_off = i + L
            return data, dqt, dht, sof, sos, ecs_off
        i += L
    raise SystemExit('no SOS in ' + path)


def build_st_table(dqt, dht, sof, sos):
    """按 ST 的 hantro_jpeg_prepare_vlc_hw_table() 布局重建 544 B 表。"""
    aux = bytearray(544)
    comps = sof[2]
    for t in range(3):
        src = 0 if t == 0 else min(comps[t][3], 3) if t < len(comps) else 0
        if src not in dqt:
            src = 0
        for i2 in range(64):
            aux[t * 64 + ZIGZAG[i2]] = dqt[src][i2]
    ac = {k[1]: v[1] for k, v in dht.items() if k[0] == 1}
    dc = {k[1]: v[1] for k, v in dht.items() if k[0] == 0}
    ac0, dc0 = sos[0][2], sos[0][1]
    others = sorted(k for k in ac if k != ac0)
    a1, a2 = (ac0, others[0]) if ac0 == 1 else (others[0], ac0)
    others = sorted(k for k in dc if k != dc0)
    d1, d2 = (dc0, others[0]) if dc0 == 1 else (others[0], dc0)
    aux[192:192 + 162] = bytes(ac[a1] + [0] * (162 - len(ac[a1])))[:162]
    aux[354:354 + 162] = bytes(ac[a2] + [0] * (162 - len(ac[a2])))[:162]
    aux[516:516 + 12] = bytes(dc[d1] + [0] * (12 - len(dc[d1])))[:12]
    aux[528:528 + 12] = bytes(dc[d2] + [0] * (12 - len(dc[d2])))[:12]
    return bytes(aux)


def variants(aux):
    out = {}
    out['plain'] = aux
    out['swab32'] = b''.join(aux[i:i + 4][::-1] for i in range(0, len(aux), 4))
    out['swab16'] = b''.join(aux[i:i + 2][::-1] for i in range(0, len(aux), 2))
    # AC/DC 两组交换
    v = bytearray(aux)
    v[192:354], v[354:516] = aux[354:516], aux[192:354]
    out['ac_swap'] = bytes(v)
    # 不带 zigzag 还原
    nat = bytearray(aux)
    return out


def main():
    dumpdir = sys.argv[1]
    jpeg = sys.argv[2] if len(sys.argv) > 2 else \
        '/home/admin/vp9-assets/t1080.jpg'
    data, dqt, dht, sof, sos, ecs_off = parse_jpeg(jpeg)
    aux = build_st_table(dqt, dht, sof, sos)
    exp = variants(aux)
    ecs_head = data[ecs_off:ecs_off + 32]
    print('jpeg=%s %dx%d ecs_off=%d ecs_head=%s' %
          (jpeg, sof[0], sof[1], ecs_off, ecs_head.hex()))

    idx = []
    for line in open(dumpdir + '/index.txt'):
        if line.startswith('#') or not line.strip():
            continue
        f = line.split()
        idx.append((f[0], int(f[2])))
    print('dumped regions:', len(idx))

    hits = 0
    for name, size in idx:
        path = '%s/%s_%s.bin' % (dumpdir, name, '')
        import glob
        cand = glob.glob('%s/%s_*' % (dumpdir, name))
        if not cand:
            continue
        blob = open(cand[0], 'rb').read()
        for vn, want in exp.items():
            at = blob.find(want)
            if at >= 0:
                print('AUX-TABLE match [%s] blob=%d table_at=+%d file=%s' %
                      (vn, size, at, cand[0]))
                hits += 1
        pos = blob.find(ecs_head)
        if pos >= 0:
            print('ECS head found at +%d (size=%d file=%s)' %
                  (pos, size, cand[0]))
            hits += 1
    if not hits:
        print('no hit; 可用 dumpdir 里的 .bin 手工比对')
    return 0


if __name__ == '__main__':
    sys.exit(main())