# 交接文档 — MWV207 2D 引擎数据通路（2026-11-17 封档）

> 目的：把"2D 硬引擎能不能拿来替代 present 的整帧拷贝"这件事的全部已知事实、
> 已修代码、未解缺口与工具一并交接，避免下一个会话从零重复逆向。
> 配套：`docs/UNRESOLVED.md`（§"2D 引擎…"四节，含逐次实验记录）、
> `tools/j2d/README.md`（更长的过程日志）、`tools/vendor-capture/regs/`（厂商现场原始 dump）。

## 0. 一句话现状

**命令编码、环格式、提交通路三项都已解出并验证；唯独"draw 数据面寻址"是闭源知识，
按任何已知编码回放都不产生像素写。** 因此 present 仍走 3D/PE 的 `mwv207_copy_image`，
2D 不可用。若继续，收益上限也只是 present 每帧约 **0.29–0.40 ms**（见 §9.1）。

## 1. 硬件事实（实测，不是推断）

| 项 | 值 / 结论 |
|---|---|
| 2D 窗口基址 | `J9MATHS_BANDCUTTER = 0x968000`（`~/vendor-src/usr/src/mwv207-1.8.0.uos/jmgpu_define.h:85`，与我们 `mwv207_pipe_2d_create(jdev, 0, 0x968000, "mwv207_2d0")` 一致） |
| 特殊全局 | `0x96C000` 段（`0x4200/0x4208/0x420c/0x4210/0x4214/0x4308/0x4800/0x6000+i*4/0x4100`）—— **可读可写，是真寄存器** |
| 命令寄存器 | `0x968000+0x0 … +0x3FFF` —— **只写不可读**：读任何偏移都返回"最后一次写入的值"（全窗口读回 `0x8888`），扫描 32 KB 里值 `2` 出现 4163 次 ⇒ 不能靠回读判断内部状态，只能看副作用（同步写 / 像素 / `0x96C100`） |
| 时钟 | `GU2D_PLL`（pll 11）`fb=0x01200006 post=0x01010101`，**已配好**（`MWV207_PLL_STATE_BASE(id)=0x9b0000+0xA00+0x100*(id/6)+0x20*(id%6)`）⇒ 不是"没开时钟" |
| 核实例 | 厂商 KMD `dec_subsys_array` / TOP 表：本机 **只有一个 2D 核**；`0x96xxxx` 在 BAR1 里可见 |
| 工作模式 | 本机 `j2dmode = 1` = **waitlink**（不是 ringbuffer）；厂商在该模式下**从不使能** 2D 私有 MMU |
| 厂商 2D 在现场确实工作 | `~/j2d-captures/sem1/` 里抓到的 `XCopyArea` 命令流、`0x96C100 = 0x00001400`（2D 常忙）可证 |

## 2. waitlink 协议（我们已完全对齐，引擎会执行）

厂商实现：`jmgpu_accessible.c` 的 `j9_nipas()`（启动）/`j9_gelatinizable()`（每提交）/`j9_prominences()`（取空闲槽）。

- 环：**256 个槽 × 0x1000 字节**，每槽独立，槽首 16 字节是链头。
- 链头 4 字：`{ 0x80000000, byte16_cnt, 0x80000000, 0x80000000 }`。
  - **实测硬规则**：只写 2 字头时引擎会把第一对"寄存器/值"当头部吃掉（少执行一段）；
  - `byte16_cnt = 字数/4 - 1`，整链必须 **16 字节对齐**；56 字节链 + `byte16_cnt=2` 静默不执行。
- 同步写：`0x40000000 | reg`，紧跟 `val`，其中 `val = ((v ^ 0x5555) | 1) & 0xffff`
  （厂商 `J9_METERSTICK`）；`reg = 0x6004 + (seqno & 15) * 4`。
- 链尾：`wait` 描述符 `{0x88000000, 0x80000000, 0x80000000, 0x80000000}`；
  提交下一段时把上一槽的 wait 就地改成 link `{0x89000000, <下一槽地址>}`。
- 每次提交都要**再踢一次** `0x4C00 = 1`（只改 link 不踢，引擎不会醒）。
- **link 目标必须在环内**（我们环是 `0x40000–0x7ffff`）：指向环外 `0x300000` 时引擎静默不动。
- **link 目标必须 4 KB 对齐**（我们环旧的 `0x40030/0x40500/0x40600` 都不执行，
  `0x41000` 才执行）—— 这是 2026-11-16 找到的根因，已在 KMD 里按槽对齐。
