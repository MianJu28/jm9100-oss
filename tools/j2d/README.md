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

## 已反解的命令格式（2026-11-12 三轮捕获）

寄存器写（`0x40000000 | reg` + 值，reg 是 2D 引擎内的偏移）：

| 寄存器 | 观测值 | 推测用途 |
|---|---|---|
| `0x1008` | 恒 0 | rop / 模式 |
| `0x100c` | 恒 0 | 混合模式 |
| `0x1010` | 源地址 | 拷贝源 |
| `0x1014` | 恒 0 | — |
| `0x1018` | 源描述字 | 拷贝源格式/步长 |
| `0x1020` | 填充色（ARGB8888） | 填充 |
| `0x1054` | 恒 `0xffffffff` | 写掩码 |

绘制操作（`opcode` 后紧跟 N 个 32 位操作数）：

| opcode | 操作数 | 语义 |
|---|---|---|
| `0x82000054` | 4 | 填充 `[0, dst_addr, dst_desc, (w<<16\|h)]` |
| `0x8200001c` | 7 | 拷贝 `[0x200f0, dst_addr, dst_desc, (w<<16\|h), src_addr, src_desc, (w<<16\|h)]` |

`0x81000000` 是命令缓冲结束；缓冲按 16 字节对齐，尾部补 0。

### 描述字（未完全解出）

同一尺寸（256×256）下观测到的描述字：

| 场景 | dst_desc | src_desc |
|---|---|---|
| pixmap(32bpp) → pixmap | `0x00010080` | `0x00040600` |
| pixmap → 窗口 | `0x00100000`（同源同值） | `0x00100000` |
| pixmap(24bpp) → pixmap | `0x00050680` | `0x00010080` |

变化的是"哪块 surface"，不变的是宽高（`(w<<16|h)` 另有一份），所以描述字里装的是
**步长 / 像素格式 / 平铺(TS) 属性**这类每 surface 固定的信息，而不是尺寸。
已知 `0x00040600` 会在同一进程的不同 surface 上重复出现，说明它至少不是全局常量。

下一步：在自有栈上用"2D 拷贝 → GL 读回比对"的自校验环，逐位试出描述字编码
（比继续在厂商栈上做黑盒差分更快，因为我们可以直接控制 surface 属性）。
