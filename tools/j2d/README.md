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

## 自校验工具（`tests/j2d-copy/j2dtest.c`）

用 `DRM_IOCTL_MWV207_SUBMIT(engine_type=3)` 直接给 2D 引擎喂上面反解出的命令，
再用 CPU 读回比对，无需屏幕或第三方参照：

```
gcc -O2 -o /tmp/j2dtest tests/j2d-copy/j2dtest.c $(pkg-config --cflags --libs libdrm)
/tmp/j2dtest 64 64 0 <desc_cand>     # 0 = 填充, 1 = 拷贝
```

当前结论（2026-11-12，开源栈 `8DFC520FB8FCDECFBC911AA`）：

- 命令能被引擎接受：提交成功、out fence 置位（说明环形缓冲被消费）。
- 但 dst 内容仍是全 0，填充/拷贝都没有落盘 —— 尝试过的描述字候选
  （`0`、`pitch`、`pitch<<16`、`0x40600`、`0x100000`、`0x10080` 等）都无效。
- 因此缺口不在"命令流格式"（那部分已由厂商捕获反解出来），而在
  ① 描述字编码，或 ② 我们 KMD 的 2D 环形头布局（`pipe_2d_submit()` 在用户
  命令前插了 `0x80000000, count, 0x80000000, 0x80000000` 四个字，而厂商
  抓包里 `count` 是缓冲区**第一个**字），或 ③ 2D 引擎的寄存器基址/opcode
  与本机这一代硬件不一致。

下一步优先级：先确认 ①/②/③ 中的哪一个，再决定是改 KMD 的环形头还是继续解描述字。

## 2026-11-12 后续：段头布局已证伪，问题在 opcode 层

- 从厂商 KMD 源码 `jmgpu_accessible.c:46-52` 拿到真正的段头结构：

  ```c
  typedef struct tag_jms_wl_header {
          u32 dont_care0;
          u32 byte16_cnt;      /* ← 计数在**第二个**字 */
          u32 dont_care1;
          u32 dont_care2;
  } j9_reopposing;
  ```

  与我们的 `pipe_2d_submit()` 完全一致（`[0x80000000][count][0x80000000][0x80000000]`）。
  ⇒ "厂商把计数写在第一个字"的猜测是错的；把计数挪到第一个字还会让段起点
  4 字节对齐破坏，KMD 直接报 `mwv207_2d0: submit failed, errcode=-5`（我们的
  `WARN_ON_ONCE((span & 0xf) || span > size)` 分支）。该实验已回退（`807fe4b`）。
- 真正的缺口在 **draw opcode**：`0x82000054` / `0x8200001c` 由厂商 userspace 的
  `libdrm_jmgpu.so` 生成（该库导出 `jmo_2D_Clear`/`jmo_2D_Blit`/`jmo_2D_Construct`/
  `jmo_2D_Commit`/`jmo_2D_BatchBlit` 等一整套 2D API，符号未 strip），并不在 DDX 里
  （DDX 链接它，且二进制里查不到这两个立即数）。这些 opcode 的确切编码只存在于
  闭源库内部，要复刻需要反汇编该库或抓更多样本来差分。

## 2026-11-12 第二轮：静态反编译得到的两条硬结论

### 1. load-state 的字格式（已确认）

`jmo_HARDWARE_Load2DState`（`libdrm_jmgpu.so` 偏移 `0x8ece0`）就是"一次写
若干个寄存器"的打包器：

```asm
ubfx  x0, x0, #2, #16      ; x0 = (reg >> 2) & 0xffff
ubfiz w4, w19, #16, #10    ; w4 = (width & 0x3ff) << 16
orr   w4, w4, w0
orr   w4, w4, #0x8000000   ; 加上 0x08000000 标记
str   w4, [x3, w2, uxtw #2]; 写命令字
...  memcpy(下一个字, vals, width*4)
```

即：**`word = 0x08000000 | (width << 16) | ((reg >> 2) & 0xffff)`，
随后跟 `width` 个字的数据**。这和抓包里看到的 `0x40000000|reg` + 值
是两种不同粒度的写法（后者是单寄存器）。

### 2. draw 类的字是另一套编码

抓包里的两条绘制命令按上面的位域拆开：

| 命令字 | 拆解 |
|---|---|
| `0x82000054` | `0x80000000 | 0x02000000 | 0x54` |
| `0x8200001c` | `0x80000000 | 0x02000000 | 0x1c` |

`0x54`/`0x1c` **不是寄存器号**（寄存器号在这里要 `>>2` 才对得上），
而是 draw 类自己的子操作码；两条命令后面分别跟 4 个和 7 个操作数。

### 3. 静态搜不到 emit 点

- 库里 `0x82000054` / `0x8200001c` **不以立即数形式出现**（`0x82000000`
  只在 `.rodata` 的测试向量里出现，和 2D 无关），说明 emit 点是
  `0x80000000 | 0x02000000 | cmd` 这样算出来的；