- 冷复位（只在 probe 首次做，之后由引擎自己跑）：`0x96C200 = 0x80000000`(FROM_AHB) →
  等 NOC idle（`0x9B0194/0x9B0198`）→ `0x9B0048` 清/置 bit8 → `0x9B0014` 清/置 bit16 →
  `msleep(10)` → `0x96C800 = 0` 轮询到 1（CUREMASTER）。
  **注意**：在在线桌面上反复做这套会冻结显示（已发生两次，靠重启恢复）。
- 启动链（我们 `pipe_2d_start()`，48 字节）：`{nop, 2, nop, nop, sync(0x6004=0x5555), nop, nop, wait(4 字)}`，
  随后 `0x4200 = (ring_addr>>4) | (1<<29)`、`0x4C00 = 1`。

## 3. 寄存器表（2D 窗口内）

| 偏移 | 名称 | 位域 / 说明 |
|---|---|---|
| `0x4200` | SPECIAL_GLOBALS | `CMDADDR 27:0`、`MODE 29`（0=ringbuffer, 1=waitlink）、`FROM 31`（0=MEM, 1=AHB） |
| `0x4204` | OFFSET | 27:0 |
| `0x4208` | WPTR | 写指针（槽号） |
| `0x420c` | RPTR | 读指针（**读回常等于 WPTR，别当进度用**） |
| `0x4210` | CTRL | `FETCH bit0`（=1 启动 ringbuffer 取指） |
| `0x4214` | SIZE_MASK | `size - 1`（ringbuffer 模式） |
| `0x4308` | GREENSTUFF | 中断清除（我们 ISR 写 `0xffffffff`） |
| `0x4800` | CUREMASTER | 复位握手：写 0，读回变 1 |
| `0x6000` | MUTILATING | 同步/空闲判定字 |
| `0x6004 + i*4` | 同步槽 i | 引擎把 sync_val 写进来，KMD 比对后给 fence 发信号 |
| `0x4100` | AD_STATUS | `FIFO 9:0`、`2DBUSY 11:10`、`PARSERBUSY 12`、`ENDIAN 31`（LE=0） |
| `0x9E83B4` | 2D MMU PD | `PD(31:4) = pgtable>>12`、`MODE 1:0`、`SETUP 5` |
| `0x9E8388` | 2D MMU CTRL | `ENABLE 0`、`OUTOFBOUND_EXC 2`、`SETUP 5` |
| TOP `0x9B0014` | TOP_GU2D | `RESET bit16` |
| TOP `0x9B0048` | TOP_NOC | `GU2D bit8` |

2D 私有 MMU **只在 ringbuffer 模式使用**（厂商 `j9_handle_prebendate()` → `j9_handle_trinketing()`）；
waitlink 模式厂商两个寄存器恒为 0（现场 dump 证实）。我们实现为"只建表不使能"，勿再打开，
否则取指地址被二次翻译 → 读到垃圾。

## 4. 命令编码（DDX 反汇编 + 现场抓包，双向确认）

命令字是 **`寄存器 | 值` 交替**，`0x4000xxxx` 为寄存器写，`0x8xxxxxxx` 为控制字。

### 4.1 快速填充（`jmgpu2dFillRectQuick 0x17be0`，26 字）

```
0x40001008, 0                       # 恒 0
0x4000100c, depth_table[depth-8]<<3
0x40001014, depth_table[depth-8]
0x40001020, color
0x40001054, dst_addr                # 提交时由 reloc 填低 32 位
0x82000054, 0, (pitch/16)<<16 | off, x | y<<16, w<<16 | h
0x81000000                          # END
```

`depth_table`（DDX `.rodata 0x1f6f8`）：8bpp→`2`、24bpp→`1`，其余 `0xffffffff`。

### 4.2 ROP 填充（`jmgpu2dFillRectRop 0x17d60`）

同上，opcode 换 `0x8200001c`，紧跟一个字是 rop（`.rodata 0x1f760` 按 `[ctx+44]` 索引）。

### 4.3 拷贝（`jmgpu2dCopyArea 0x17900`，26 字）

```
0x40001008, 0
0x4000100c, fmt << 3
0x40001014, fmt
0x40001010, dst_addr
0x40001018, src_addr
0x40001054, 0xffffffff
0x8200001c, fmt|0x20000, <dst 组>, <src 组> …    # 现场看到 7 个操作数
0x81000000
```

现场抓包（`tools/vendor-capture/regs/vendor-2d-ring-1024.txt`，厂商环 `0x213000`）的真实形状：

