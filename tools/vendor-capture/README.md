# vendor-capture — 记录厂商 UMD 的内核命令流

用途：在厂商栈上跑一次解码/绘制，把进程发出去的 ioctl 缓冲区落盘，用来反推厂商的
寄存器序列（例如 JPEG 解码需要的启动位）。起因见 `docs/UNRESOLVED.md` 的
"JPEG baseline 解码调研"一节。

```
cc -shared -fPIC -O2 -o vcap.so vcap.c -ldl

VCAP_DIR=/tmp/vcap LD_PRELOAD=$PWD/vcap.so \
  ffmpeg -hwaccel vaapi -vaapi_device /dev/dri/renderD128 -i t1080.jpg -f null -v error -

ls /tmp/vcap        # NNNNN-req<hex>-sz<n>.bin + log.txt
cat /tmp/vcap/log.txt | head
```

`log.txt` 每行给出请求号的方向/类型/序号/大小，`*_sz*.bin` 是对应请求的原始参数缓冲区
（≤64 KB 全量，更大的只取前 4 KB）。命令字格式与 `tools/j2d/j2d-decode.py`
一致（`0x80000000|reg` 起块、`0x0001NNNN` 写块等），按需过滤 `nr=0x4b` 之类的提交请求。

## 新增（2026-11-13 15:00）：跟指针 / 快照两种抓法

厂商 UMD 写寄存器不是走 ioctl，而是让 KMD 把 DEC 窗口映射进用户态后直接内存写；
数据缓冲则通过 8 字节的 `{ptr,size}` 结构传给内核。因此：

```sh
# 1) 跟 ioctl 里的用户指针（拿到 DEC 窗口/缓冲内容）
cc -O2 -shared -fPIC -o ~/dev-bin/vfollow.so vfollow.c -ldl
VF_DIR=/tmp/vf VF_MAX=16384 LD_PRELOAD=~/dev-bin/vfollow.so \
  ffmpeg -hwaccel vaapi -hwaccel_output_format vaapi \
         -vaapi_device /dev/dri/renderD128 -i ~/vp9-assets/t1080.jpg -f null /dev/null

# 2) 在指定 ioctl 之前快照进程的可读写映射（找"刚填好、还没交给内核"的缓冲）
cc -O2 -shared -fPIC -o ~/dev-bin/viodump.so viodump.c -ldl
VIODUMP_DIR=/tmp/vid VIODUMP_AT=25 LD_PRELOAD=~/dev-bin/viodump.so ffmpeg ...

# 3) 想看 ioctl 序列：VIODUMP_PRINT=1；想看 8 字节载荷：VIODUMP_HEX=1
```

`vendor-jpeg-window.{bin,txt}` 是厂商 1080p JPEG 解码的 DEC 窗口全文（16 KB / 201 个非零字），
`vendor-jpeg-regs.txt` 是更早的 502 条 kprobe 写入序列（从 `0x00c` 起）。
