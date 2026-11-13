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
