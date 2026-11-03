# C1：16 位归一化纹理采样恒 0（探针与证据）

这些探针用于复现/对照 `docs/UNRESOLVED.md` 的 C1：16 位归一化采样恒 0。

## 构建与运行

```sh
gcc -O2 -o probe16d probe16d.c -lGL -lX11 -lGLX
DISPLAY=:1 LIBGL_DRIVERS_PATH=<放 libmwv207_dri.so 的目录> \
  MESA_LOADER_DRIVER_OVERRIDE=mwv207 ./probe16d
```

`probe16c/16d` 采样 1×1 纹理并读回 RGBA32F；`rt16` 只做上传/读回；`ui16` 用
`usampler2D` 测 16 位整数格式（R16UI/RG16UI/RGBA16UI）；`vs16` 在顶点着色器里取样。

## 2026-11-03 实测（r13，GPU 0x1F76）

| 探针 | 结果 |
|---|---|
| `probe16d` | RGBA8 = 1.0/0.251/0.0628、R8 = 0.502198、RG8 = 0.502198/1.0、R16F(0x3C00) = 1.0 —— 正常 |
| `probe16d` | R16_UNORM、R16_SNORM、RG16_UNORM、RG16_SNORM、RGBA16_UNORM 全部 0 |
| `rt16` | R16 上传 `3c00 4228 1111 2222`，glGetTexImage 原样读回 → 存储字节精确 |
| `ui16` | R16UI 0.234379（= 15360/65535）、RG16UI 0.234379/0.258427、RGBA16UI 0.500008/0.250004/0.125002/1.0 —— 16 位**整数**取样正常 |
| `vs16` | 顶点着色器取 R16_UNORM 也是 0 |

## 结论

把工作正常的 `R16_FLOAT` 纹理强制换成扩展码 `0x36`（R16_SNORM）后同样读 0，
同一纹理用码 `7`（RF16）正常；`TX_EXT_CONFIG.FORMAT` 码 `0x32–0x36` 全部返回 0，
而整数 `24–26`、浮点 `7–9` 正常，上传/读回字节精确，tiling 与 LINEAR、
`HORIZONAL_ALIGN`、`TX_CONFIG2.NORM_*`、`TX_CONFIG3.ENABLE_NEW_MODE`、
`TX_EXT_CONFIG` bit23/bit31 逐位试验均无变化。

→ 0x1F76 的归一化 16 位取样返回 0，属硬件解码缺口；不是编码未解，也不是驱动笔误。