```
40001010 01e001ae  40001018 02b90100  40001054 ffffffff
8200001c 000200f0  01e0022c 02bd0700  00400700
         01e001ae  02b90100  00400700
81000000
```

⇒ 7 操作数 = `fmt|0x20000` + **两组 (dst, src, w<<16|h)**，第 2/3 个与第 5/6 个相差
`0xFD`/`0x1080` 量级（同一面内不同偏移/基址）。**我们此前只给 4 个操作数、地址放错字段，
是从一开始就错的。**

### 4.4 控制字语义（手工实验）

- `0x81000000`（END）：在**我们的链布局**下会截断后续命令（fence 同步写不执行 → 提交必超时），
  所以 KMD 现在把用户流末尾 END 换成 NOP；
  但厂商链里 `END` 之后仍跟着 sync/wait —— 两者差异疑点在 `byte16_cnt` 是否覆盖这些字，
  **该矛盾未定论**，接手时先按"END 即截断"用。
- `0x88000000` = wait，`0x89000000` = link，`0x80000000` = NOP。

## 5. KMD 实现现状（`src/kernel-6.6/mwv207_pipe_2d.c`，1055 行）

我们复刻了厂商 waitlink 引擎，已修的真实缺陷（每条都有现场证据）：

| 缺陷 | 修法 |
|---|---|
| `struct mwv207_job.cmds` 是 `char *`，按 `u32` 索引只读到字节 → END 替换循环被编译器整体删掉（`.ko` 里无比较指令） | `const u32 *cmds = (const u32 *)mjob->cmds;` + `WRITE_ONCE(payload[i-1], 0x80000000u)`；用 `-save-temps` + `objdump` 复核 |
| 用户流尾部 `0x81000000` 截断后续 fence 同步写 | 提交时把尾部 END 换成 NOP |
| 启动链只有 40 字节，`byte16_cnt` 与实际不符 | 补齐为 48 字节（与厂商 `j9_nipas()` 一致） |
| 所有 fence 共用 `0x6000` 同步槽 | 每 fence 独占 `0x6004 + (seqno&15)*4` |
| 只改 link 不踢引擎 | 每次提交 `pipe_2d_kick()`：`writel(1, 0x4C00)` + `readl(0x4100)` 屏障 |
| 环放系统内存 + 开 2D MMU（照抄 ringbuffer 模式） | 环回 VRAM（`mwv207_bo_create(..., ttm_bo_type_kernel)` + pin + kmap），MMU 只建表不使能 |
| link 目标只 16 字节对齐 | `PIPE_2D_SLOT_SIZE 0x1000` + `pipe_2d_slot_align()`，`pipe_2d_wait_for_space()` 返回 4 KB 对齐槽基址 |
| 环放在系统内存导致取指失败 | 见上（VRAM） |
| 冷启动后引擎不响应 | `probe` 时补 `0x96C200 = 0x80000000`（FROM_AHB），`pipe->cold_reset_done` 门控 |

参数：环 `0x40000`（`0x40000–0x7ffff`，128 KB）、槽 4 KB、`diag` 门控日志（`echo 1 > /sys/module/mwv207/parameters/diag`
打印 `wtol`/`kick` 与寄存器回读）。

提交链实测形状（`0x41000` 槽，验证时 dump）：

```
0x41000: 80000000 00000006 80000000 80000000   # 4 字头, byte16_cnt=6 (112B)
0x41010: 40001008 0  4000100c 0  40001014 0  40001020 00ff00ff
0x41030: 40001054 ffffffff  82000054 0  03aad000  0  00400040
0x41048: 80000000                              # 原 END 已换 NOP
0x41050: 4000601c 00005553 80000000 80000000   # fence 同步写
0x41060: 88000000 …                            # wait 尾
```

## 6. 已确认 vs 未确认（避免重复踩坑）

已确认：
1. 引擎能执行 waitlink 链（同步值落 `0x96E004…`、link 能跳段）；
2. fill/copy **opcode 被接受、执行到链尾**（同步值照样写入）；
3. `0x01010/0x01018` 装面地址（厂商现场值都在 0–80 MB 的 VRAM 偏移范围内）；
4. `0x01008/0x0100c/0x01014` 在厂商命令里恒 0、`0x01054` 恒 `0xffffffff`；
5. 唤醒条件：4 KB 对齐槽 + 4 字头 + 16B 对齐 + 环内 link + 每次踢 `0x4C00`；
6. 2D 私有 MMU 在 waitlink 模式不该使能；
7. 2D 窗口只写不可读。

