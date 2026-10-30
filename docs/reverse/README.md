# 闭源驱动反编译文档集

> 目标（用户指令 m04316/m04318）：**"反编译闭源驱动，整理 api 文档，和寄存器信息"**。
> 对象：景美 JM9100/MWV207 官方 1.8.0 闭源包（解包于 `/tmp/v180`）。
> 工具：本机无 r2/ghidra/radare2，全部结论由 `nm` / `readelf` / `objdump` / `strings`
> 加自写脚本得出，可重跑（脚本在 `reverse/tools/`）。

## 文档

| 文档 | 内容 |
|---|---|
| [01-LAYERS.md](01-LAYERS.md) | 分层结构、符号可用性、13 个二进制清单、调用链实证 |
| [02-HAL-API.md](02-HAL-API.md) | `libdrm_jmgpu.so` 全部 1496 个 `jmo_*` 符号，按 28 个家族分组 |
| [03-REGISTERS.md](03-REGISTERS.md) | 编码约定的**双向实证**、提取方法、379 个寄存器、块分布、Top |
| [04-UNMAPPED-REGS.md](04-UNMAPPED-REGS.md) | 开源驱动 49 个"厂商库查不到"的命令号：32 个有 HAL 实证 |
| [05-OCCLUSION-QUERY.md](05-OCCLUSION-QUERY.md) | 遮挡查询：从"不可解"到寄存器级定位 + 候选修复 |

## 三条最重要的结论

1. **`libdrm_jmgpu.so` 就是 HAL**。`jmgpu_dri.so` 里 322 个 `jmo_/jms_` 全是 UNDEF，
   全集合只有 `libdrm_jmgpu.so` 定义了它们（1524 个）。所有寄存器写入都在这一层。

2. **命令号 = 厂商地址 >> 2，已被闭源代码直接证明**。
   `jmo_HARDWARE_LoadState32` 入口第一条指令就是 `lsr w1, w1, #2`；
   `LoadCtrlStateNEW` 里 `orr w1, w1, #0x8010000` 拼出的命令字与开源
   `MWV207_EMIT` 的 `0x08010000 + reg` 完全一致。

3. **"遮挡寄存器不可解"是我自己的 grep 错误**。厂商头里地址是大写 5 位
   `0x03824`，我此前用小写 4 位 `0x3824` 检索。改正后
   `OCCLUSION_ADDRESS/DATA/CONTROL` 三个寄存器定义完整，缺陷**可修**。

## 数据边界与置信度

- **有据**：符号名、ELF 地址、立即数常量、调用关系 —— 直接来自二进制。
- **引用（不复制）**：寄存器名/位域/枚举值来自随包发布的公开 KMD 头
  （`/tmp/v180/usr/src/mwv207-1.8.0.uos/`，146 个 `.h`）。这些头标注
  "confidential… strictly prohibited"，故**只取事实性定义，不复制实现代码或注释**。
- **推断（已标注）**：由"哪个 HAL 函数用了这个号"反推的语义，凡无双重印证的标 未确认。
- **统计诚实性**：宽松口径下立即数命中有 ~1/3 可能来自随机巧合
  （候选空间 55297、随机命中率 2.45%），故发布数字采用最严口径（379），
  并在 `03-REGISTERS.md` §3.1 完整披露零假设检验过程。

## 重跑

```bash
cd /home/admin/jm9100-oss/reverse
python3 tools/extract_jm_regs.py      # 提取寄存器映射
python3 tools/gen_reverse_docs.py     # 生成文档
```
