# 厂商 2D 引擎命令流捕获（按厂商实现方案复刻 2D 拷贝）

## 为什么做这个

present/合成拷贝在我们栈上走 **3D（glamor：着色器或 `glBlitFramebuffer`）**，
线性目标上限约 9–17 GB/s；厂商在同样 800×600 窗口上整帧只要 0.167 ms，
说明它的拷贝走的是 **2D 引擎**（EXA）而不是 3D。

厂商的 2D 加速链路是：

```
Xorg mwv207_drv.so (EXA)  --拼命令流-->  DRM_IOCTL_JM_J2D_SEND_CMD
                          --内核落环-->  2D 引擎 (0x968000 块)
```

- ioctl 请求号：`_IOWR('d', DRM_JM_J2D_SEND_CMD=0x4B, 16)` = **`0xc010644b`**
  （`struct drm_jm_j2d_send_cmd { u64 cmd; u32 size; u32 flags; }`，
  来自厂商 `jmgpu_garbage.h:166`）。
- 我们的 KMD 已经暴露同一台 2D 引擎（`DRM_IOCTL_MWV207_SUBMIT` 的
  `engine_type = 3`，`src/kernel-6.6/mwv207_pipe_2d.c`），**缺的只是命令编码**。

## 怎么用

必须在**厂商栈**上（`jmgpu` 内核模块在、X 用 `mwv207_drv.so`）：

```sh
sudo ./tools/j2d/capture-j2d.sh /tmp/j2d-cap
```

脚本会：停 lightdm → 用 `LD_PRELOAD=j2dcap.so` 起 `Xorg :0` →
用 `xcopy` 做 64/256/1024 三种尺寸的 `XCopyArea` → 杀掉 X →
用 `j2d-decode.py` 反解成"寄存器写"序列。

产物：

| 文件 | 内容 |
|---|---|
| `/tmp/j2d-cap/*.bin` | 每次 `J2D_SEND_CMD` 的原始命令流 |
| `/tmp/j2d-cap/log.txt` | 序号 / size / flags |
| `/tmp/j2d-cap/decode.txt` | 反解出的 `WREG 0x06010 = …` 序列 |

## 命令字（来自厂商 KMD 落环逻辑 `jmgpu_accessible.c:186`）

| 字 | 含义 |
|---|---|
| `0x80000000` | NOP |
| `0x40000000 \| reg` + 值 | 写寄存器 `reg`（`0x6000` 段是 2D 槽寄存器，对应我方 `pipe_2d_write(pipe, 0x6000 + slot*4, …)`） |
| 其它 `0x8xxxxxxx` + 3 字 | 链接/等待描述符 |
| 首字 | 头部 `byte16_cnt`（按 16 字节计的长度-1） |

## 下一步

拿到 64/256/1024 三组的 `decode.txt` 后，把"相同命令、只有地址/步长/尺寸不同"
的那几条挑出来，就是 2D 拷贝的寄存器配方；再在 UMD 的
`resource_copy_region`（`src/mwv207-umd/drivers/mwv207/mwv207_context.c`）里
用 `engine_type=3` 提交同一套命令即可。风险点是格式/裁剪/ROP 位，先做
同格式 1:1 拷贝（正是 present 需要的）。