被推翻的假设（写进报告防止再试）：
- "厂商用 TILE 平铺扫描出" → 假，两边显示窗口寄存器逐字段相同，都线性（`0x990434 = 0x1e0` pitch 7680）；
- "引擎永不执行" → 只是我们的链/唤醒不对；
- "draw op 根本不写显存" → **错**，用 VRAM 全量扫描（`/tmp/vscan.py`）能看到填充色块，只落点不在指定地址；
- "打开 2D MMU 就能寻址" → 开了也没用，反而可能更糟；
- "厂商在 ringbuffer 模式跑" → 本机是 waitlink（`j2dmode=1`）。

## 7. 未解缺口与候选（这是唯一剩下的问题）

现象：链执行完、同步值写入、目标地址零字节变化；换 5 种字段布局、含厂商原样逐字重放，
`matched=0`。目标地址若指向环附近区域，能在**别的偏移**发现色块（落点与第 4 操作数相关，
像"地址 >> 4"），但按目标校验始终 0。

候选（按可能性排序）：
1. **面地址是"2D 侧句柄/登记号"而非 VRAM 偏移** —— 厂商 UMD 提交前可能把面登记进引擎侧表；
2. **缺少每提交状态**：DDX 在用 mmap 直接写某个非 ioctl 通道的配置（我们只抓了 ioctl 与环）；
3. 描述字（`0x00040600`/`0x00010080`/`0x000200f0`）的位域含义没解全，可能携带必需的模式位；
4. `byte16_cnt`/END 语义差异（见 §4.4）导致引擎只执行了头部。

## 8. 工具与用法（都在仓库里，可直接接手）

| 工具 | 用途 |
|---|---|
| `tests/j2d-copy/j2dtest.c` | 自研栈自检。`j2dtest w h mode`：`0`=填充、`1`=拷贝、`3`=描述字扫描、`4`/`5`=扫描变体、`6`=厂商 Quick 填充、`7`=厂商 ROP 填充、`8`=厂商拷贝、`11`=空链 ping。**超时已改成返回错误**（原来吞掉超时，导致"提交成功"全是假的） |
| `tools/j2d/vsubmit.c` | 厂商栈直发：`ioctl(fd, 0xc010644b, {cmd,size,1})`，**fd 必须 `/dev/dri/card0`**（`/dev/jmgpu` 返回 `Inappropriate ioctl`） |
| `tools/j2d/copytest.py` | 写源/目标面 → 按命令形状发链 → 回读校验（输出 `matched=N`） |
| `tools/j2d/livecap.py` | 厂商栈边跑 `XCopyArea` 边 dump 2D 环（1 MB / 256 槽） |
| `tools/j2d/vendor2d-probe.sh` | 厂商栈取证：Xorg maps、2D 全局/TOP/MMU 寄存器、VRAM 前 64 MB。**用 `bash tools/j2d/vendor2d-probe.sh` 跑**（`sudo ./…` 会报找不到命令） |
| `tools/j2d/xcopy.c` | 触发一次 `XCopyArea`（`cc -O2 -o ~/dev-bin/xcopy tools/j2d/xcopy.c -lX11`，用法 `xcopy :0 256 3`） |
| `tools/j2d/j2dcap.c` / `j2d-decode.py` | 抓/解 ioctl `0xc010644b` 的命令流 |
| `tools/j2d/capture-vendor-engregs.sh` | 厂商栈 kprobe `engine_write_reg` 抓 `0x968xxx` 写序列（**本机 ftrace 不可用，探针零事件**，见 §9） |
| `tools/vendor-capture/regs/*` | 厂商现场原始 dump（环、TOP、README 有判读） |
| `~/dev-bin/{barmap,vdump,vw2.py,vscan.py,j2dring.py}` | BAR1 读写、VRAM 读、VRAM 写（逐字写 + 页对齐 flush）、VRAM 图案扫描、环 dump |

编译 `j2dtest` 需要 `-I/usr/include/libdrm`（否则 `drm.h` 找不到）。

## 9. 环境坑（每一条都真实踩过）

