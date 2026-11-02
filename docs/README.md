# MWV207 / JM9100 驱动文档

景美 JM9100（MWV207）开源图形驱动的代码级文档。

- 硬件：景美 JM9100 / MWV207，PCI `0731:9100`
- 软件：Mesa 24.1.2 的 gallium 驱动 `mwv207` + `winsys/mwv207/drm` + 内核 KMD `mwv207.ko`
- 源码：`src/mwv207-umd/{drivers,winsys}/mwv207`（适配代码唯一位置；上游 mesa 参考树在 `ref/oh-mesa/`）

## 文档索引

| 文档 | 内容 |
|---|---|
| **[UNRESOLVED.md](UNRESOLVED.md)** | **未修复缺陷与未实现功能总清单（唯一索引，先看这个）** |
| [00-ARCHITECTURE.md](api/00-ARCHITECTURE.md) | 系统分层、文件清单与职责、数据流、构建与加载 |
| [01-WINSYS-KMD.md](api/01-WINSYS-KMD.md) | winsys/drm 层 API、BO/CS 生命周期、ioctl ABI（UMD 与 KMD 两套） |
| [02-CORE-OBJECTS.md](api/02-CORE-OBJECTS.md) | screen/context 虚表、完整 capability 表、资源与格式、表面与镜像描述符 |
| [03-DRAW-PIPELINE.md](api/03-DRAW-PIPELINE.md) | 命令流编码、状态原子、混合、采样器、顶点、绘制、BLT、查询、计算 |
| [04-NIR-COMPILER.md](api/04-NIR-COMPILER.md) | NIR 编译管线、pass 清单、指令编码与指令表、寄存器分配、反汇编 |
| [05-VIDEO-CODEC.md](api/05-VIDEO-CODEC.md) | 视频解码分层、能力表、解码管线、寄存器序列、参考帧管理 |
| [06-KNOWN-ISSUES.md](api/06-KNOWN-ISSUES.md) | 已知缺陷本树复核状态、winsys 新观察 O1–O12、本轮定位的 3 个问题、E/F/G/H/I 五组通读发现 |
| [07-WINSYS-AUDIT.md](api/07-WINSYS-AUDIT.md) | O2/O3 真实影响面追踪与最小修复建议 |
| [08-KMD.md](api/08-KMD.md) | **KMD 内核侧**：probe/remove/PM 次序、6 个 ioctl 实现、TTM 搬运路径、调度器与 fence、中断 demux、KMS/CRTC、K 系列复核与新增 N1–N14 |
| [09-SYMBOL-INDEX.md](api/09-SYMBOL-INDEX.md) | **符号索引**：668 个函数定义、194 个结构体/枚举、182 个函数宏（含签名与 file:line） |
| [10-HARDWARE-BLOCKS.md](api/10-HARDWARE-BLOCKS.md) | **硬件功能块总览**：1392 个寄存器按块统计、地址段分布、功能推断与驱动使用情况 |
| [registers/REGISTERS.md](registers/REGISTERS.md) | **寄存器参考**：驱动寄存器号 ↔ 厂商寄存器映射、覆盖率、未映射清单 |
| [registers/REGISTERS-FIELDS.md](registers/REGISTERS-FIELDS.md) | 每个驱动所用寄存器的字段位域、命名取值、驱动写入点 |
| [registers/REGISTER-MAP.md](registers/REGISTER-MAP.md) | **寄存器地址总表**：厂商全部 1392 个寄存器的地址/块/字段数/驱动使用标记 |
| [registers/REGISTER-ARRAYS.md](registers/REGISTER-ARRAYS.md) | **多实例寄存器文件**（PE 渲染目标 8 槽、2 个纹理单元、uniform 区）与 49 个未映射寄存器分析 |
| [registers/ISA-OPCODES.md](registers/ISA-OPCODES.md) | **ISA 操作码**：厂商 `SH_EXTENDED/CMPLX/IMG_ATOM/VISION_OPCODE` 全部取值与驱动交叉验证 |

## 关键发现速览

1. **寄存器号映射**：驱动命令流里的 16 位寄存器号 = 厂商寄存器 `_Address >> 2`。
   `0x530 → MWV207REG_PE_ALPHA_CONFIG`、`0x50CE → MWV207REG_BLT_CLUSTER_CONTROL`、
   `0x529 → MWV207REG_PE_CONFIG`。据此把 170/219 个驱动寄存器号映射到厂商寄存器定义。
2. **寄存器定义被剥离**：开源驱动的 `include/hw/mwv207reg_{3d,texture,shader}.h` 是空壳
   （只有 include guard），故驱动通篇使用裸十六进制常量。厂商 KMD 包头文件中保留了
   完整定义（1392 寄存器 / 3954 字段），已解析为 `registers/register-db.json`。
3. **格式表有三套码值空间**：`texture_infos[]`（TX 采样器，6 位）、`blitInfos[]`
   （BLT/镜像布局，6 位）、`pixel_infos[]`（渲染目标，另一套未定位的编号）。
4. **命令字编码**：`MWV207_EMIT` 写 `0x08010000 + reg`，bits[26:16] 为字数，
   bits[15:0] 为寄存器号。