- 逐个排查 `orr w, w, #0x2000000` 的 6 个命中点，分别落在
  `jmo_DECHARDWARE_SetTilingConfig`、`jmo_HARDWARE_UpdateTextureDesc`、
  `jmo_HARDWARE_BindTexture` 等 3D/DEC 代码里，没有一个属于 2D；
- 2D 的调用链是
  `jmo_2D_Clear/Blit → _DoBlitOrClear(0x37610) → jmo_HARDWARE_Clear2D(0xb6da4)
  → jmo_HARDWARE_StartDE(0xb5744)`，它们往上下文里那个"每块 0x86c8 字节"
  的命令结构里填字段，再由更内层统一序列化——真正的落字函数还没定位到。

### 4. 描述字扫描：16 种全灭

把 `desc` 按"`(格式/标志 << 16) | (pitch/8)`"这个从抓包反推出来的形状
穷举（fmt 0..7 × 额外标志 {0,0x600}，`tests/j2d-copy/j2dtest.c` 模式 3），
目标缓冲全部保持全 0 ⇒ 问题不在描述字取值，而是整条 draw 命令没有被
2D 引擎执行。

### 5. 于是改用动态观测（已备好工具）

`tools/j2d/j2dsem.c`：LD_PRELOAD 拦截 `jmo_HARDWARE_Load2DState` /
`StartDE` / `Clear2D` / `2DChipEnable`，打印每次调用的
`(ctx, reg, width, vals[])`，并把命令缓冲新增的字（游标在 `ctx+0x3770`，
基址指针在 `ctx+0x3768`）逐条 dump 出来。`capture-j2d.sh` 默认
`J2D_SEM=1`，会在起临时 X 时同时预加载两个 so，日志落在
`$OUT/sem.log`。

这一步不再需要读反汇编：只要在厂商栈上跑一次已知拷贝，就能拿到
"高层语义 → 命令字"的完整对照，直接就能回填 `j2dtest`。

## 2026-11-12 第三轮：抓到"是谁拼的命令流"，并解出描述字公式

### 1. 命令流是厂商 DDX 自己拼的（不是 libdrm_jmgpu）

`j2dsem` 的调用栈（`~/j2d-captures/sem2/sem.log`）给出决定性事实：

```
drm_jmgpu_j2d_send_cmd
 ← mwv207_drv.so(+0x150bb)          ← jmgpuDoneSolid（提交累积的 solid 填充）
 ← libexa.so(+0x8e1f) / (+0xa84f)   ← EXA 的 CopyArea/PixmapClear
 ← Xorg(miPaintWindow / MapWindow …)
```

也就是说：**2D 命令字由闭源 DDX（`mwv207_drv.so`）构建，`libdrm_jmgpu.so`
只负责把缓冲发下去**。这解释了为什么在 libdrm_jmgpu 里搜不到
`0x82000054`/`0x8200001c` 的立即数。

### 2. DDX 里的构造函数与公式

符号未 strip，直接可读：

- `jmgpu2dCopyArea`（`0x17900`）：**拷贝**命令的拼装点。关键指令：

  ```asm
  179c0: mov  w10, #0x1c
  179d0: movk w10, #0x8200, lsl #16   ; w10 = 0x8200001c  ← 拷贝 opcode
  179d8: mov  w4,  #0x81000000        ; 结束标记
  179d4: orr  w3,  w3, #0x20000       ; w3 = 格式表值 | 0x20000
  17a5c: orr  w2,  w16, w2, lsl #16   ; desc = (pitch/16 << 16) | 格式码
  ```

- `jmgpu2dFillRectHW`（`0x14e60`）/ `jmgpuDoneSolid`（`0x15050`）：
  填充路径；`jmgpuDoneSolid` 里就是 `bl jmgpu2dFillRectHW` 之后
  `bl drm_jmgpu_j2d_send_cmd`。

- 两张 `.rodata` 表（已 dump）：

  | 表 | 地址 | 内容 |
  |---|---|---|
  | 格式码 | `0x1f650`（16 项） | `0, 0xa0, 0x50, 0xf0, 0xa, 0xaa, 0x5a, 0xfa, 0x5, 0xa5, 0x55, 0xf5, 0xf, 0xaf, 0x5f, 0xff` |
  | 位深码 | `0x1f690`（按 depth-8 索引） | `8→2`、`16→1`、其余 `0xffffffff`(无效) |

  第一条操作数 = 格式码 `| 0x20000` ⇒ 抓包里的 `0x000200f0` 对应格式码 `0xf0`
  （32bpp 那档）。

### 3. 下一步（等切回开源栈）

`tests/j2d-copy/j2dtest.c` 新增模式 4：按
`desc = (pitch/16 << 16) | 低 16 位格式码` 扫描
`{0x600,0x680,0x80,0xc0,0x60,0x0}` 六档，配合 `0x20000 | 0xf0` 的拷贝
操作数；跑了就能确定 32bpp 线性 surface 的正确描述字。