1. **不要在厂商栈的在线桌面上用 `vsubmit` 反复发 2D 命令** —— 会把引擎挂住，`Xorg` 进 `R` 态自旋、桌面冻结（发生过一次，靠 `systemctl restart lightdm` 恢复）。要试请在停 X 的文本模式、并准备重启。
2. 手工全复位（FROM_AHB + NOC/GU2D 清置）会冻结显示（发生过两次）。
3. 每次失败后 **2 s 调度器 TDR** 会复位引擎；连续失败会把桌面拖死。
4. `sudo -S` 与 heredoc 混用会吞掉密码 ⇒ 脚本写 `/tmp/x.py` 再 `printf '666\n' | sudo -S -p '' python3 /tmp/x.py`。
5. **`pkill -f <pattern>` 会匹配到自己那条 bash 命令**（命令行含同样字符串）⇒ 用 `ps -eo pid,args | awk -v pat=… 'index($0,pat) && !/awk/ {print $1}' | xargs -r kill`。
6. `dd` 从 `resource2` 读返回 0 字节，必须用 python `mmap`；`pwrite` 到 resource2 返回 `EIO`。
7. `mmap` 的 `flush(off, 4)` 报 `EINVAL`，必须页对齐 `flush(off & ~0xfff, 0x1000)`；整文件 mmap 连续写多字会随机 `SIGBUS` ⇒ 一次少量写、写完 `vdump` 复核。
8. **`./switch-mwv207.sh kmd stage` 不刷新 initramfs**：冷启动优先用 `/boot/initrd.img-*` 里的同名 `.ko`，必须 `sudo depmod -a <kver>` + `sudo update-initramfs -u -k <kver>`（`update-initramfs` 偶发 plymouth hook 段错误 `139`，重跑一次即可）。
9. `cat ~/dev-bin/vwrite`（ELF）曾把终端灌满 —— `cat` 前先 `file`。
10. 2D 窗口读回不可信（§1），判据只能用副作用。

### 9.1 为什么现在不做（收益核算）

- 我方 present 800×600 **0.399 ms/帧**（2503 FPS），其中两次整帧拷贝
  （UMD 平铺→线性影子 + Xorg glamor 客户端 pixmap→屏幕 pixmap）；
  `MWV207_SCANOUT_TILE=0`（全线性无影子）反而 **0.483 ms** ⇒ 去重不划算（提交 `6b685b7`）。
- 厂商 present **0.014 ms** 是"零拷贝异步翻转"架构，不是 2D 加速的功劳（其显示窗口寄存器与我们相同）。
- 所以 2D 能拿到的上限 ≈ 0.29 ms/帧，且要动 KMD 提交路径与 X 侧拷贝链路。

## 10. 若继续（建议的实验设计，含止损）

1. **先确认面地址语义**：在厂商栈**停掉 X**（文本模式）后，用 `vsubmit` 发一条拷贝，
   目标面用已知标记，扫全 VRAM 找像素落点；把落点与命令里的每个字段做拟合
   （特别是第 4 操作数 `>>4` 的猜想）。
2. **抓 DDX 的非 ioctl 写**：LD_PRELOAD 拦 `mmap`，对 DX 映射区做前后快照
   （`tools/vendor-capture/viodump.c` 已有"ioctl 前快照 rw 映射"的骨架，可改成按时间间隔快照）。
3. **读 EXA pixmap 私有结构**：`jmgpu2dCopyAreaHW 0x14ad0` 取 `exaGetPixmapDriverPrivate`
   的 `+32`(pitch)/`+72..76`(坐标) —— 把它的值与命令里的两组地址对齐，判断"面基址"是否
   就是 `pixmap->devPrivate` 换算出的 VRAM 地址+偏移。
4. **止损条件**：若第 1 步在文本模式下仍找不到任何像素落点，说明缺的是闭源登记步骤，
   **停止投入**，把结论回填本文档与 `docs/UNRESOLVED.md`。

## 11. 相关提交与文档

- KMD 修复：`679c60d`（2D 初次提交路径）、`8efa84e`（diag 诊断）、`e0f7eb9`（**4 KB 槽对齐 + diag**）、
  `6ce8546`/`705d145`（DDX 编码表 + 探针修正 + 收口）、`8c47679`（HANDOVER 收口行）。
- 文档：`docs/UNRESOLVED.md` 的 4 节 2D（按时间顺序，含每次实验的原始读数）；
  `tools/j2d/README.md`（过程日志，含被推翻假设的记录）；
  `tools/vendor-capture/regs/README.md`（厂商现场判读）。
- 相关背景：present 链路见 `reports/PRESENT-PATH-20261111.md`；
  厂商/我方差距见 `reports/VENDOR-GAP-20261113.md` 与 `reports/VENDOR-STACK-PROBE-20261116.md`。