5. **ISA 扩展操作码有厂商依据**：驱动反汇编表 `0x7f + n` 的 `n` 就是厂商
   `SH_EXTENDED_OPCODE`（U08）取值，逐项一致 → 见 `registers/ISA-OPCODES.md`。
6. **KMD 侧的高风险点是调度器而非内存管理**：`mwv207_sched.c:310-314` 的指针算术让
   `sched_dec[1]` 与 `sched_enc[0]` **同指一个槽位**（而 `INFO` 仍报 DEC=2）；
   三条管线的 `reset` **只 put 在飞 fence、不 signal**，用户态可能永久等待。
7. **视频解码的寄存器映射与厂商库完全不重叠**：解码块用的是按行号对齐的三张表
   （1840 行），厂商 1392 条寄存器库里**没有视频解码块**，故 `word → 厂商名` 无法反查。
   另注：正常解码路径**不发 out fence**，完成判定靠 KMD 轮询。

## 工具

| 工具 | 作用 |
|---|---|
| [docs/tools/extract_regs.py](docs/tools/extract_regs.py) | 扫描驱动源码，提取所有 `MWV207_EMIT*` 写入点（寄存器号、值表达式、文件:行号） |
| [docs/tools/map_regs.py](docs/tools/map_regs.py) | 验证"寄存器号 = 厂商地址 >> 2"的映射关系 |
| [docs/tools/build_register_db.py](docs/tools/build_register_db.py) | 解析厂商头文件构建寄存器数据库并与驱动用法关联 |
| [docs/tools/generate_register_docs.py](docs/tools/generate_register_docs.py) | 生成 `REGISTERS.md` / `REGISTERS-FIELDS.md` |
| [docs/tools/generate_register_map.py](docs/tools/generate_register_map.py) | 生成厂商视角的 `REGISTER-MAP.md`（全部 1392 个寄存器） |
| [docs/tools/generate_register_arrays.py](docs/tools/generate_register_arrays.py) | 生成 `REGISTER-ARRAYS.md`（多实例寄存器文件 + 未映射寄存器分析） |
| [docs/tools/generate_symbol_index.py](docs/tools/generate_symbol_index.py) | 生成 `api/09-SYMBOL-INDEX.md`（函数/结构体/宏索引） |
| [docs/tools/check_coverage.py](docs/tools/check_coverage.py) | 文档覆盖率自检：驱动源文件是否被文档引用 |
| [docs/tools/generate_block_overview.py](docs/tools/generate_block_overview.py) | 生成 `api/10-HARDWARE-BLOCKS.md`（按块的寄存器统计与功能推断） |
| [docs/tools/validate_register_map.py](docs/tools/validate_register_map.py) | **映射自校验**：驱动号×4=厂商地址、vendor 名存在、每条 file:line 真实存在 |
| [docs/tools/generate_isa_opcodes.py](docs/tools/generate_isa_opcodes.py) | 生成 `registers/ISA-OPCODES.md`（厂商 ISA 操作码 + 驱动交叉验证） |
| [docs/tools/validate_doc_citations.py](docs/tools/validate_doc_citations.py) | 校验所有文档中的 `文件:行号` 引用是否可解析且不越界 |

## 校验结果

`python3 docs/tools/validate_register_map.py` 通过：

```
驱动寄存器号（进入 driver_join）：170
厂商寄存器（register-db）：      1392
uses 引用点：                    292（异常 0）
映射等式错误：                   0
其他错误：                       0
提示（不阻断）：                 0
结论： 全部映射自洽
```

即：170 条 `驱动号 × 4 == 厂商 _Address` 等式全部成立，292 处
`文件:行号` 引用全部在源码中有界有效。

`python3 docs/tools/validate_doc_citations.py` 通过：

```
引用总数：4442   可解析：2880   越界：0   找不到文件：1562
结论： 无行号越界
```

其中 1562 条"找不到文件"全部是 `jmgpu_*.h` 厂商头引用 —— 厂商头需从
`ref/vendor-1.8.0/` 的 deb 解到 `/tmp`（脚本按 `/tmp/dpkg/gg`、`/tmp/v180` 等
候选路径查找，`/tmp` 清理后会降级为提示，不影响结论）。补齐厂商头后应为
"可解析 = 总数、找不到 = 0"。

两项校验只覆盖**可机械判定**的部分（映射等式、行号存在性与边界）；
寄存器/字段**语义**的正确性仍依赖 `REGISTERS-FIELDS.md` 中的人工对照，
以及 `api/06-KNOWN-ISSUES.md` 里逐条标注的问题列表。

`register-db.json` 与 `command-registers.json` 是上述脚本的中间产物，可供二次查询。

## 数据来源与边界

- 驱动源码：开源 UMD（MIT 许可）——本文档的主要依据。
- 厂商寄存器定义：来自官方闭源包内的 KMD 头文件（`mwv207-1.6.1-v25.u1` / `1.8.0.uos`，
  两者寄存器定义一致：同为 1392 个）。这些头文件标注为厂商机密，**文档中仅记录
  互操作性所必需的事实**（寄存器名、地址、字段位域、命名取值），不复制文件本身。
- 结论标注规则：无 `文件:行号` 依据的推断一律标为"未确认"。