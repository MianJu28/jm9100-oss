# 寄存器地址总表（厂商视角）

本表由 `docs/tools/generate_register_map.py` 从 `register-db.json` 生成，
数据源为厂商 KMD 头文件中的 `MWV207REG_*` 定义（1392 个寄存器）。

**驱动寄存器号** = 厂商寄存器地址 `>> 2`（即下表的 `reg_id` 列）。
凡是 `reg_id` 出现在驱动命令流中的寄存器，`驱动` 列标为 ✅，并可交叉查阅
[REGISTERS-FIELDS.md](REGISTERS-FIELDS.md) 中的字段位域与驱动写入点。

- 寄存器总数：**1392**
- 驱动实际使用：**170**（12.2%）
- 字段总数：**3954**
- 命名取值总数：6514（其中 103 条为解析噪声，值为 `N:N` 形式，已在字段表中忽略）

## 块分布

| 块 | 寄存器数 | 地址区间 (min–max) | 驱动使用 |
|---|---:|---|---:|
| `?` | 32 | `0x00090`–`0x17F24` | 0 |
| `HI` | 3 | `0x00098`–`0x003A8` | 0 |
| `AHB` | 45 | `0x00154`–`0x02DA4` | 0 |
| `MMU` | 9 | `0x00180`–`0x001B0` | 0 |
| `MMUAHB` | 11 | `0x00380`–`0x003B8` | 0 |
| `CMD` | 6 | `0x003A4`–`0x039A8` | 0 |
| `AXI` | 4 | `0x003AC`–`0x0395C` | 0 |
| `DRAW` | 3 | `0x00490`–`0x0389C` | 0 |
| `FRAME` | 4 | `0x00490`–`0x004A8` | 0 |
| `REQ` | 3 | `0x00498`–`0x004A0` | 0 |
| `OVERLAY` | 5 | `0x004B0`–`0x004D0` | 0 |
| `RD` | 1 | `0x004C0`–`0x004C0` | 0 |
| `DISPLAY` | 6 | `0x004D8`–`0x00584` | 0 |
| `PANEL` | 2 | `0x004F0`–`0x004F8` | 0 |
| `WR` | 1 | `0x00500`–`0x00500` | 0 |
| `GAMMA` | 2 | `0x00538`–`0x00540` | 0 |
| `CURSOR` | 5 | `0x00548`–`0x00558` | 0 |
| `CE` | 28 | `0x00554`–`0x03200` | 0 |
| `RESERVED` | 1 | `0x00560`–`0x00560` | 0 |
| `DBI` | 6 | `0x00588`–`0x005F8` | 0 |
| `MEMORY` | 1 | `0x0058C`–`0x0058C` | 0 |
| `TIME` | 2 | `0x00590`–`0x00594` | 0 |
| `DRAM` | 1 | `0x005A4`–`0x005A4` | 0 |
| `DPI` | 1 | `0x005B0`–`0x005B0` | 0 |
| `DC` | 12 | `0x005B8`–`0x00600` | 0 |
| `PROBE` | 6 | `0x005EC`–`0x03954` | 0 |
| `DEBUG` | 2 | `0x00608`–`0x00610` | 0 |
| `FEDE` | 1 | `0x00670`–`0x00670` | 0 |
| `FE` | 42 | `0x00674`–`0x17A80` | 0 |
| `FA` | 1 | `0x007FC`–`0x007FC` | 0 |
| `AHBDEC` | 79 | `0x00800`–`0x01380` | 0 |
| `VS` | 36 | `0x00850`–`0x15634` | 0 |
| `SHADER` | 7 | `0x00860`–`0x15600` | 0 |
| `VERTEX` | 1 | `0x00864`–`0x00864` | 0 |
| `SH` | 12 | `0x00868`–`0x30000` | 0 |
| `PS` | 46 | `0x0087C`–`0x1E600` | 0 |
| `TW` | 25 | `0x00900`–`0x00960` | 0 |
| `PA` | 11 | `0x00A38`–`0x1E980` | 0 |
| `FINAL` | 1 | `0x00AAC`–`0x00AAC` | 0 |
| `POS` | 1 | `0x00AB4`–`0x00AB4` | 0 |
| `SE` | 10 | `0x00C1C`–`0x00D80` | 0 |
| `MULTI` | 10 | `0x00E04`–`0x12E80` | 0 |
| `RA` | 6 | `0x00E08`–`0x00E34` | 0 |
| `HZ` | 2 | `0x00E24`–`0x00E28` | 0 |
| `OCL` | 1 | `0x01020`–`0x01020` | 0 |
| `PIXEL` | 2 | `0x01024`–`0x36000` | 0 |
| `PSCS` | 1 | `0x0109C`–`0x0109C` | 0 |
| `GPIPE` | 2 | `0x010A8`–`0x34000` | 0 |
| `SHL1` | 1 | `0x010C4`–`0x010C4` | 0 |
| `HW` | 1 | `0x010E0`–`0x010E0` | 0 |
| `GS` | 20 | `0x01100`–`0x1562C` | 0 |
| `DE` | 113 | `0x01308`–`0x1D000` | 0 |
| `DEYUV` | 1 | `0x0130C`–`0x0130C` | 0 |
| `DEST` | 1 | `0x0132C`–`0x0132C` | 0 |
| `DEYUV2` | 8 | `0x01340`–`0x0135C` | 0 |
| `DERGB2` | 8 | `0x01360`–`0x0137C` | 0 |
| `PE` | 43 | `0x01454`–`0x17FA0` | 0 |
| `DEPTH` | 4 | `0x014C4`–`0x1EA00` | 0 |
| `STENCIL` | 4 | `0x014CC`–`0x03960` | 0 |
| `HIST` | 3 | `0x015EC`–`0x015F4` | 0 |
| `RS` | 18 | `0x016A0`–`0x01AF8` | 0 |
| `MC` | 18 | `0x016A4`–`0x01B80` | 0 |
| `MRT` | 7 | `0x017A0`–`0x01B20` | 0 |
| `MRTMC` | 1 | `0x01A20`–`0x01A20` | 0 |
| `TX` | 60 | `0x02140`–`0x17400` | 0 |
| `MCFE` | 8 | `0x02400`–`0x02B00` | 0 |
| `AHBTT` | 1 | `0x02C00`–`0x02C00` | 0 |
| `AHBTTD` | 1 | `0x02C04`–`0x02C04` | 0 |
| `AHBVIP` | 4 | `0x02C08`–`0x02C20` | 0 |
| `AHBVIPDMA` | 3 | `0x02C10`–`0x02C18` | 0 |
| `AHBXTTD` | 2 | `0x02C40`–`0x02DA0` | 0 |
| `AHBX` | 60 | `0x02C44`–`0x03304` | 0 |
| `AHBSW` | 3 | `0x02C74`–`0x02C7C` | 0 |
| `AHBXDMA` | 6 | `0x02D44`–`0x02D58` | 0 |
| `AHBXVIP` | 3 | `0x02D9C`–`0x0329C` | 0 |
| `AHBXDDR` | 2 | `0x03264`–`0x03268` | 0 |
| `AHBXVIP0` | 2 | `0x032A0`–`0x032A4` | 0 |
| `AHBXVIP1` | 2 | `0x032A8`–`0x032AC` | 0 |
| `AHBXVIP2` | 2 | `0x032B0`–`0x032B4` | 0 |
| `AHBXVIP3` | 2 | `0x032B8`–`0x032BC` | 0 |
| `AHBXDMA0` | 5 | `0x032C0`–`0x03310` | 0 |
| `AHBXDMA2` | 5 | `0x032C8`–`0x03328` | 0 |
| `AHBXDMA1` | 3 | `0x03314`–`0x0331C` | 0 |
| `AHBXDMA3` | 3 | `0x0332C`–`0x03334` | 0 |
| `AHBXDMA4` | 3 | `0x03338`–`0x03340` | 0 |
| `VARYING` | 2 | `0x03820`–`0x03834` | 0 |
| `OCCLUSION` | 3 | `0x03824`–`0x03860` | 0 |
| `COMPONENT` | 4 | `0x03828`–`0x03854` | 0 |
| `WINDOW` | 3 | `0x0383C`–`0x03844` | 0 |
| `STORE` | 1 | `0x03848`–`0x03848` | 0 |
| `PC` | 46 | `0x03858`–`0x132D4` | 0 |
| `CLOCK` | 1 | `0x03864`–`0x03864` | 0 |
| `FENCE` | 3 | `0x03868`–`0x03898` | 0 |
| `TOGGLE` | 1 | `0x0387C`–`0x0387C` | 0 |
| `USC` | 1 | `0x03884`–`0x03884` | 0 |
| `SEMANTIC` | 1 | `0x03888`–`0x03888` | 0 |
| `COLOR` | 1 | `0x038A0`–`0x038A0` | 0 |
| `APP` | 2 | `0x03900`–`0x03904` | 0 |
| `CLUSTER` | 3 | `0x03908`–`0x03980` | 0 |
| `TRIGGER` | 3 | `0x0390C`–`0x17E6C` | 0 |
| `POWER` | 2 | `0x03920`–`0x03990` | 0 |
| `CONFIG` | 3 | `0x03930`–`0x039C8` | 0 |
| `SRAM` | 3 | `0x03938`–`0x03948` | 0 |
| `ON` | 2 | `0x0393C`–`0x03940` | 0 |
| `VIP` | 9 | `0x03950`–`0x18608` | 0 |
| `VIP40` | 1 | `0x03964`–`0x03964` | 0 |
| `SP` | 3 | `0x0396C`–`0x03974` | 0 |
| `CULL` | 2 | `0x039B0`–`0x039B4` | 0 |
| `CLIP` | 2 | `0x039B8`–`0x039BC` | 0 |
| `VIEWPORT` | 7 | `0x039C0`–`0x1E940` | 0 |
| `TX3` | 1 | `0x10300`–`0x10300` | 0 |
| `PATTERN` | 2 | `0x12EA0`–`0x12EC0` | 0 |
| `TPCNV12` | 1 | `0x12F60`–`0x12F60` | 0 |
| `PCHZ` | 4 | `0x13068`–`0x13220` | 0 |
| `PCEZ` | 3 | `0x13074`–`0x1307C` | 0 |
| `PCC` | 4 | `0x13120`–`0x13180` | 0 |
| `PCZ` | 4 | `0x131A0`–`0x13200` | 0 |
| `PCTX` | 3 | `0x13260`–`0x132A0` | 0 |
| `PCL1` | 5 | `0x132D8`–`0x132E8` | 0 |
| `BLT` | 90 | `0x14000`–`0x143FC` | 0 |
| `SO` | 8 | `0x14400`–`0x144C0` | 0 |
| `TCS` | 17 | `0x14A00`–`0x15624` | 0 |
| `TPG` | 1 | `0x14B00`–`0x14B00` | 0 |
| `TES` | 17 | `0x14B04`–`0x15628` | 0 |
| `TEXTURE` | 1 | `0x14C54`–`0x14C54` | 0 |
| `SAMPLER` | 3 | `0x14C58`–`0x14C60` | 0 |
| `TPC` | 27 | `0x14C80`–`0x14CE8` | 0 |
| `ROI` | 4 | `0x17E40`–`0x17E90` | 0 |
| `OUT` | 5 | `0x17E50`–`0x17E70` | 0 |
| `SCALE` | 2 | `0x17E60`–`0x17E64` | 0 |
| `IN` | 3 | `0x17E68`–`0x17E94` | 0 |
| `YUV2` | 5 | `0x17E74`–`0x17E84` | 0 |
| `CLAMP` | 3 | `0x17E98`–`0x17EA0` | 0 |
| `TTD` | 1 | `0x17EC0`–`0x17EC0` | 0 |
| `PTN` | 6 | `0x17ED4`–`0x17EE8` | 0 |
| `CHECKSUM` | 3 | `0x17EEC`–`0x17EF4` | 0 |
| `SW` | 1 | `0x17EFC`–`0x17EFC` | 0 |
| `DENOISE` | 6 | `0x17F00`–`0x17F14` | 0 |
| `XDEEP` | 3 | `0x17F18`–`0x17F20` | 0 |
| `XVIP` | 1 | `0x17F2C`–`0x17F2C` | 0 |
| `COMP` | 1 | `0x17F30`–`0x17F30` | 0 |
| `DEC` | 24 | `0x18000`–`0x18380` | 0 |
| `WD` | 2 | `0x18400`–`0x18404` | 0 |
| `VERIF` | 1 | `0x185C0`–`0x185C0` | 0 |
| `DEC400` | 80 | `0x18800`–`0x19400` | 0 |
| `TFB` | 11 | `0x1C000`–`0x1C800` | 0 |

## 全部寄存器（按地址升序）

| 地址 | reg_id | 寄存器名 | 块 | 字段数 | 驱动 | 定义位置 |
|---|---:|---|---|---:|---|---|
| `0x00090` | `0x0024` | MWV207REG_CONTROL2 | ? | 9 |  | jmgpu_discrete.h:2278 |
| `0x00098` | `0x0026` | MWV207REG_HI_CHIP_PATCH_REV | HI | 1 |  | jmgpu_discrete.h:2587 |
| `0x0009C` | `0x0027` | MWV207REG_HI_CHIP_SPECS4 | HI | 4 |  | jmgpu_discrete.h:2606 |
| `0x00154` | `0x0055` | MWV207REG_AHB_POWER_CONTROL0 | AHB | 4 |  | jmgpu_number.h:924 |
| `0x00158` | `0x0056` | MWV207REG_AHB_POWER_CONTROL1 | AHB | 3 |  | jmgpu_number.h:959 |
| `0x0015C` | `0x0057` | MWV207REG_AHB_POWER_CONTROL2 | AHB | 1 |  | jmgpu_number.h:988 |
| `0x00160` | `0x0058` | MWV207REG_AHB_POWER_CONTROL3 | AHB | 1 |  | jmgpu_number.h:1005 |
| `0x00164` | `0x0059` | MWV207REG_AHB_POWER_CONTROL4 | AHB | 2 |  | jmgpu_number.h:1022 |
| `0x00168` | `0x005A` | MWV207REG_AHB_POWER_CONTROL5 | AHB | 2 |  | jmgpu_number.h:1045 |
| `0x0016C` | `0x005B` | MWV207REG_AHB_POWER_CONTROL6 | AHB | 2 |  | jmgpu_number.h:1068 |
| `0x00170` | `0x005C` | MWV207REG_AHB_POWER_CONTROL7 | AHB | 3 |  | jmgpu_number.h:1091 |
| `0x00174` | `0x005D` | MWV207REG_AHB_POWER_CONTROL8 | AHB | 1 |  | jmgpu_number.h:1120 |
| `0x00178` | `0x005E` | MWV207REG_AHB_POWER_CONTROL9 | AHB | 1 |  | jmgpu_number.h:1137 |
| `0x0017C` | `0x005F` | MWV207REG_AHB_POWER_CONTROL10 | AHB | 1 |  | jmgpu_number.h:1153 |
| `0x00180` | `0x0060` | MWV207REG_MMU_SAFE_ADDRESS | MMU | 1 |  | jmgpu_teamleader.h:28 |
| `0x00184` | `0x0061` | MWV207REG_MMU_CONFIGURATION | MMU | 6 |  | jmgpu_teamleader.h:66 |
| `0x00188` | `0x0062` | MWV207REG_MMU_STATUS | MMU | 4 |  | jmgpu_teamleader.h:123 |
| `0x0018C` | `0x0063` | MWV207REG_MMU_CONTROL | MMU | 5 |  | jmgpu_teamleader.h:180 |
| `0x00190` | `0x0064` | MWV207REG_MMU_EXCEPTION | MMU | 1 |  | jmgpu_teamleader.h:47 |
| `0x001A0` | `0x0068` | MWV207REG_MMU_CONFIGURATION_EXT | MMU | 8 |  | jmgpu_teamleader.h:226 |
| `0x001A8` | `0x006A` | MWV207REG_MMU_BARRIER | MMU | 3 |  | jmgpu_teamleader.h:308 |
| `0x001AC` | `0x006B` | MWV207REG_MMU_CONFIG | MMU | 8 |  | jmgpu_teamleader.h:350 |
| `0x001B0` | `0x006C` | MWV207REG_MMU_DC_CONTROL | MMU | 4 |  | jmgpu_teamleader.h:426 |
| `0x00380` | `0x00E0` | MWV207REG_MMUAHB_EXCEPTION | MMUAHB | 1 |  | jmgpu_property.h:28 |
| `0x00384` | `0x00E1` | MWV207REG_MMUAHB_STATUS | MMUAHB | 8 |  | jmgpu_property.h:47 |
| `0x00388` | `0x00E2` | MWV207REG_MMUAHB_CONTROL | MMUAHB | 7 |  | jmgpu_property.h:148 |
| `0x0038C` | `0x00E3` | MWV207REG_MMUAHB_TABLE_ARRAY_BASE_ADDRESS_LOW | MMUAHB | 1 |  | jmgpu_property.h:213 |
| `0x00390` | `0x00E4` | MWV207REG_MMUAHB_TABLE_ARRAY_BASE_ADDRESS_HIGH | MMUAHB | 3 |  | jmgpu_property.h:232 |
| `0x00394` | `0x00E5` | MWV207REG_MMUAHB_TABLE_ARRAY_SIZE | MMUAHB | 1 |  | jmgpu_property.h:263 |
| `0x00398` | `0x00E6` | MWV207REG_MMUAHB_SAFE_NON_SECURE_ADDRESS | MMUAHB | 1 |  | jmgpu_property.h:282 |
| `0x0039C` | `0x00E7` | MWV207REG_MMUAHB_SAFE_SECURE_ADDRESS | MMUAHB | 1 |  | jmgpu_property.h:301 |
| `0x003A0` | `0x00E8` | MWV207REG_MMUAHB_SAFE_ADDRESS_EXT | MMUAHB | 6 |  | jmgpu_property.h:320 |
| `0x003A4` | `0x00E9` | MWV207REG_CMD_BUFFER_AHB_CTRL | CMD | 3 |  | jmgpu_property.h:371 |
| `0x003A8` | `0x00EA` | MWV207REG_HI_AHB_CONTROL | HI | 5 |  | jmgpu_property.h:408 |
| `0x003AC` | `0x00EB` | MWV207REG_AXI_AHB_CONFIG | AXI | 7 |  | jmgpu_property.h:456 |
| `0x003B0` | `0x00EC` | MWV207REG_AHB_CONTROL0 | AHB | 13 |  | jmgpu_property.h:573 |
| `0x003B4` | `0x00ED` | MWV207REG_MMUAHB_CONTEXT_PD_ENTRY0 | MMUAHB | 4 |  | jmgpu_property.h:664 |
| `0x003B8` | `0x00EE` | MWV207REG_MMUAHB_EXCEPTION_HIGH | MMUAHB | 1 |  | jmgpu_property.h:704 |
| `0x003C0` | `0x00F0` | MWV207REG_AXI_AHB_CONFIG_EX | AXI | 8 |  | jmgpu_property.h:511 |
| `0x00484` | `0x0121` | MWV207REG_ENDIANNESS0 | ? | 1 |  | jmgpu_member.h:932 |
| `0x00488` | `0x0122` | MWV207REG_ENDIANNESS1 | ? | 1 |  | jmgpu_member.h:949 |
| `0x0048C` | `0x0123` | MWV207REG_ENDIANNESS2 | ? | 1 |  | jmgpu_member.h:966 |
| `0x00490` | `0x0124` | MWV207REG_DRAW_PRIMITIVE_START_TIME_STAMP | DRAW | 1 |  | jmgpu_member.h:983 |
| `0x00490` | `0x0124` | MWV207REG_FRAME_BUFFER_CONFIG | FRAME | 10 |  | jmgpu_namespace.h:29 |
| `0x00494` | `0x0125` | MWV207REG_DRAW_PRIMITIVE_END_TIME_STAMP | DRAW | 1 |  | jmgpu_member.h:1000 |
| `0x00498` | `0x0126` | MWV207REG_FRAME_BUFFER_ADDRESS | FRAME | 1 |  | jmgpu_namespace.h:115 |
| `0x00498` | `0x0126` | MWV207REG_REQ_BANK_ADDR_MASK | REQ | 1 |  | jmgpu_member.h:1017 |
| `0x0049C` | `0x0127` | MWV207REG_REQ_ROW_ADDR_MASK | REQ | 1 |  | jmgpu_member.h:1034 |
| `0x004A0` | `0x0128` | MWV207REG_FRAME_BUFFER_STRIDE | FRAME | 1 |  | jmgpu_namespace.h:133 |
| `0x004A0` | `0x0128` | MWV207REG_REQ_WEIGHT | REQ | 1 |  | jmgpu_member.h:1051 |
| `0x004A8` | `0x012A` | MWV207REG_FRAME_BUFFER_ORIGIN | FRAME | 2 |  | jmgpu_namespace.h:152 |
| `0x004B0` | `0x012C` | MWV207REG_OVERLAY_CONFIG | OVERLAY | 6 |  | jmgpu_namespace.h:178 |
| `0x004B8` | `0x012E` | MWV207REG_OVERLAY_ADDRESS | OVERLAY | 1 |  | jmgpu_namespace.h:236 |
| `0x004C0` | `0x0130` | MWV207REG_OVERLAY_STRIDE | OVERLAY | 1 |  | jmgpu_namespace.h:255 |
| `0x004C0` | `0x0130` | MWV207REG_RD_REQ_AGING_THRESH | RD | 1 |  | jmgpu_member.h:1067 |
| `0x004C8` | `0x0132` | MWV207REG_OVERLAY_TL | OVERLAY | 2 |  | jmgpu_namespace.h:274 |
| `0x004D0` | `0x0134` | MWV207REG_OVERLAY_BR | OVERLAY | 2 |  | jmgpu_namespace.h:299 |
| `0x004D8` | `0x0136` | MWV207REG_DISPLAY_DITHER_CONFIG | DISPLAY | 4 |  | jmgpu_namespace.h:325 |
| `0x004E0` | `0x0138` | MWV207REG_DISPLAY_DITHER_TABLE_LOW | DISPLAY | 8 |  | jmgpu_namespace.h:361 |
| `0x004E8` | `0x013A` | MWV207REG_DISPLAY_DITHER_TABLE_HIGH | DISPLAY | 8 |  | jmgpu_namespace.h:420 |
| `0x004F0` | `0x013C` | MWV207REG_PANEL_CONFIG | PANEL | 11 |  | jmgpu_namespace.h:481 |
| `0x004F8` | `0x013E` | MWV207REG_PANEL_TIMING | PANEL | 8 |  | jmgpu_namespace.h:582 |
| `0x00500` | `0x0140` | MWV207REG_HDISPLAY | ? | 2 |  | jmgpu_namespace.h:643 |
| `0x00500` | `0x0140` | MWV207REG_WR_REQ_AGING_THRESH | WR | 1 |  | jmgpu_member.h:1083 |
| `0x00508` | `0x0142` | MWV207REG_HSYNC | ? | 4 |  | jmgpu_namespace.h:668 |
| `0x00510` | `0x0144` | MWV207REG_HCOUNTER1 | ? | 4 |  | jmgpu_namespace.h:709 |
| `0x00518` | `0x0146` | MWV207REG_HCOUNTER2 | ? | 4 |  | jmgpu_namespace.h:750 |
| `0x00520` | `0x0148` | MWV207REG_VDISPLAY | ? | 2 |  | jmgpu_namespace.h:791 |
| `0x00528` | `0x014A` | MWV207REG_VSYNC | ? | 4 |  | jmgpu_namespace.h:816 |
| `0x00530` | `0x014C` | MWV207REG_DISPLAY_CURRENT_LOCATION | DISPLAY | 2 |  | jmgpu_namespace.h:857 |
| `0x00538` | `0x014E` | MWV207REG_GAMMA_INDEX | GAMMA | 1 |  | jmgpu_namespace.h:882 |
| `0x00540` | `0x0150` | MWV207REG_CONTROL | ? | 3 |  | jmgpu_member.h:1099 |
| `0x00540` | `0x0150` | MWV207REG_GAMMA_DATA | GAMMA | 3 |  | jmgpu_namespace.h:901 |
| `0x00544` | `0x0151` | MWV207REG_COUNTER0 | ? | 1 |  | jmgpu_member.h:1129 |
| `0x00548` | `0x0152` | MWV207REG_COUNTER1 | ? | 1 |  | jmgpu_member.h:1145 |
| `0x00548` | `0x0152` | MWV207REG_CURSOR_CONFIG | CURSOR | 5 |  | jmgpu_namespace.h:932 |
| `0x0054C` | `0x0153` | MWV207REG_COUNTER2 | ? | 1 |  | jmgpu_member.h:1161 |
| `0x0054C` | `0x0153` | MWV207REG_CURSOR_ADDRESS | CURSOR | 1 |  | jmgpu_namespace.h:978 |
| `0x00550` | `0x0154` | MWV207REG_COUNTER3 | ? | 1 |  | jmgpu_member.h:1177 |
| `0x00550` | `0x0154` | MWV207REG_CURSOR_LOCATION | CURSOR | 2 |  | jmgpu_namespace.h:996 |
| `0x00554` | `0x0155` | MWV207REG_CE_TRIGGER_ASYNC | CE | 1 |  | jmgpu_member.h:1196 |
| `0x00554` | `0x0155` | MWV207REG_CURSOR_BACKGROUND | CURSOR | 3 |  | jmgpu_namespace.h:1021 |
| `0x00558` | `0x0156` | MWV207REG_CONTROL0 | ? | 11 |  | jmgpu_member.h:1220 |
| `0x00558` | `0x0156` | MWV207REG_CURSOR_FOREGROUND | CURSOR | 3 |  | jmgpu_namespace.h:1052 |
| `0x0055C` | `0x0157` | MWV207REG_CONTROL1 | ? | 7 |  | jmgpu_member.h:1295 |
| `0x0055C` | `0x0157` | MWV207REG_RESERVED | ? | 1 |  | jmgpu_namespace.h:1084 |
| `0x00560` | `0x0158` | MWV207REG_RESERVED_GROUP | RESERVED | 1 |  | jmgpu_namespace.h:1103 |
| `0x00560` | `0x0158` | MWV207REG_STATUS0 | ? | 1 |  | jmgpu_member.h:1344 |
| `0x00578` | `0x015E` | MWV207REG_CONTROL3 | ? | 8 |  | jmgpu_member.h:1458 |
| `0x00580` | `0x0160` | MWV207REG_DISPLAY_INTR | DISPLAY | 5 |  | jmgpu_namespace.h:1119 |
| `0x00584` | `0x0161` | MWV207REG_DISPLAY_INTR_ENABLE | DISPLAY | 2 |  | jmgpu_namespace.h:1162 |
| `0x00588` | `0x0162` | MWV207REG_DBI_CONFIG | DBI | 6 |  | jmgpu_namespace.h:1188 |
| `0x0058C` | `0x0163` | MWV207REG_MEMORY_DIAGNOSE | MEMORY | 2 |  | jmgpu_member.h:1629 |
| `0x00590` | `0x0164` | MWV207REG_DBI_IF_RESET | DBI | 1 |  | jmgpu_namespace.h:1258 |
| `0x00590` | `0x0164` | MWV207REG_TIME_STAMP_ADDRESS | TIME | 2 |  | jmgpu_member.h:1657 |
| `0x00594` | `0x0165` | MWV207REG_TIME_STAMP_TRIGGER | TIME | 3 |  | jmgpu_member.h:1683 |
| `0x00598` | `0x0166` | MWV207REG_DBI_WR_CHAR1 | DBI | 3 |  | jmgpu_namespace.h:1278 |
| `0x005A0` | `0x0168` | MWV207REG_DBI_WR_CHAR2 | DBI | 2 |  | jmgpu_namespace.h:1307 |
| `0x005A4` | `0x0169` | MWV207REG_DRAM_CONTROL | DRAM | 3 |  | jmgpu_member.h:1791 |
| `0x005A8` | `0x016A` | MWV207REG_DBI_CMD | DBI | 2 |  | jmgpu_namespace.h:1331 |
| `0x005B0` | `0x016C` | MWV207REG_DPI_CONFIG | DPI | 1 |  | jmgpu_namespace.h:1361 |
| `0x005B8` | `0x016E` | MWV207REG_DC_CHIP_ID | DC | 1 |  | jmgpu_namespace.h:1385 |
| `0x005BC` | `0x016F` | MWV207REG_DC_CHIP_REV | DC | 1 |  | jmgpu_namespace.h:1404 |
| `0x005C0` | `0x0170` | MWV207REG_DC_CHIP_DATE | DC | 1 |  | jmgpu_namespace.h:1423 |
| `0x005C4` | `0x0171` | MWV207REG_DC_CHIP_TIME | DC | 1 |  | jmgpu_namespace.h:1442 |
| `0x005C8` | `0x0172` | MWV207REG_DC_CHIP_PATCH_REV | DC | 1 |  | jmgpu_namespace.h:1462 |
| `0x005CC` | `0x0173` | MWV207REG_RESERVED1 | ? | 1 |  | jmgpu_namespace.h:1482 |
| `0x005D0` | `0x0174` | MWV207REG_DC_TILE_IN_CFG | DC | 4 |  | jmgpu_namespace.h:1501 |
| `0x005D8` | `0x0176` | MWV207REG_DC_TILE_UV_FRAME_BUFFER_ADR | DC | 1 |  | jmgpu_namespace.h:1547 |
| `0x005E0` | `0x0178` | MWV207REG_DC_TILE_UV_FRAME_BUFFER_STR | DC | 1 |  | jmgpu_namespace.h:1566 |
| `0x005E8` | `0x017A` | MWV207REG_DC_CHIP_INFO | DC | 1 |  | jmgpu_namespace.h:1586 |
| `0x005EC` | `0x017B` | MWV207REG_DC_PRODUCT_ID | DC | 1 |  | jmgpu_namespace.h:1606 |
| `0x005EC` | `0x017B` | MWV207REG_PROBE_AXI_PROBE_CTRL | PROBE | 3 |  | jmgpu_discrete.h:4339 |
| `0x005F0` | `0x017C` | MWV207REG_DC_ECO_ID | DC | 1 |  | jmgpu_namespace.h:1626 |
| `0x005F0` | `0x017C` | MWV207REG_PROBE_AXI_PROBE_MODULE_ENABLE | PROBE | 23 |  | jmgpu_discrete.h:4403 |
| `0x005F4` | `0x017D` | MWV207REG_AHB_FULL_PIPELINE_CONFIG | AHB | 1 |  | jmgpu_discrete.h:4601 |
| `0x005F8` | `0x017E` | MWV207REG_DBI_TYPEC_CFG | DBI | 4 |  | jmgpu_namespace.h:1646 |
| `0x00600` | `0x0180` | MWV207REG_DC_STATUS | DC | 1 |  | jmgpu_namespace.h:1679 |
| `0x00608` | `0x0182` | MWV207REG_DEBUG_COUNTER_SELECT | DEBUG | 1 |  | jmgpu_namespace.h:1696 |
| `0x00610` | `0x0184` | MWV207REG_DEBUG_COUNTER_VALUE | DEBUG | 1 |  | jmgpu_namespace.h:1722 |
| `0x00670` | `0x019C` | MWV207REG_FEDE_TIMEOUT | FEDE | 1 |  | jmgpu_exclusive.h:517 |
| `0x00674` | `0x019D` | MWV207REG_FE_PRIMITIVE_RESTART | FE | 1 |  | jmgpu_exclusive.h:534 |
| `0x00678` | `0x019E` | MWV207REG_FE_INDEX_MIN | FE | 1 |  | jmgpu_exclusive.h:553 |
| `0x0067C` | `0x019F` | MWV207REG_FE_INDEX_MAX | FE | 1 |  | jmgpu_exclusive.h:572 |
| `0x00680` | `0x01A0` | MWV207REG_FE_VERTEX_ADDRESS | FE | 2 |  | jmgpu_exclusive.h:461 |
| `0x006A0` | `0x01A8` | MWV207REG_FE_VERTEX_STRIDE | FE | 3 |  | jmgpu_exclusive.h:484 |
| `0x006C0` | `0x01B0` | MWV207REG_FE_GENERICS_X | FE | 1 |  | jmgpu_exclusive.h:590 |
| `0x00700` | `0x01C0` | MWV207REG_FE_GENERICS_Y | FE | 1 |  | jmgpu_exclusive.h:609 |
| `0x00740` | `0x01D0` | MWV207REG_FE_GENERICS_Z | FE | 1 |  | jmgpu_exclusive.h:628 |
| `0x00780` | `0x01E0` | MWV207REG_FE_GENERICS_W | FE | 1 |  | jmgpu_exclusive.h:647 |
| `0x007C0` | `0x01F0` | MWV207REG_FE_PATCH_CONTROL | FE | 1 |  | jmgpu_exclusive.h:667 |
| `0x007C4` | `0x01F1` | MWV207REG_FE_ID | FE | 4 |  | jmgpu_exclusive.h:687 |
| `0x007C8` | `0x01F2` | MWV207REG_FE_RESET_ATTRIBUTES | FE | 1 |  | jmgpu_exclusive.h:737 |
| `0x007CC` | `0x01F3` | MWV207REG_FE_COMPUTE_INDIRECT | FE | 2 |  | jmgpu_exclusive.h:757 |
| `0x007D0` | `0x01F4` | MWV207REG_FE_PACKING | FE | 16 |  | jmgpu_exclusive.h:784 |
| `0x007D8` | `0x01F6` | MWV207REG_FE_CONTROL | FE | 2 |  | jmgpu_exclusive.h:894 |
| `0x007DC` | `0x01F7` | MWV207REG_FE_DESCRIPTOR_START_ADDRESS | FE | 1 |  | jmgpu_exclusive.h:925 |
| `0x007E0` | `0x01F8` | MWV207REG_FE_DESCRIPTOR_END_ADDRESS | FE | 1 |  | jmgpu_exclusive.h:944 |
| `0x007E4` | `0x01F9` | MWV207REG_FE_ASYNC_STATUS | FE | 1 |  | jmgpu_exclusive.h:963 |
| `0x007E8` | `0x01FA` | MWV207REG_FE_FENCE_DATA | FE | 1 |  | jmgpu_exclusive.h:982 |
| `0x007EC` | `0x01FB` | MWV207REG_FE_BASE_INSTANCE | FE | 1 |  | jmgpu_exclusive.h:1002 |
| `0x007F0` | `0x01FC` | MWV207REG_FE_ASYNC_CONFIG | FE | 1 |  | jmgpu_exclusive.h:1019 |
| `0x007F4` | `0x01FD` | MWV207REG_FE_FENCE_DATA_HIGH | FE | 1 |  | jmgpu_exclusive.h:1039 |
| `0x007F8` | `0x01FE` | MWV207REG_FE_INDEX_END_ADDRESS | FE | 1 |  | jmgpu_exclusive.h:1059 |
| `0x007FC` | `0x01FF` | MWV207REG_FA_CONTROL | FA | 1 |  | jmgpu_exclusive.h:1075 |
| `0x00800` | `0x0200` | MWV207REG_AHBDEC_CONTROL | AHBDEC | 21 |  | jmgpu_entity.h:29 |
| `0x00804` | `0x0201` | MWV207REG_AHBDEC_CONTROL_EX | AHBDEC | 16 |  | jmgpu_entity.h:203 |
| `0x00808` | `0x0202` | MWV207REG_AHBDEC_CONTROL_EX2 | AHBDEC | 8 |  | jmgpu_entity.h:341 |
| `0x0080C` | `0x0203` | MWV207REG_AHBDEC_INTR_ENBL | AHBDEC | 1 |  | jmgpu_entity.h:407 |
| `0x00810` | `0x0204` | MWV207REG_AHBDEC_INTR_ENBL_EX | AHBDEC | 1 |  | jmgpu_entity.h:426 |
| `0x00814` | `0x0205` | MWV207REG_AHBDEC_INTR_ENBL_EX2 | AHBDEC | 1 |  | jmgpu_entity.h:445 |
| `0x00818` | `0x0206` | MWV207REG_AHBDEC_INTR_ACKNOWLEDGE | AHBDEC | 1 |  | jmgpu_entity.h:463 |
| `0x0081C` | `0x0207` | MWV207REG_AHBDEC_INTR_ACKNOWLEDGE_EX | AHBDEC | 1 |  | jmgpu_entity.h:481 |
| `0x00820` | `0x0208` | MWV207REG_AHBDEC_INTR_ACKNOWLEDGE_EX2 | AHBDEC | 1 |  | jmgpu_entity.h:499 |
| `0x00824` | `0x0209` | MWV207REG_AHBDEC_TILE_STATUS_DEBUG | AHBDEC | 1 |  | jmgpu_entity.h:518 |
| `0x00828` | `0x020A` | MWV207REG_AHBDEC_ENCODER_DEBUG | AHBDEC | 1 |  | jmgpu_entity.h:537 |
| `0x0082C` | `0x020B` | MWV207REG_AHBDEC_DECODER_DEBUG | AHBDEC | 1 |  | jmgpu_entity.h:556 |
| `0x00830` | `0x020C` | MWV207REG_AHBDEC_TOTAL_READS_IN | AHBDEC | 1 |  | jmgpu_entity.h:575 |
| `0x00834` | `0x020D` | MWV207REG_AHBDEC_TOTAL_WRITES_IN | AHBDEC | 1 |  | jmgpu_entity.h:594 |
| `0x00838` | `0x020E` | MWV207REG_AHBDEC_TOTAL_READ_BURSTS_IN | AHBDEC | 1 |  | jmgpu_entity.h:613 |
| `0x0083C` | `0x020F` | MWV207REG_AHBDEC_TOTAL_WRITE_BURSTS_IN | AHBDEC | 1 |  | jmgpu_entity.h:632 |
| `0x00840` | `0x0210` | MWV207REG_AHBDEC_TOTAL_READS_REQ_IN | AHBDEC | 1 |  | jmgpu_entity.h:651 |
| `0x00844` | `0x0211` | MWV207REG_AHBDEC_TOTAL_WRITES_REQ_IN | AHBDEC | 1 |  | jmgpu_entity.h:670 |
| `0x00848` | `0x0212` | MWV207REG_AHBDEC_TOTAL_READ_LASTS_IN | AHBDEC | 1 |  | jmgpu_entity.h:689 |
| `0x0084C` | `0x0213` | MWV207REG_AHBDEC_TOTAL_WRITE_LASTS_IN | AHBDEC | 1 |  | jmgpu_entity.h:708 |
| `0x00850` | `0x0214` | MWV207REG_AHBDEC_TOTAL_WRITE_RESPONSE_IN | AHBDEC | 1 |  | jmgpu_entity.h:727 |
| `0x00850` | `0x0214` | MWV207REG_VS_BALANCE_INTERVAL | VS | 1 |  | jmgpu_announce.h:543 |
| `0x00854` | `0x0215` | MWV207REG_AHBDEC_TOTAL_READS_OUT | AHBDEC | 1 |  | jmgpu_entity.h:746 |
| `0x00854` | `0x0215` | MWV207REG_VS_BALANCE_THRESHOLD | VS | 1 |  | jmgpu_announce.h:561 |
| `0x00858` | `0x0216` | MWV207REG_AHBDEC_TOTAL_WRITES_OUT | AHBDEC | 1 |  | jmgpu_entity.h:765 |
| `0x00858` | `0x0216` | MWV207REG_VS_BALANCE_SHIFT | VS | 4 |  | jmgpu_announce.h:577 |
| `0x0085C` | `0x0217` | MWV207REG_AHBDEC_TOTAL_READ_BURSTS_OUT | AHBDEC | 1 |  | jmgpu_entity.h:784 |
| `0x0085C` | `0x0217` | MWV207REG_VS_PROGRAM | VS | 2 |  | jmgpu_announce.h:635 |
| `0x00860` | `0x0218` | MWV207REG_AHBDEC_TOTAL_WRITE_BURSTS_OUT | AHBDEC | 1 |  | jmgpu_entity.h:803 |
| `0x00860` | `0x0218` | MWV207REG_SHADER_CONFIG | SHADER | 17 |  | jmgpu_announce.h:661 |
| `0x00864` | `0x0219` | MWV207REG_AHBDEC_TOTAL_READS_REQ_OUT | AHBDEC | 1 |  | jmgpu_entity.h:822 |
| `0x00864` | `0x0219` | MWV207REG_VERTEX_SHADER_CONST | VERTEX | 1 |  | jmgpu_announce.h:812 |
| `0x00868` | `0x021A` | MWV207REG_AHBDEC_TOTAL_WRITES_REQ_OUT | AHBDEC | 1 |  | jmgpu_entity.h:841 |
| `0x00868` | `0x021A` | MWV207REG_SH_CACHE_CONTROL | SH | 7 |  | jmgpu_announce.h:831 |
| `0x0086C` | `0x021B` | MWV207REG_AHBDEC_TOTAL_READ_LASTS_OUT | AHBDEC | 1 |  | jmgpu_entity.h:860 |
| `0x0086C` | `0x021B` | MWV207REG_VS_INSTRUCTION | VS | 1 |  | jmgpu_announce.h:889 |
| `0x00870` | `0x021C` | MWV207REG_AHBDEC_TOTAL_WRITE_LASTS_OUT | AHBDEC | 1 |  | jmgpu_entity.h:879 |
| `0x00870` | `0x021C` | MWV207REG_VS_ATTRIBUTE | VS | 2 |  | jmgpu_announce.h:908 |
| `0x00874` | `0x021D` | MWV207REG_AHBDEC_TOTAL_WRITE_RESPONSE_OUT | AHBDEC | 1 |  | jmgpu_entity.h:898 |
| `0x00874` | `0x021D` | MWV207REG_VS_START_PC | VS | 1 |  | jmgpu_announce.h:932 |
| `0x00878` | `0x021E` | MWV207REG_AHBDEC_STATUS | AHBDEC | 3 |  | jmgpu_entity.h:917 |
| `0x00878` | `0x021E` | MWV207REG_VS_END_PC | VS | 1 |  | jmgpu_announce.h:952 |
| `0x0087C` | `0x021F` | MWV207REG_AHBDEC_DEBUG_INFO_SELECT | AHBDEC | 2 |  | jmgpu_entity.h:948 |
| `0x0087C` | `0x021F` | MWV207REG_PS_START_PC | PS | 1 |  | jmgpu_announce.h:972 |
| `0x00880` | `0x0220` | MWV207REG_AHBDEC_READ_CONFIG | AHBDEC | 9 |  | jmgpu_entity.h:972 |
| `0x00880` | `0x0220` | MWV207REG_PS_END_PC | PS | 1 |  | jmgpu_announce.h:992 |
| `0x00884` | `0x0221` | MWV207REG_SH_SAMPLER_ARBITRATION | SH | 2 |  | jmgpu_announce.h:1011 |
| `0x00888` | `0x0222` | MWV207REG_SH_POWER_MANAGEMENT | SH | 2 |  | jmgpu_announce.h:1042 |
| `0x0088C` | `0x0223` | MWV207REG_VS_INSTRUCTION_PREFETCH | VS | 1 |  | jmgpu_announce.h:1068 |
| `0x00890` | `0x0224` | MWV207REG_VS_INSTRUCTION_PREFETCH_END | VS | 1 |  | jmgpu_announce.h:1087 |
| `0x00894` | `0x0225` | MWV207REG_SHADER_CONFIG2 | SHADER | 1 |  | jmgpu_announce.h:1106 |
| `0x00898` | `0x0226` | MWV207REG_VS_UNPACK | VS | 16 |  | jmgpu_announce.h:1126 |
| `0x008A0` | `0x0228` | MWV207REG_VS_THROTTLE | VS | 3 |  | jmgpu_announce.h:1236 |
| `0x008A4` | `0x0229` | MWV207REG_VS_UNIFORM_LOCK | VS | 1 |  | jmgpu_announce.h:1341 |
| `0x008A8` | `0x022A` | MWV207REG_VS_SAMPLER_BASE | VS | 2 |  | jmgpu_announce.h:1361 |
| `0x008AC` | `0x022B` | MWV207REG_VS_SAMPLER_LOCK | VS | 2 |  | jmgpu_announce.h:1386 |
| `0x008B0` | `0x022C` | MWV207REG_SH_ICACHE_INVALIDATE | SH | 5 |  | jmgpu_announce.h:1412 |
| `0x008B4` | `0x022D` | MWV207REG_SHADER_STEERING | SHADER | 1 |  | jmgpu_announce.h:1455 |
| `0x008B8` | `0x022E` | MWV207REG_SHADER_STATIC | SHADER | 12 |  | jmgpu_announce.h:1476 |
| `0x008BC` | `0x022F` | MWV207REG_VS_RELATIVE_END | VS | 1 |  | jmgpu_announce.h:1584 |
| `0x008C0` | `0x0230` | MWV207REG_VS_INPUT | VS | 4 |  | jmgpu_announce.h:1266 |
| `0x008E0` | `0x0238` | MWV207REG_VS_OUTPUT | VS | 4 |  | jmgpu_announce.h:1304 |
| `0x00900` | `0x0240` | MWV207REG_AHBDEC_READ_EX_CONFIG | AHBDEC | 10 |  | jmgpu_entity.h:1131 |
| `0x00900` | `0x0240` | MWV207REG_TW_CONFIG | TW | 11 |  | jmgpu_announce.h:1601 |
| `0x00904` | `0x0241` | MWV207REG_TW_INFO_X | TW | 2 |  | jmgpu_announce.h:1701 |
| `0x00908` | `0x0242` | MWV207REG_TW_INFO_Y | TW | 2 |  | jmgpu_announce.h:1724 |
| `0x0090C` | `0x0243` | MWV207REG_TW_INFO_Z | TW | 2 |  | jmgpu_announce.h:1747 |
| `0x00910` | `0x0244` | MWV207REG_TW_WORK_GROUP_X | TW | 2 |  | jmgpu_announce.h:1772 |
| `0x00914` | `0x0245` | MWV207REG_TW_WORK_GROUP_Y | TW | 2 |  | jmgpu_announce.h:1797 |
| `0x00918` | `0x0246` | MWV207REG_TW_WORK_GROUP_Z | TW | 2 |  | jmgpu_announce.h:1822 |
| `0x0091C` | `0x0247` | MWV207REG_TW_SHADER_INFO | TW | 1 |  | jmgpu_announce.h:1845 |
| `0x00920` | `0x0248` | MWV207REG_TW_TRIGGER | TW | 1 |  | jmgpu_announce.h:1861 |
| `0x00924` | `0x0249` | MWV207REG_TW_SHADER_INFO2 | TW | 4 |  | jmgpu_announce.h:1878 |
| `0x00928` | `0x024A` | MWV207REG_TW_INFO_GLOBAL_SIZE_X | TW | 1 |  | jmgpu_announce.h:1916 |
| `0x0092C` | `0x024B` | MWV207REG_TW_INFO_GLOBAL_OFFSET_X | TW | 1 |  | jmgpu_announce.h:1933 |
| `0x00930` | `0x024C` | MWV207REG_TW_INFO_GLOBAL_SIZE_Y | TW | 1 |  | jmgpu_announce.h:1950 |
| `0x00934` | `0x024D` | MWV207REG_TW_INFO_GLOBAL_OFFSET_Y | TW | 1 |  | jmgpu_announce.h:1967 |
| `0x00938` | `0x024E` | MWV207REG_TW_INFO_GLOBAL_SIZE_Z | TW | 1 |  | jmgpu_announce.h:1984 |
| `0x0093C` | `0x024F` | MWV207REG_TW_INFO_GLOBAL_OFFSET_Z | TW | 1 |  | jmgpu_announce.h:2001 |
| `0x00940` | `0x0250` | MWV207REG_TW_WORK_GROUP_COUNT_X | TW | 1 |  | jmgpu_announce.h:2018 |
| `0x00944` | `0x0251` | MWV207REG_TW_WORK_GROUP_COUNT_Y | TW | 1 |  | jmgpu_announce.h:2034 |
| `0x00948` | `0x0252` | MWV207REG_TW_WORK_GROUP_COUNT_Z | TW | 1 |  | jmgpu_announce.h:2050 |
| `0x0094C` | `0x0253` | MWV207REG_TW_WORKGROUP_SIZE_X | TW | 1 |  | jmgpu_announce.h:2066 |
| `0x00950` | `0x0254` | MWV207REG_TW_WORKGROUP_SIZE_Y | TW | 1 |  | jmgpu_announce.h:2083 |
| `0x00954` | `0x0255` | MWV207REG_TW_WORKGROUP_SIZE_Z | TW | 1 |  | jmgpu_announce.h:2100 |
| `0x00958` | `0x0256` | MWV207REG_TW_INFO_GLOBAL_SCALE_X | TW | 1 |  | jmgpu_announce.h:2117 |
| `0x0095C` | `0x0257` | MWV207REG_TW_INFO_GLOBAL_SCALE_Y | TW | 1 |  | jmgpu_announce.h:2134 |
| `0x00960` | `0x0258` | MWV207REG_TW_INFO_GLOBAL_SCALE_Z | TW | 1 |  | jmgpu_announce.h:2151 |
| `0x00980` | `0x0260` | MWV207REG_AHBDEC_WRITE_CONFIG | AHBDEC | 8 |  | jmgpu_entity.h:1234 |
| `0x00A00` | `0x0280` | MWV207REG_AHBDEC_WRITE_EX_CONFIG | AHBDEC | 10 |  | jmgpu_entity.h:1384 |
| `0x00A38` | `0x028E` | MWV207REG_PA_LINE_ADJUST_SUB | PA | 1 |  | jmgpu_nearest.h:701 |
| `0x00A3C` | `0x028F` | MWV207REG_PA_LINE_ADJUST_ADD | PA | 1 |  | jmgpu_nearest.h:719 |
| `0x00A80` | `0x02A0` | MWV207REG_AHBDEC_READ_BUFFER_BASE | AHBDEC | 1 |  | jmgpu_entity.h:1492 |
| `0x00A80` | `0x02A0` | MWV207REG_PA_CLIP_W | PA | 1 |  | jmgpu_nearest.h:738 |
| `0x00A84` | `0x02A1` | MWV207REG_PA_MAX_POINT_SIZE | PA | 1 |  | jmgpu_nearest.h:757 |
| `0x00A88` | `0x02A2` | MWV207REG_PA_CLIP_CONTROL | PA | 10 |  | jmgpu_nearest.h:776 |
| `0x00A8C` | `0x02A3` | MWV207REG_PA_SMALL_W | PA | 1 |  | jmgpu_nearest.h:875 |
| `0x00A90` | `0x02A4` | MWV207REG_PA_PACK | PA | 8 |  | jmgpu_nearest.h:894 |
| `0x00AA8` | `0x02AA` | MWV207REG_PA_CONTROL | PA | 3 |  | jmgpu_nearest.h:1031 |
| `0x00AAC` | `0x02AB` | MWV207REG_FINAL_PA_CONTROL | FINAL | 1 |  | jmgpu_nearest.h:1065 |
| `0x00AB0` | `0x02AC` | MWV207REG_PA_CONTROL_EX | PA | 6 |  | jmgpu_nearest.h:1082 |
| `0x00AB4` | `0x02AD` | MWV207REG_POS_PA_CLUSTER_CTRL | POS | 1 |  | jmgpu_nearest.h:1142 |
| `0x00AB8` | `0x02AE` | MWV207REG_PA_DEBUG_MODE | PA | 4 |  | jmgpu_nearest.h:1161 |
| `0x00B00` | `0x02C0` | MWV207REG_AHBDEC_READ_BUFFER_BASE_EX | AHBDEC | 1 |  | jmgpu_entity.h:1511 |
| `0x00B80` | `0x02E0` | MWV207REG_AHBDEC_READ_BUFFER_END | AHBDEC | 1 |  | jmgpu_entity.h:1529 |
| `0x00C00` | `0x0300` | MWV207REG_AHBDEC_READ_BUFFER_END_EX | AHBDEC | 1 |  | jmgpu_entity.h:1547 |
| `0x00C1C` | `0x0307` | MWV207REG_SE_AREA_THRESHOLD | SE | 1 |  | jmgpu_borders.h:182 |
| `0x00C20` | `0x0308` | MWV207REG_SE_CLIP_COMPARE_RIGHT | SE | 1 |  | jmgpu_borders.h:201 |
| `0x00C24` | `0x0309` | MWV207REG_SE_CLIP_COMPARE_BOTTOM | SE | 1 |  | jmgpu_borders.h:221 |
| `0x00C28` | `0x030A` | MWV207REG_SE_DEPTH_BIAS_CLAMP | SE | 1 |  | jmgpu_borders.h:241 |
| `0x00C40` | `0x0310` | MWV207REG_SE_CLIP_LEFT_EX | SE | 1 |  | jmgpu_borders.h:262 |
| `0x00C80` | `0x0320` | MWV207REG_AHBDEC_READ_FLUSH_CACHE | AHBDEC | 1 |  | jmgpu_entity.h:1566 |
| `0x00C80` | `0x0320` | MWV207REG_SE_CLIP_TOP_EX | SE | 1 |  | jmgpu_borders.h:282 |
| `0x00CC0` | `0x0330` | MWV207REG_SE_CLIP_RIGHT_EX | SE | 1 |  | jmgpu_borders.h:302 |
| `0x00D00` | `0x0340` | MWV207REG_AHBDEC_READ_FLUSH_CACHE_EX | AHBDEC | 1 |  | jmgpu_entity.h:1585 |
| `0x00D00` | `0x0340` | MWV207REG_SE_CLIP_BOTTOM_EX | SE | 1 |  | jmgpu_borders.h:321 |
| `0x00D40` | `0x0350` | MWV207REG_SE_CLIP_COMPARE_RIGHT_EX | SE | 1 |  | jmgpu_borders.h:339 |
| `0x00D80` | `0x0360` | MWV207REG_AHBDEC_WRITE_BUFFER_BASE | AHBDEC | 1 |  | jmgpu_entity.h:1604 |
| `0x00D80` | `0x0360` | MWV207REG_SE_CLIP_COMPARE_BOTTOM_EX | SE | 1 |  | jmgpu_borders.h:359 |
| `0x00E00` | `0x0380` | MWV207REG_AHBDEC_WRITE_BUFFER_BASE_EX | AHBDEC | 1 |  | jmgpu_entity.h:1623 |
| `0x00E04` | `0x0381` | MWV207REG_MULTI_SAMPLE_JITTER | MULTI | 16 |  | jmgpu_protocol.h:135 |
| `0x00E08` | `0x0382` | MWV207REG_RA_CONTROL | RA | 13 |  | jmgpu_protocol.h:306 |
| `0x00E0C` | `0x0383` | MWV207REG_RA_MEDIUMP_MASK | RA | 2 |  | jmgpu_protocol.h:445 |
| `0x00E10` | `0x0384` | MWV207REG_MULTI_SAMPLE_COORDS | MULTI | 8 |  | jmgpu_protocol.h:74 |
| `0x00E20` | `0x0388` | MWV207REG_RA_CONTROL_HZ | RA | 6 |  | jmgpu_protocol.h:468 |
| `0x00E24` | `0x0389` | MWV207REG_HZ_BASE_ADDRESS1 | HZ | 1 |  | jmgpu_protocol.h:555 |
| `0x00E28` | `0x038A` | MWV207REG_HZ_TILE_STATUS1 | HZ | 1 |  | jmgpu_protocol.h:574 |
| `0x00E2C` | `0x038B` | MWV207REG_RA_DEBUG | RA | 3 |  | jmgpu_protocol.h:593 |
| `0x00E30` | `0x038C` | MWV207REG_RA_TIME_OUT | RA | 1 |  | jmgpu_protocol.h:631 |
| `0x00E34` | `0x038D` | MWV207REG_RA_CONTROL_EX | RA | 5 |  | jmgpu_protocol.h:650 |
| `0x00E40` | `0x0390` | MWV207REG_CENTROIDS | ? | 8 |  | jmgpu_protocol.h:244 |
| `0x00E80` | `0x03A0` | MWV207REG_AHBDEC_WRITE_BUFFER_END | AHBDEC | 1 |  | jmgpu_entity.h:1641 |
| `0x00F00` | `0x03C0` | MWV207REG_AHBDEC_WRITE_BUFFER_END_EX | AHBDEC | 1 |  | jmgpu_entity.h:1659 |
| `0x00F80` | `0x03E0` | MWV207REG_AHBDEC_WRITE_FLUSH_CACHE | AHBDEC | 1 |  | jmgpu_entity.h:1678 |
| `0x01000` | `0x0400` | MWV207REG_AHBDEC_WRITE_FLUSH_CACHE_EX | AHBDEC | 1 |  | jmgpu_entity.h:1697 |
| `0x0101C` | `0x0407` | MWV207REG_PS_PROGRAM | PS | 2 |  | jmgpu_announce.h:3011 |
| `0x01020` | `0x0408` | MWV207REG_OCL_FEATURE_CONTROL | OCL | 1 |  | jmgpu_announce.h:3110 |
| `0x01024` | `0x0409` | MWV207REG_PIXEL_SHADER_CONST | PIXEL | 1 |  | jmgpu_announce.h:3129 |
| `0x01028` | `0x040A` | MWV207REG_PS_INSTRUCTION | PS | 1 |  | jmgpu_announce.h:3147 |
| `0x0102C` | `0x040B` | MWV207REG_PS_OUTPUT_CONTROL | PS | 12 |  | jmgpu_announce.h:3167 |
| `0x01030` | `0x040C` | MWV207REG_PS_OUTPUT_MODE | PS | 8 |  | jmgpu_announce.h:3257 |
| `0x01034` | `0x040D` | MWV207REG_PS_OUTPUT_PRECISION | PS | 8 |  | jmgpu_announce.h:3398 |
| `0x01038` | `0x040E` | MWV207REG_PS_OUTPUT_PRECISION_EX | PS | 8 |  | jmgpu_announce.h:3463 |
| `0x0103C` | `0x040F` | MWV207REG_PS_OUTPUT_MAP | PS | 8 |  | jmgpu_announce.h:3798 |
| `0x01040` | `0x0410` | MWV207REG_PS_INPUT_TYPE | PS | 32 |  | jmgpu_announce.h:3528 |
| `0x01048` | `0x0412` | MWV207REG_PS_INSTRUCTION_PREFETCH | PS | 1 |  | jmgpu_announce.h:3860 |
| `0x0104C` | `0x0413` | MWV207REG_PS_INSTRUCTION_PREFETCH_END | PS | 1 |  | jmgpu_announce.h:3879 |
| `0x01050` | `0x0414` | MWV207REG_PS_UNIFORM_LOCK | PS | 1 |  | jmgpu_announce.h:3897 |
| `0x01054` | `0x0415` | MWV207REG_PS_SAMPLE_COVERAGE_CTRL | PS | 12 |  | jmgpu_announce.h:3914 |
| `0x01058` | `0x0416` | MWV207REG_PS_SAMPLER_BASE | PS | 2 |  | jmgpu_announce.h:4140 |
| `0x0105C` | `0x0417` | MWV207REG_PS_SAMPLER_LOCK | PS | 2 |  | jmgpu_announce.h:4165 |
| `0x01060` | `0x0418` | MWV207REG_PS_SAMPLE_DITHER_TABLE | PS | 8 |  | jmgpu_announce.h:4024 |
| `0x01080` | `0x0420` | MWV207REG_AHBDEC_READ_CACHE_BASE | AHBDEC | 1 |  | jmgpu_entity.h:1716 |
| `0x01080` | `0x0420` | MWV207REG_PS_UNPACK | PS | 8 |  | jmgpu_announce.h:4086 |
| `0x01090` | `0x0424` | MWV207REG_PS_RELATIVE_END | PS | 1 |  | jmgpu_announce.h:4191 |
| `0x01094` | `0x0425` | MWV207REG_PS_INSTRUCTION_PREFETCH_RELATIVE_END | PS | 1 |  | jmgpu_announce.h:4209 |
| `0x01098` | `0x0426` | MWV207REG_PS_MAX | PS | 3 |  | jmgpu_announce.h:4228 |
| `0x0109C` | `0x0427` | MWV207REG_PSCS_THROTTLE | PSCS | 3 |  | jmgpu_announce.h:4257 |
| `0x010A0` | `0x0428` | MWV207REG_PS_TRIGGER_NN | PS | 2 |  | jmgpu_announce.h:4286 |
| `0x010A4` | `0x0429` | MWV207REG_PS_WAIT_FOR_EVENT | PS | 1 |  | jmgpu_announce.h:4311 |
| `0x010A8` | `0x042A` | MWV207REG_GPIPE_LOAD_BALANCING | GPIPE | 3 |  | jmgpu_announce.h:4330 |
| `0x010AC` | `0x042B` | MWV207REG_SH_ALLOC_UNIFORMS | SH | 3 |  | jmgpu_announce.h:4361 |
| `0x010B0` | `0x042C` | MWV207REG_SH_ALLOC_SAMPLERS | SH | 3 |  | jmgpu_announce.h:4395 |
| `0x010B4` | `0x042D` | MWV207REG_SH_ALLOC_TEXTURES | SH | 3 |  | jmgpu_announce.h:4429 |
| `0x010B8` | `0x042E` | MWV207REG_PS_TRIGGER_TP | PS | 2 |  | jmgpu_announce.h:4463 |
| `0x010BC` | `0x042F` | MWV207REG_PS_TRIGGER_CUSTOMER_SUPER_INSTRUCTION | PS | 2 |  | jmgpu_announce.h:4488 |
| `0x010C0` | `0x0430` | MWV207REG_SH_STATE_CONTROL | SH | 2 |  | jmgpu_announce.h:4513 |
| `0x010C4` | `0x0431` | MWV207REG_SHL1_LS_CONTROL | SHL1 | 4 |  | jmgpu_announce.h:4543 |
| `0x010C8` | `0x0432` | MWV207REG_PS_OUTPUT_CONTROL1 | PS | 8 |  | jmgpu_announce.h:4586 |
| `0x010CC` | `0x0433` | MWV207REG_PS_OUTPUT_CONTROL2 | PS | 8 |  | jmgpu_announce.h:4645 |
| `0x010D0` | `0x0434` | MWV207REG_SH_INTERNAL_CONTROL | SH | 1 |  | jmgpu_announce.h:4707 |
| `0x010D4` | `0x0435` | MWV207REG_PS_INPUT_HPVECFMT | PS | 32 |  | jmgpu_announce.h:4726 |
| `0x010D8` | `0x0436` | MWV207REG_PS_OUTPUT_PRECVECFMT | PS | 8 |  | jmgpu_announce.h:4964 |
| `0x010DC` | `0x0437` | MWV207REG_SH_INTERNAL_CONTROL1 | SH | 2 |  | jmgpu_announce.h:5050 |
| `0x010E0` | `0x0438` | MWV207REG_HW_INTERNAL_CONTROL | HW | 1 |  | jmgpu_announce.h:5074 |
| `0x010E4` | `0x0439` | MWV207REG_PS_SAMPLE_COVERAGE_CTRL_EX | PS | 1 |  | jmgpu_announce.h:5093 |
| `0x01100` | `0x0440` | MWV207REG_AHBDEC_READ_CACHE_BASE_EX | AHBDEC | 1 |  | jmgpu_entity.h:1735 |
| `0x01100` | `0x0440` | MWV207REG_GS_CONTROL | GS | 9 |  | jmgpu_announce.h:2172 |
| `0x01104` | `0x0441` | MWV207REG_GS_TYPE | GS | 1 |  | jmgpu_announce.h:2257 |
| `0x01108` | `0x0442` | MWV207REG_GS_COUNTERS | GS | 2 |  | jmgpu_announce.h:2287 |
| `0x0110C` | `0x0443` | MWV207REG_GS_START | GS | 1 |  | jmgpu_announce.h:2347 |
| `0x01110` | `0x0444` | MWV207REG_GS_RELATIVE_END | GS | 1 |  | jmgpu_announce.h:2367 |
| `0x01114` | `0x0445` | MWV207REG_GS_INSTRUCTION | GS | 1 |  | jmgpu_announce.h:2385 |
| `0x01118` | `0x0446` | MWV207REG_GS_INSTRUCTION_PREFETCH | GS | 1 |  | jmgpu_announce.h:2405 |
| `0x0111C` | `0x0447` | MWV207REG_GS_INSTRUCTION_PREFETCH_RELATIVE_END | GS | 1 |  | jmgpu_announce.h:2424 |
| `0x01120` | `0x0448` | MWV207REG_GS_OUTPUT | GS | 4 |  | jmgpu_announce.h:2313 |
| `0x01140` | `0x0450` | MWV207REG_GS_THROTTLE | GS | 3 |  | jmgpu_announce.h:2443 |
| `0x01144` | `0x0451` | MWV207REG_GS_ATTRIBUTE | GS | 3 |  | jmgpu_announce.h:2472 |
| `0x01148` | `0x0452` | MWV207REG_GS_ATTRIBUTE_EX | GS | 1 |  | jmgpu_announce.h:2501 |
| `0x0114C` | `0x0453` | MWV207REG_GS_UNIFORM_BASE | GS | 1 |  | jmgpu_announce.h:2520 |
| `0x01150` | `0x0454` | MWV207REG_GS_UNIFORM_LOCK | GS | 1 |  | jmgpu_announce.h:2539 |
| `0x01154` | `0x0455` | MWV207REG_GS_SAMPLER_BASE | GS | 2 |  | jmgpu_announce.h:2559 |
| `0x01158` | `0x0456` | MWV207REG_GS_SAMPLER_LOCK | GS | 2 |  | jmgpu_announce.h:2584 |
| `0x0115C` | `0x0457` | MWV207REG_GS_THROTTLE_EX | GS | 1 |  | jmgpu_announce.h:2610 |
| `0x01160` | `0x0458` | MWV207REG_GS_ATTRIBUTE_GLOBAL_CONTROL | GS | 3 |  | jmgpu_announce.h:2629 |
| `0x01180` | `0x0460` | MWV207REG_AHBDEC_WRITE_CACHE_BASE | AHBDEC | 1 |  | jmgpu_entity.h:1754 |
| `0x01200` | `0x0480` | MWV207REG_AHBDEC_WRITE_CACHE_BASE_EX | AHBDEC | 1 |  | jmgpu_entity.h:1773 |
| `0x01280` | `0x04A0` | MWV207REG_AHBDEC_DEBUG_INFO_OUT | AHBDEC | 1 |  | jmgpu_entity.h:1792 |
| `0x01284` | `0x04A1` | MWV207REG_AHBDEC_DEBUG0 | AHBDEC | 1 |  | jmgpu_entity.h:1811 |
| `0x01288` | `0x04A2` | MWV207REG_AHBDEC_DEBUG1 | AHBDEC | 1 |  | jmgpu_entity.h:1830 |
| `0x0128C` | `0x04A3` | MWV207REG_AHBDEC_DEBUG2 | AHBDEC | 1 |  | jmgpu_entity.h:1849 |
| `0x01290` | `0x04A4` | MWV207REG_AHBDEC_DEBUG3 | AHBDEC | 1 |  | jmgpu_entity.h:1868 |
| `0x01294` | `0x04A5` | MWV207REG_AHBDEC_DEBUG4 | AHBDEC | 1 |  | jmgpu_entity.h:1884 |
| `0x01298` | `0x04A6` | MWV207REG_AHBDEC_DEBUG5 | AHBDEC | 1 |  | jmgpu_entity.h:1900 |
| `0x0129C` | `0x04A7` | MWV207REG_AHBDEC_STATE_COMMIT | AHBDEC | 1 |  | jmgpu_entity.h:1918 |
| `0x012A0` | `0x04A8` | MWV207REG_AHBDEC_DEBUG6 | AHBDEC | 1 |  | jmgpu_entity.h:1934 |
| `0x012A4` | `0x04A9` | MWV207REG_AHBDEC_DEBUG7 | AHBDEC | 1 |  | jmgpu_entity.h:1950 |
| `0x012A8` | `0x04AA` | MWV207REG_AHBDEC_CR_DEBUG_TILE128_TYPE0 | AHBDEC | 1 |  | jmgpu_entity.h:1969 |
| `0x012AC` | `0x04AB` | MWV207REG_AHBDEC_CR_DEBUG_TILE128_TYPE1 | AHBDEC | 1 |  | jmgpu_entity.h:1988 |
| `0x012B0` | `0x04AC` | MWV207REG_AHBDEC_CR_DEBUG_TILE128_TYPE2 | AHBDEC | 1 |  | jmgpu_entity.h:2007 |
| `0x012B4` | `0x04AD` | MWV207REG_AHBDEC_CR_DEBUG_TILE128_TYPE3 | AHBDEC | 1 |  | jmgpu_entity.h:2026 |
| `0x012B8` | `0x04AE` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE0 | AHBDEC | 1 |  | jmgpu_entity.h:2045 |
| `0x012BC` | `0x04AF` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE1 | AHBDEC | 1 |  | jmgpu_entity.h:2064 |
| `0x012C0` | `0x04B0` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE2 | AHBDEC | 1 |  | jmgpu_entity.h:2083 |
| `0x012C4` | `0x04B1` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE3 | AHBDEC | 1 |  | jmgpu_entity.h:2102 |
| `0x012C8` | `0x04B2` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE4 | AHBDEC | 1 |  | jmgpu_entity.h:2121 |
| `0x012CC` | `0x04B3` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE5 | AHBDEC | 1 |  | jmgpu_entity.h:2140 |
| `0x012D0` | `0x04B4` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE6 | AHBDEC | 1 |  | jmgpu_entity.h:2159 |
| `0x012D4` | `0x04B5` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE7 | AHBDEC | 1 |  | jmgpu_entity.h:2178 |
| `0x012D8` | `0x04B6` | MWV207REG_AHBDEC_DEBUG_READ_GATE_DOMAIN_CLK_COUNTER | AHBDEC | 1 |  | jmgpu_entity.h:2197 |
| `0x012DC` | `0x04B7` | MWV207REG_AHBDEC_DEBUG_WRITE_GATE_DOMAIN_CLK_COUNTER | AHBDEC | 1 |  | jmgpu_entity.h:2216 |
| `0x012E0` | `0x04B8` | MWV207REG_AHBDEC_DEBUG_OTHER_GATE_DOMAIN_CLK_COUNTER | AHBDEC | 1 |  | jmgpu_entity.h:2235 |
| `0x01300` | `0x04C0` | MWV207REG_AHBDEC_FAST_CLEAR_VALUE | AHBDEC | 1 |  | jmgpu_entity.h:2254 |
| `0x01308` | `0x04C2` | MWV207REG_DE_MULTI_SOURCE | DE | 6 |  | jmgpu_interview.h:2918 |
| `0x0130C` | `0x04C3` | MWV207REG_DEYUV_CONVERSION | DEYUV | 16 |  | jmgpu_interview.h:2990 |
| `0x01310` | `0x04C4` | MWV207REG_DE_PLANE2_ADDRESS | DE | 2 |  | jmgpu_interview.h:3155 |
| `0x01314` | `0x04C5` | MWV207REG_DE_PLANE2_STRIDE | DE | 1 |  | jmgpu_interview.h:3180 |
| `0x01318` | `0x04C6` | MWV207REG_DE_PLANE3_ADDRESS | DE | 2 |  | jmgpu_interview.h:3199 |
| `0x0131C` | `0x04C7` | MWV207REG_DE_PLANE3_STRIDE | DE | 1 |  | jmgpu_interview.h:3225 |
| `0x01320` | `0x04C8` | MWV207REG_DE_STALL_DE | DE | 1 |  | jmgpu_interview.h:3244 |
| `0x01324` | `0x04C9` | MWV207REG_DE_BLOCK_SIZE | DE | 2 |  | jmgpu_interview.h:3299 |
| `0x01328` | `0x04CA` | MWV207REG_DE_GENERAL_CONFIG | DE | 16 |  | jmgpu_interview.h:3323 |
| `0x0132C` | `0x04CB` | MWV207REG_DEST_CONFIG_EX | DEST | 16 |  | jmgpu_interview.h:3461 |
| `0x01330` | `0x04CC` | MWV207REG_DE_PATTERN_COLOR | DE | 4 |  | jmgpu_interview.h:3266 |
| `0x01340` | `0x04D0` | MWV207REG_DEYUV2_RGB_COEF0 | DEYUV2 | 2 |  | jmgpu_interview.h:3608 |
| `0x01344` | `0x04D1` | MWV207REG_DEYUV2_RGB_COEF1 | DEYUV2 | 2 |  | jmgpu_interview.h:3632 |
| `0x01348` | `0x04D2` | MWV207REG_DEYUV2_RGB_COEF2 | DEYUV2 | 2 |  | jmgpu_interview.h:3656 |
| `0x0134C` | `0x04D3` | MWV207REG_DEYUV2_RGB_COEF3 | DEYUV2 | 2 |  | jmgpu_interview.h:3680 |
| `0x01350` | `0x04D4` | MWV207REG_DEYUV2_RGB_COEF4 | DEYUV2 | 2 |  | jmgpu_interview.h:3704 |
| `0x01354` | `0x04D5` | MWV207REG_DEYUV2_RGB_COEF_D0 | DEYUV2 | 1 |  | jmgpu_interview.h:3728 |
| `0x01358` | `0x04D6` | MWV207REG_DEYUV2_RGB_COEF_D1 | DEYUV2 | 1 |  | jmgpu_interview.h:3747 |
| `0x0135C` | `0x04D7` | MWV207REG_DEYUV2_RGB_COEF_D2 | DEYUV2 | 1 |  | jmgpu_interview.h:3766 |
| `0x01360` | `0x04D8` | MWV207REG_DERGB2_YUV_COEF0 | DERGB2 | 2 |  | jmgpu_interview.h:3785 |
| `0x01364` | `0x04D9` | MWV207REG_DERGB2_YUV_COEF1 | DERGB2 | 2 |  | jmgpu_interview.h:3809 |
| `0x01368` | `0x04DA` | MWV207REG_DERGB2_YUV_COEF2 | DERGB2 | 2 |  | jmgpu_interview.h:3833 |
| `0x0136C` | `0x04DB` | MWV207REG_DERGB2_YUV_COEF3 | DERGB2 | 2 |  | jmgpu_interview.h:3857 |
| `0x01370` | `0x04DC` | MWV207REG_DERGB2_YUV_COEF4 | DERGB2 | 2 |  | jmgpu_interview.h:3881 |
| `0x01374` | `0x04DD` | MWV207REG_DERGB2_YUV_COEF_D0 | DERGB2 | 1 |  | jmgpu_interview.h:3905 |
| `0x01378` | `0x04DE` | MWV207REG_DERGB2_YUV_COEF_D1 | DERGB2 | 1 |  | jmgpu_interview.h:3924 |
| `0x0137C` | `0x04DF` | MWV207REG_DERGB2_YUV_COEF_D2 | DERGB2 | 1 |  | jmgpu_interview.h:3943 |
| `0x01380` | `0x04E0` | MWV207REG_AHBDEC_FAST_CLEAR_VALUE_EX | AHBDEC | 1 |  | jmgpu_entity.h:2274 |
| `0x01380` | `0x04E0` | MWV207REG_DE_WALKER_CONFIG | DE | 4 |  | jmgpu_interview.h:3959 |
| `0x01384` | `0x04E1` | MWV207REG_DE_DST_TILE_STATUS_ADDRESS | DE | 2 |  | jmgpu_interview.h:4006 |
| `0x01388` | `0x04E2` | MWV207REG_DE_REORDER_CONTROL | DE | 2 |  | jmgpu_interview.h:4032 |
| `0x01398` | `0x04E6` | MWV207REG_DE_DST_TILE_STATUS_CLEAR | DE | 1 |  | jmgpu_interview.h:4198 |
| `0x0139C` | `0x04E7` | MWV207REG_DE_DST_TILE_STATUS_CONFIG | DE | 5 |  | jmgpu_interview.h:4214 |
| `0x013A0` | `0x04E8` | MWV207REG_DE_DST_PLANE0_ALIGN_CONFIG | DE | 1 |  | jmgpu_interview.h:4277 |
| `0x013A4` | `0x04E9` | MWV207REG_DE_DST_PLANE1_ALIGN_CONFIG | DE | 1 |  | jmgpu_interview.h:4293 |
| `0x013A8` | `0x04EA` | MWV207REG_DE_DST_PLANE2_ALIGN_CONFIG | DE | 1 |  | jmgpu_interview.h:4309 |
| `0x013C0` | `0x04F0` | MWV207REG_DE_DST_SCALE_FACTOR | DE | 1 |  | jmgpu_interview.h:4458 |
| `0x013C4` | `0x04F1` | MWV207REG_DE_DST_MEAN_FOR_CHANNEL_R | DE | 1 |  | jmgpu_interview.h:4477 |
| `0x013C8` | `0x04F2` | MWV207REG_DE_DST_MEAN_FOR_CHANNEL_G | DE | 1 |  | jmgpu_interview.h:4496 |
| `0x013CC` | `0x04F3` | MWV207REG_DE_DST_MEAN_FOR_CHANNEL_B | DE | 1 |  | jmgpu_interview.h:4515 |
| `0x013D0` | `0x04F4` | MWV207REG_DE_DST_STD_RECIPROCAL_FOR_CHANNEL_R | DE | 1 |  | jmgpu_interview.h:4534 |
| `0x013D4` | `0x04F5` | MWV207REG_DE_DST_STD_RECIPROCAL_FOR_CHANNEL_G | DE | 1 |  | jmgpu_interview.h:4553 |
| `0x013D8` | `0x04F6` | MWV207REG_DE_DST_STD_RECIPROCAL_FOR_CHANNEL_B | DE | 1 |  | jmgpu_interview.h:4572 |
| `0x013DC` | `0x04F7` | MWV207REG_DE_DST_NORMALIZATION_MODE | DE | 1 |  | jmgpu_interview.h:4590 |
| `0x013E0` | `0x04F8` | MWV207REG_DE_DST_QUANTIZATION_BYPASS | DE | 1 |  | jmgpu_interview.h:4611 |
| `0x013E4` | `0x04F9` | MWV207REG_DE_DST_STEP_RECIPROCAL | DE | 1 |  | jmgpu_interview.h:4631 |
| `0x013E8` | `0x04FA` | MWV207REG_DE_DST_MIN_VALUE_FOR_CHANNEL_R | DE | 1 |  | jmgpu_interview.h:4650 |
| `0x013EC` | `0x04FB` | MWV207REG_DE_DST_MIN_VALUE_FOR_CHANNEL_G | DE | 1 |  | jmgpu_interview.h:4669 |
| `0x013F0` | `0x04FC` | MWV207REG_DE_DST_MIN_VALUE_FOR_CHANNEL_B | DE | 1 |  | jmgpu_interview.h:4688 |
| `0x013F4` | `0x04FD` | MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_R | DE | 1 |  | jmgpu_interview.h:4707 |
| `0x013F8` | `0x04FE` | MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_G | DE | 1 |  | jmgpu_interview.h:4726 |
| `0x013FC` | `0x04FF` | MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_B | DE | 1 |  | jmgpu_interview.h:4745 |
| `0x01454` | `0x0515` | MWV207REG_PE_HIERARCHICAL_CONTROL | PE | 2 |  | jmgpu_toolkit.h:1283 |
| `0x01458` | `0x0516` | MWV207REG_PE_HIERARCHICAL_ADDRESS | PE | 2 |  | jmgpu_toolkit.h:1360 |
| `0x0145C` | `0x0517` | MWV207REG_PE_VAA_CONTROL | PE | 1 |  | jmgpu_toolkit.h:1386 |
| `0x01460` | `0x0518` | MWV207REG_PE_COLOR_BASE_ADDRESS | PE | 2 |  | jmgpu_toolkit.h:1406 |
| `0x01480` | `0x0520` | MWV207REG_PE_DEPTH_BASE_ADDRESS | PE | 2 |  | jmgpu_toolkit.h:1432 |
| `0x014A0` | `0x0528` | MWV207REG_PE_EXTRA_REFERENCE | PE | 4 |  | jmgpu_toolkit.h:1458 |
| `0x014A4` | `0x0529` | MWV207REG_PE_CONFIG | PE | 21 |  | jmgpu_toolkit.h:1666 |
| `0x014A8` | `0x052A` | MWV207REG_PE_DITHER_LOW | PE | 8 |  | jmgpu_toolkit.h:1499 |
| `0x014AC` | `0x052B` | MWV207REG_PE_DITHER_HIGH | PE | 8 |  | jmgpu_toolkit.h:1558 |
| `0x014B0` | `0x052C` | MWV207REG_PE_BLEND_FACTOR_LOW | PE | 2 |  | jmgpu_toolkit.h:1617 |
| `0x014B4` | `0x052D` | MWV207REG_PE_BLEND_FACTOR_HIGH | PE | 2 |  | jmgpu_toolkit.h:1640 |
| `0x014B8` | `0x052E` | MWV207REG_PE_BACKFACE_STENCIL | PE | 2 |  | jmgpu_toolkit.h:1903 |
| `0x014BC` | `0x052F` | MWV207REG_PE_CONFIG_EX | PE | 13 |  | jmgpu_toolkit.h:1929 |
| `0x014C0` | `0x0530` | MWV207REG_PE_ALPHA_CONFIG | PE | 15 |  | jmgpu_toolkit.h:2063 |
| `0x014C4` | `0x0531` | MWV207REG_DEPTH_BUFFER_END_ADDRESS | DEPTH | 1 |  | jmgpu_toolkit.h:2223 |
| `0x014C8` | `0x0532` | MWV207REG_PE_RENDER_TARGET_FORMAT | PE | 6 |  | jmgpu_toolkit.h:2239 |
| `0x014CC` | `0x0533` | MWV207REG_STENCIL_STRIDE | STENCIL | 1 |  | jmgpu_toolkit.h:2338 |
| `0x014D0` | `0x0534` | MWV207REG_STENCIL_BASE_ADDRESS | STENCIL | 2 |  | jmgpu_toolkit.h:2357 |
| `0x014D4` | `0x0535` | MWV207REG_STENCIL_BUFFER_END_ADDRESS | STENCIL | 1 |  | jmgpu_toolkit.h:2383 |
| `0x01500` | `0x0540` | MWV207REG_PE_RENDER_TARGET_ADDRESS | PE | 2 |  | jmgpu_toolkit.h:2403 |
| `0x01580` | `0x0560` | MWV207REG_PE_RENDER_TARGET_CONFIG | PE | 4 |  | jmgpu_toolkit.h:2429 |
| `0x01590` | `0x0564` | MWV207REG_PE_RENDER_TARGET_EXTRA_CONFIG | PE | 1 |  | jmgpu_toolkit.h:2516 |
| `0x015C0` | `0x0570` | MWV207REG_DE_ADJUST_CONTROL | DE | 3 |  | jmgpu_interview.h:7822 |
| `0x015C4` | `0x0571` | MWV207REG_DE_ADJUST_FACTOR1 | DE | 1 |  | jmgpu_interview.h:7865 |
| `0x015C8` | `0x0572` | MWV207REG_DE_ADJUST_FACTOR2 | DE | 1 |  | jmgpu_interview.h:7884 |
| `0x015CC` | `0x0573` | MWV207REG_DE_HIST_CONTROL | DE | 17 |  | jmgpu_interview.h:7903 |
| `0x015D0` | `0x0574` | MWV207REG_DE_HIST_EQUAL_MN | DE | 1 |  | jmgpu_interview.h:8047 |
| `0x015D4` | `0x0575` | MWV207REG_DE_HIST_SOBEL_MATRIX_GX0 | DE | 3 |  | jmgpu_interview.h:8066 |
| `0x015D8` | `0x0576` | MWV207REG_DE_HIST_SOBEL_MATRIX_GX1 | DE | 3 |  | jmgpu_interview.h:8095 |
| `0x015DC` | `0x0577` | MWV207REG_DE_HIST_SOBEL_MATRIX_GX2 | DE | 3 |  | jmgpu_interview.h:8124 |
| `0x015E0` | `0x0578` | MWV207REG_DE_HIST_SOBEL_MATRIX_GY0 | DE | 3 |  | jmgpu_interview.h:8153 |
| `0x015E4` | `0x0579` | MWV207REG_DE_HIST_SOBEL_MATRIX_GY1 | DE | 3 |  | jmgpu_interview.h:8182 |
| `0x015E8` | `0x057A` | MWV207REG_DE_HIST_SOBEL_MATRIX_GY2 | DE | 3 |  | jmgpu_interview.h:8211 |
| `0x015EC` | `0x057B` | MWV207REG_HIST_BASE_ADDRESS0 | HIST | 2 |  | jmgpu_interview.h:8240 |
| `0x015F0` | `0x057C` | MWV207REG_HIST_BASE_ADDRESS1 | HIST | 2 |  | jmgpu_interview.h:8266 |
| `0x015F4` | `0x057D` | MWV207REG_HIST_BASE_ADDRESS2 | HIST | 2 |  | jmgpu_interview.h:8292 |
| `0x016A0` | `0x05A8` | MWV207REG_RS_CONFIG | RS | 14 |  | jmgpu_multiuser.h:1306 |
| `0x016A4` | `0x05A9` | MWV207REG_MC_CACHE_HZ | MC | 2 |  | jmgpu_multiuser.h:1456 |
| `0x016A8` | `0x05AA` | MWV207REG_MC_CLEAR_VALUE_HZ | MC | 1 |  | jmgpu_multiuser.h:1482 |
| `0x016AC` | `0x05AB` | MWV207REG_MC_COUNTER_HZ | MC | 1 |  | jmgpu_multiuser.h:1500 |
| `0x016B0` | `0x05AC` | MWV207REG_RS_TILE_FILLER_TRIGGER | RS | 1 |  | jmgpu_multiuser.h:1792 |
| `0x016B4` | `0x05AD` | MWV207REG_RS_INTERLEAVE_ADDRESS | RS | 1 |  | jmgpu_multiuser.h:1812 |
| `0x016B8` | `0x05AE` | MWV207REG_RS_CONTROL | RS | 2 |  | jmgpu_multiuser.h:1831 |
| `0x016BC` | `0x05AF` | MWV207REG_MC_COLOR_CLEAR64 | MC | 1 |  | jmgpu_multiuser.h:1865 |
| `0x016C0` | `0x05B0` | MWV207REG_RS_SOURCE_ADDRESS | RS | 2 |  | jmgpu_multiuser.h:1518 |
| `0x016E0` | `0x05B8` | MWV207REG_RS_TARGET_ADDRESS | RS | 2 |  | jmgpu_multiuser.h:1543 |
| `0x01700` | `0x05C0` | MWV207REG_RS_OFFSET | RS | 2 |  | jmgpu_multiuser.h:1569 |
| `0x01720` | `0x05C8` | MWV207REG_MC_TEXTURE_CONFIG | MC | 15 |  | jmgpu_multiuser.h:1595 |
| `0x01740` | `0x05D0` | MWV207REG_MC_TEXTURE_BUFFER | MC | 1 |  | jmgpu_multiuser.h:1753 |
| `0x01760` | `0x05D8` | MWV207REG_MC_TEXTURE_CLEAR | MC | 1 |  | jmgpu_multiuser.h:1773 |
| `0x01780` | `0x05E0` | MWV207REG_MC_TEXTURE_CLEAR64 | MC | 1 |  | jmgpu_multiuser.h:1884 |
| `0x017A0` | `0x05E8` | MWV207REG_MRT_MEMORY_CONFIG | MRT | 9 |  | jmgpu_multiuser.h:1903 |
| `0x017C0` | `0x05F0` | MWV207REG_MRT_COLOR_CACHE | MRT | 2 |  | jmgpu_multiuser.h:2002 |
| `0x017E0` | `0x05F8` | MWV207REG_MRT_COLOR_BASE | MRT | 2 |  | jmgpu_multiuser.h:2028 |
| `0x01A00` | `0x0680` | MWV207REG_MRT_COLOR_CLEAR_VALUE | MRT | 1 |  | jmgpu_multiuser.h:2055 |
| `0x01A20` | `0x0688` | MWV207REG_MRTMC_COLOR_CLEAR64 | MRTMC | 1 |  | jmgpu_multiuser.h:2074 |
| `0x01A40` | `0x0690` | MWV207REG_MRT_TILE_STATUS_COUNTER_C | MRT | 1 |  | jmgpu_multiuser.h:2092 |
| `0x01A60` | `0x0698` | MWV207REG_RS_SRC_SURFACE_SIZE | RS | 2 |  | jmgpu_multiuser.h:2111 |
| `0x01A64` | `0x0699` | MWV207REG_RS_SRC_ROTATION_CONFIG | RS | 6 |  | jmgpu_multiuser.h:2137 |
| `0x01A68` | `0x069A` | MWV207REG_RS_SRC_BUFFER_END_ADDRESS | RS | 1 |  | jmgpu_multiuser.h:2385 |
| `0x01A6C` | `0x069B` | MWV207REG_RS_SRC_UBUFFER_END_ADDRESS | RS | 1 |  | jmgpu_multiuser.h:2404 |
| `0x01A70` | `0x069C` | MWV207REG_MC_SHADER_CONFIG | MC | 7 |  | jmgpu_multiuser.h:2217 |
| `0x01A80` | `0x06A0` | MWV207REG_MC_TEXTURE_BASE_BUFFER | MC | 1 |  | jmgpu_multiuser.h:2197 |
| `0x01AA0` | `0x06A8` | MWV207REG_MC_SHADER_BUFFER | MC | 1 |  | jmgpu_multiuser.h:2305 |
| `0x01AB0` | `0x06AC` | MWV207REG_MC_SHADER_CLEAR | MC | 1 |  | jmgpu_multiuser.h:2325 |
| `0x01AC0` | `0x06B0` | MWV207REG_MC_SHADER_CLEAR64 | MC | 1 |  | jmgpu_multiuser.h:2345 |
| `0x01AD0` | `0x06B4` | MWV207REG_MC_SHADER_BASE_BUFFER | MC | 1 |  | jmgpu_multiuser.h:2365 |
| `0x01AE0` | `0x06B8` | MWV207REG_RS_SRC_VBUFFER_END_ADDRESS | RS | 1 |  | jmgpu_multiuser.h:2423 |
| `0x01AE4` | `0x06B9` | MWV207REG_RS_DST_BUFFER_END_ADDRESS | RS | 1 |  | jmgpu_multiuser.h:2442 |
| `0x01AE8` | `0x06BA` | MWV207REG_RS_DST_CONFIG_EX | RS | 2 |  | jmgpu_multiuser.h:2458 |
| `0x01AEC` | `0x06BB` | MWV207REG_RS_DST_ADDRESS1 | RS | 2 |  | jmgpu_multiuser.h:2490 |
| `0x01AF0` | `0x06BC` | MWV207REG_RS_DST_STRIDE1 | RS | 1 |  | jmgpu_multiuser.h:2513 |
| `0x01AF4` | `0x06BD` | MWV207REG_RS_DST_ADDRESS2 | RS | 2 |  | jmgpu_multiuser.h:2533 |
| `0x01AF8` | `0x06BE` | MWV207REG_RS_DST_STRIDE2 | RS | 1 |  | jmgpu_multiuser.h:2556 |
| `0x01B00` | `0x06C0` | MWV207REG_MRT_COLOR_CLEAR_VALUE96 | MRT | 1 |  | jmgpu_multiuser.h:2605 |
| `0x01B20` | `0x06C8` | MWV207REG_MRT_COLOR_CLEAR_VALUE128 | MRT | 1 |  | jmgpu_multiuser.h:2624 |
| `0x01B50` | `0x06D4` | MWV207REG_MC_SHADER_CLEAR96 | MC | 1 |  | jmgpu_multiuser.h:2662 |
| `0x01B60` | `0x06D8` | MWV207REG_MC_SHADER_CLEAR128 | MC | 1 |  | jmgpu_multiuser.h:2682 |
| `0x01B70` | `0x06DC` | MWV207REG_MC_TEXTURE_CLEAR96 | MC | 1 |  | jmgpu_multiuser.h:2702 |
| `0x01B80` | `0x06E0` | MWV207REG_MC_TEXTURE_CLEAR128 | MC | 1 |  | jmgpu_multiuser.h:2722 |
| `0x02140` | `0x0850` | MWV207REG_TX_STRIDE | TX | 1 |  | jmgpu_cursor.h:340 |
| `0x02180` | `0x0860` | MWV207REG_TX_VOLUME | TX | 3 |  | jmgpu_cursor.h:360 |
| `0x021C0` | `0x0870` | MWV207REG_TX_MODE | TX | 17 |  | jmgpu_cursor.h:396 |
| `0x02200` | `0x0880` | MWV207REG_TX_CONTROL_YUV | TX | 15 |  | jmgpu_cursor.h:633 |
| `0x02240` | `0x0890` | MWV207REG_TX_STRIDE_YUV | TX | 2 |  | jmgpu_cursor.h:782 |
| `0x02280` | `0x08A0` | MWV207REG_TX_ASTC0 | TX | 8 |  | jmgpu_cursor.h:808 |
| `0x022C0` | `0x08B0` | MWV207REG_TX_ASTC1 | TX | 8 |  | jmgpu_cursor.h:934 |
| `0x02300` | `0x08C0` | MWV207REG_TX_ASTC2 | TX | 8 |  | jmgpu_cursor.h:1060 |
| `0x02340` | `0x08D0` | MWV207REG_TX_ASTC3 | TX | 8 |  | jmgpu_cursor.h:1186 |
| `0x02400` | `0x0900` | MWV207REG_MCFE_STD_DESC_RING_BUF_START_ADDR | MCFE | 1 |  | jmgpu_deallocate.h:28 |
| `0x02500` | `0x0940` | MWV207REG_MCFE_STD_DESC_FIFO_DEPTH_EXP | MCFE | 1 |  | jmgpu_deallocate.h:46 |
| `0x02600` | `0x0980` | MWV207REG_MCFE_STD_DESC_FIFO_WR_PTR | MCFE | 1 |  | jmgpu_deallocate.h:65 |
| `0x02700` | `0x09C0` | MWV207REG_MCFE_STD_DESC_FIFO_RD_PTR | MCFE | 1 |  | jmgpu_deallocate.h:83 |
| `0x02800` | `0x0A00` | MWV207REG_MCFE_PRI_DESC_RING_BUF_START_ADDR | MCFE | 1 |  | jmgpu_deallocate.h:101 |
| `0x02900` | `0x0A40` | MWV207REG_MCFE_PRI_DESC_FIFO_DEPTH_EXP | MCFE | 1 |  | jmgpu_deallocate.h:119 |
| `0x02A00` | `0x0A80` | MWV207REG_MCFE_PRI_DESC_FIFO_WR_PTR | MCFE | 1 |  | jmgpu_deallocate.h:138 |
| `0x02B00` | `0x0AC0` | MWV207REG_MCFE_PRI_DESC_FIFO_RD_PTR | MCFE | 1 |  | jmgpu_deallocate.h:156 |
| `0x02C00` | `0x0B00` | MWV207REG_AHBTT_SYNC | AHBTT | 1 |  | jmgpu_ahbtransform.h:28 |
| `0x02C00` | `0x0B00` | MWV207REG_TX_SAMPLER_STRIDE | TX | 2 |  | jmgpu_cursor.h:1713 |
| `0x02C04` | `0x0B01` | MWV207REG_AHBTTD_MISC | AHBTTD | 7 |  | jmgpu_ahbtransform.h:47 |
| `0x02C08` | `0x0B02` | MWV207REG_AHBVIP_TENSOR_IO | AHBVIP | 1 |  | jmgpu_ahbtransform.h:95 |
| `0x02C0C` | `0x0B03` | MWV207REG_AHBVIP_TENSOR_IO_WR | AHBVIP | 1 |  | jmgpu_ahbtransform.h:113 |
| `0x02C10` | `0x0B04` | MWV207REG_AHBVIPDMA_RAW_STATUS | AHBVIPDMA | 3 |  | jmgpu_ahbtransform.h:132 |
| `0x02C14` | `0x0B05` | MWV207REG_AHBVIPDMA_INTR_EN | AHBVIPDMA | 3 |  | jmgpu_ahbtransform.h:160 |
| `0x02C18` | `0x0B06` | MWV207REG_AHBVIPDMA_INTR_STATUS | AHBVIPDMA | 3 |  | jmgpu_ahbtransform.h:188 |
| `0x02C1C` | `0x0B07` | MWV207REG_AHBVIP_CLOCK_GATE | AHBVIP | 28 |  | jmgpu_ahbtransform.h:216 |
| `0x02C20` | `0x0B08` | MWV207REG_AHBVIP_DEBUG | AHBVIP | 1 |  | jmgpu_ahbtransform.h:394 |
| `0x02C40` | `0x0B10` | MWV207REG_AHBXTTD_MISC | AHBXTTD | 19 |  | jmgpu_ahbtransform.h:414 |
| `0x02C44` | `0x0B11` | MWV207REG_AHBX_TENSOR_IO | AHBX | 1 |  | jmgpu_ahbtransform.h:531 |
| `0x02C48` | `0x0B12` | MWV207REG_AHBX_TENSOR_IO_WR | AHBX | 1 |  | jmgpu_ahbtransform.h:549 |
| `0x02C4C` | `0x0B13` | MWV207REG_AHB_SBI_IN_PUREQ_TO_PUREQ_MAX_CYCLE | AHB | 1 |  | jmgpu_ahbtransform.h:567 |
| `0x02C50` | `0x0B14` | MWV207REG_AHB_SBI_IN_PUREQ_TO_PUREQ_TIMEOUT | AHB | 1 |  | jmgpu_ahbtransform.h:585 |
| `0x02C54` | `0x0B15` | MWV207REG_AHB_SBI_OUT_CUREQ_TO_CUREQ_MAX_CYCLE | AHB | 1 |  | jmgpu_ahbtransform.h:603 |
| `0x02C58` | `0x0B16` | MWV207REG_AHB_SBI_OUT_CUREQ_TO_CUREQ_TIMEOUT | AHB | 1 |  | jmgpu_ahbtransform.h:621 |
| `0x02C5C` | `0x0B17` | MWV207REG_AHB_SBI_IN_PRREQ_TO_PRACK_MAX_CYCLE | AHB | 1 |  | jmgpu_ahbtransform.h:639 |
| `0x02C60` | `0x0B18` | MWV207REG_AHB_SBI_IN_PRREQ_TO_PRACK_TIMEOUT | AHB | 1 |  | jmgpu_ahbtransform.h:657 |
| `0x02C64` | `0x0B19` | MWV207REG_AHB_SBI_OUT_CRREQ_TO_CRACK_MAX_CYCLE | AHB | 1 |  | jmgpu_ahbtransform.h:675 |
| `0x02C68` | `0x0B1A` | MWV207REG_AHB_SBI_OUT_CRREQ_TO_CRACK_TIMEOUT | AHB | 1 |  | jmgpu_ahbtransform.h:693 |
| `0x02C6C` | `0x0B1B` | MWV207REG_AHB_OCM_REMAP_START_ADDRESS | AHB | 1 |  | jmgpu_ahbtransform.h:711 |
| `0x02C70` | `0x0B1C` | MWV207REG_AHB_OCM_REMAP_END_ADDRESS | AHB | 1 |  | jmgpu_ahbtransform.h:729 |
| `0x02C74` | `0x0B1D` | MWV207REG_AHBSW_CONTROL_VXSBI0 | AHBSW | 4 |  | jmgpu_ahbtransform.h:748 |
| `0x02C78` | `0x0B1E` | MWV207REG_AHBSW_CONTROL_VXSBI1 | AHBSW | 4 |  | jmgpu_ahbtransform.h:790 |
| `0x02C7C` | `0x0B1F` | MWV207REG_AHBSW_CONTROL_VXSBI2 | AHBSW | 4 |  | jmgpu_ahbtransform.h:828 |
| `0x02C80` | `0x0B20` | MWV207REG_AHB_PTN_GEN_BAYER_IMAGE | AHB | 2 |  | jmgpu_ahbtransform.h:866 |
| `0x02C84` | `0x0B21` | MWV207REG_AHB_PTN_GEN_RED | AHB | 2 |  | jmgpu_ahbtransform.h:889 |
| `0x02C88` | `0x0B22` | MWV207REG_AHB_PTN_GEN_GREENS | AHB | 2 |  | jmgpu_ahbtransform.h:912 |
| `0x02C8C` | `0x0B23` | MWV207REG_AHB_PTN_GEN_BLUE | AHB | 2 |  | jmgpu_ahbtransform.h:935 |
| `0x02C90` | `0x0B24` | MWV207REG_AHB_PTN_GEN_INC_Y0 | AHB | 2 |  | jmgpu_ahbtransform.h:961 |
| `0x02C94` | `0x0B25` | MWV207REG_AHB_PTN_GEN_INC_Y1 | AHB | 1 |  | jmgpu_ahbtransform.h:983 |
| `0x02C98` | `0x0B26` | MWV207REG_AHB_CHECKSUM_ACTUAL_CHECKSUM_RED | AHB | 1 |  | jmgpu_ahbtransform.h:1003 |
| `0x02C9C` | `0x0B27` | MWV207REG_AHB_CHECKSUM_ACTUAL_CHECKSUM_GREEN | AHB | 1 |  | jmgpu_ahbtransform.h:1022 |
| `0x02CA0` | `0x0B28` | MWV207REG_AHB_CHECKSUM_ACTUAL_CHECKSUM_BLUE | AHB | 1 |  | jmgpu_ahbtransform.h:1041 |
| `0x02CA4` | `0x0B29` | MWV207REG_AHB_CHECKSUM_EXPECTED_CHECKSUM_RED | AHB | 1 |  | jmgpu_ahbtransform.h:1060 |
| `0x02CA8` | `0x0B2A` | MWV207REG_AHB_CHECKSUM_EXPECTED_CHECKSUM_GREEN | AHB | 1 |  | jmgpu_ahbtransform.h:1079 |
| `0x02CAC` | `0x0B2B` | MWV207REG_AHB_CHECKSUM_EXPECTED_CHECKSUM_BLUE | AHB | 1 |  | jmgpu_ahbtransform.h:1098 |
| `0x02CB0` | `0x0B2C` | MWV207REG_AHB_PATTERNS | AHB | 6 |  | jmgpu_ahbtransform.h:1117 |
| `0x02D44` | `0x0B51` | MWV207REG_AHBXDMA_RAW_STATUS | AHBXDMA | 27 |  | jmgpu_ahbtransform.h:2349 |
| `0x02D48` | `0x0B52` | MWV207REG_AHBXDMA_RAW_STATUS2 | AHBXDMA | 18 |  | jmgpu_ahbtransform.h:2496 |
| `0x02D4C` | `0x0B53` | MWV207REG_AHBXDMA_INTR_EN | AHBXDMA | 27 |  | jmgpu_ahbtransform.h:2599 |
| `0x02D50` | `0x0B54` | MWV207REG_AHBXDMA_INTR_EN2 | AHBXDMA | 18 |  | jmgpu_ahbtransform.h:2745 |
| `0x02D54` | `0x0B55` | MWV207REG_AHBXDMA_INTR_STATUS | AHBXDMA | 27 |  | jmgpu_ahbtransform.h:2866 |
| `0x02D58` | `0x0B56` | MWV207REG_AHBXDMA_INTR_STATUS2 | AHBXDMA | 18 |  | jmgpu_ahbtransform.h:3016 |
| `0x02D5C` | `0x0B57` | MWV207REG_AHB_COMPRESSOR | AHB | 6 |  | jmgpu_ahbtransform.h:3138 |
| `0x02D60` | `0x0B58` | MWV207REG_AHB_DECOMPRESSOR | AHB | 6 |  | jmgpu_ahbtransform.h:3196 |
| `0x02D64` | `0x0B59` | MWV207REG_AHB_DENOISE_POST_PROCESS_C0 | AHB | 1 |  | jmgpu_ahbtransform.h:3254 |
| `0x02D68` | `0x0B5A` | MWV207REG_AHB_DENOISE_POST_PROCESS_C1 | AHB | 1 |  | jmgpu_ahbtransform.h:3273 |
| `0x02D6C` | `0x0B5B` | MWV207REG_AHB_DENOISE_POST_PROCESS_C2 | AHB | 1 |  | jmgpu_ahbtransform.h:3292 |
| `0x02D70` | `0x0B5C` | MWV207REG_AHB_DENOISE_POST_PROCESS_C3 | AHB | 1 |  | jmgpu_ahbtransform.h:3311 |
| `0x02D74` | `0x0B5D` | MWV207REG_AHB_DENOISE_POST_PROCESS_NN_OUT_SCALE | AHB | 1 |  | jmgpu_ahbtransform.h:3329 |
| `0x02D78` | `0x0B5E` | MWV207REG_AHB_DENOISE_POST_PROCESS_CLAMP_MIN_MAX | AHB | 2 |  | jmgpu_ahbtransform.h:3347 |
| `0x02D7C` | `0x0B5F` | MWV207REG_AHBX_AXI_QOS0 | AHBX | 8 |  | jmgpu_ahbtransform.h:3370 |
| `0x02D80` | `0x0B60` | MWV207REG_AHBX_AXI_QOS1 | AHBX | 8 |  | jmgpu_ahbtransform.h:3423 |
| `0x02D84` | `0x0B61` | MWV207REG_AHBX_AXI_QOS2 | AHBX | 8 |  | jmgpu_ahbtransform.h:3476 |
| `0x02D88` | `0x0B62` | MWV207REG_AHBX_AXI_QOS3 | AHBX | 8 |  | jmgpu_ahbtransform.h:3529 |
| `0x02D8C` | `0x0B63` | MWV207REG_AHBX_DEEP_SLEEP0 | AHBX | 13 |  | jmgpu_ahbtransform.h:3580 |
| `0x02D90` | `0x0B64` | MWV207REG_AHBX_DEEP_SLEEP1 | AHBX | 12 |  | jmgpu_ahbtransform.h:3710 |
| `0x02D94` | `0x0B65` | MWV207REG_AHBX_DEEP_SLEEP2 | AHBX | 8 |  | jmgpu_ahbtransform.h:3837 |
| `0x02D98` | `0x0B66` | MWV207REG_AHBX_CACHE | AHBX | 2 |  | jmgpu_ahbtransform.h:3896 |
| `0x02D9C` | `0x0B67` | MWV207REG_AHBXVIP_DEBUG | AHBXVIP | 1 |  | jmgpu_ahbtransform.h:3922 |
| `0x02DA0` | `0x0B68` | MWV207REG_AHBXTTD_MISC2 | AHBXTTD | 6 |  | jmgpu_ahbtransform.h:3940 |
| `0x02DA4` | `0x0B69` | MWV207REG_AHB_COMP_DECOMP | AHB | 2 |  | jmgpu_ahbtransform.h:3984 |
| `0x03000` | `0x0C00` | MWV207REG_CE_ORIGIN | CE | 2 |  | jmgpu_memory.h:29 |
| `0x03004` | `0x0C01` | MWV207REG_CE_SIZE | CE | 2 |  | jmgpu_memory.h:55 |
| `0x03008` | `0x0C02` | MWV207REG_CE_STATE_MEMORY | CE | 1 |  | jmgpu_memory.h:81 |
| `0x0300C` | `0x0C03` | MWV207REG_CE_EXECUTE | CE | 6 |  | jmgpu_memory.h:100 |
| `0x03010` | `0x0C04` | MWV207REG_CE_PROGRAM | CE | 2 |  | jmgpu_memory.h:160 |
| `0x03014` | `0x0C05` | MWV207REG_CE_CONFIG | CE | 5 |  | jmgpu_memory.h:186 |
| `0x03018` | `0x0C06` | MWV207REG_CE_COUNT | CE | 4 |  | jmgpu_memory.h:315 |
| `0x0301C` | `0x0C07` | MWV207REG_CE_SWIZZLE | CE | 5 |  | jmgpu_memory.h:472 |
| `0x03020` | `0x0C08` | MWV207REG_CE_ADDRESS | CE | 1 |  | jmgpu_memory.h:295 |
| `0x03040` | `0x0C10` | MWV207REG_CE_DITHER_LOW | CE | 8 |  | jmgpu_memory.h:351 |
| `0x03044` | `0x0C11` | MWV207REG_CE_DITHER_HIGH | CE | 8 |  | jmgpu_memory.h:410 |
| `0x03048` | `0x0C12` | MWV207REG_CE_CONST | CE | 1 |  | jmgpu_memory.h:536 |
| `0x0304C` | `0x0C13` | MWV207REG_CE_INSTRUCTION | CE | 1 |  | jmgpu_memory.h:555 |
| `0x03060` | `0x0C18` | MWV207REG_CE_ATTRIBUTE_X | CE | 1 |  | jmgpu_memory.h:576 |
| `0x03080` | `0x0C20` | MWV207REG_CE_ATTRIBUTE_XDX | CE | 1 |  | jmgpu_memory.h:595 |
| `0x030A0` | `0x0C28` | MWV207REG_CE_ATTRIBUTE_XDY | CE | 1 |  | jmgpu_memory.h:614 |
| `0x030C0` | `0x0C30` | MWV207REG_CE_ATTRIBUTE_Y | CE | 1 |  | jmgpu_memory.h:634 |
| `0x030E0` | `0x0C38` | MWV207REG_CE_ATTRIBUTE_YDX | CE | 1 |  | jmgpu_memory.h:653 |
| `0x03100` | `0x0C40` | MWV207REG_CE_ATTRIBUTE_YDY | CE | 1 |  | jmgpu_memory.h:672 |
| `0x03120` | `0x0C48` | MWV207REG_CE_SAMPLER_CONFIG | CE | 6 |  | jmgpu_memory.h:692 |
| `0x03140` | `0x0C50` | MWV207REG_CE_SAMPLER_SIZE | CE | 4 |  | jmgpu_memory.h:819 |
| `0x03160` | `0x0C58` | MWV207REG_CE_BORDER_X | CE | 3 |  | jmgpu_memory.h:884 |
| `0x03180` | `0x0C60` | MWV207REG_CE_BORDER_Y | CE | 2 |  | jmgpu_memory.h:919 |
| `0x031A0` | `0x0C68` | MWV207REG_CE_TILE_STATUS_CONTROL | CE | 2 |  | jmgpu_memory.h:945 |
| `0x031C0` | `0x0C70` | MWV207REG_CE_TILE_STATUS_BUFFER | CE | 1 |  | jmgpu_memory.h:985 |
| `0x031E0` | `0x0C78` | MWV207REG_CE_TILE_STATUS_CLEAR | CE | 1 |  | jmgpu_memory.h:1004 |
| `0x03200` | `0x0C80` | MWV207REG_AHBX_TOTAL_RD_CPU | AHBX | 1 |  | jmgpu_ahbtransform.h:4009 |
| `0x03200` | `0x0C80` | MWV207REG_CE_SAMPLER_ADDRESS | CE | 1 |  | jmgpu_memory.h:865 |
| `0x03204` | `0x0C81` | MWV207REG_AHBX_TOTAL_WR_CPU | AHBX | 1 |  | jmgpu_ahbtransform.h:4028 |
| `0x03208` | `0x0C82` | MWV207REG_AHBX_TOTAL_WR_BURST_CPU | AHBX | 1 |  | jmgpu_ahbtransform.h:4047 |
| `0x0320C` | `0x0C83` | MWV207REG_AHBX_TOTAL_WR_REQ_CPU | AHBX | 1 |  | jmgpu_ahbtransform.h:4066 |
| `0x03210` | `0x0C84` | MWV207REG_AHBX_TOTAL_WR_LAST_CPU | AHBX | 1 |  | jmgpu_ahbtransform.h:4085 |
| `0x03214` | `0x0C85` | MWV207REG_AHBX_TOTAL_RD_BURST_CPU | AHBX | 1 |  | jmgpu_ahbtransform.h:4104 |
| `0x03218` | `0x0C86` | MWV207REG_AHBX_TOTAL_RD_REQ_CPU | AHBX | 1 |  | jmgpu_ahbtransform.h:4123 |
| `0x0321C` | `0x0C87` | MWV207REG_AHBX_TOTAL_RD_LAST_CPU | AHBX | 1 |  | jmgpu_ahbtransform.h:4142 |
| `0x03220` | `0x0C88` | MWV207REG_AHBX_TOTAL_RD_ISP | AHBX | 1 |  | jmgpu_ahbtransform.h:4161 |
| `0x03224` | `0x0C89` | MWV207REG_AHBX_TOTAL_WR_ISP | AHBX | 1 |  | jmgpu_ahbtransform.h:4180 |
| `0x03228` | `0x0C8A` | MWV207REG_AHBX_TOTAL_WR_BURST_ISP | AHBX | 1 |  | jmgpu_ahbtransform.h:4199 |
| `0x0322C` | `0x0C8B` | MWV207REG_AHBX_TOTAL_WR_REQ_ISP | AHBX | 1 |  | jmgpu_ahbtransform.h:4218 |
| `0x03230` | `0x0C8C` | MWV207REG_AHBX_TOTAL_WR_LAST_ISP | AHBX | 1 |  | jmgpu_ahbtransform.h:4237 |
| `0x03234` | `0x0C8D` | MWV207REG_AHBX_TOTAL_RD_BURST_ISP | AHBX | 1 |  | jmgpu_ahbtransform.h:4256 |
| `0x03238` | `0x0C8E` | MWV207REG_AHBX_TOTAL_RD_REQ_ISP | AHBX | 1 |  | jmgpu_ahbtransform.h:4275 |
| `0x0323C` | `0x0C8F` | MWV207REG_AHBX_TOTAL_RD_LAST_ISP | AHBX | 1 |  | jmgpu_ahbtransform.h:4294 |
| `0x03240` | `0x0C90` | MWV207REG_AHBX_TOTAL_RD_DDR | AHBX | 1 |  | jmgpu_ahbtransform.h:4313 |
| `0x03244` | `0x0C91` | MWV207REG_AHBX_TOTAL_WR_DDR | AHBX | 1 |  | jmgpu_ahbtransform.h:4332 |
| `0x03248` | `0x0C92` | MWV207REG_AHBX_TOTAL_WR_BURST_DDR | AHBX | 1 |  | jmgpu_ahbtransform.h:4351 |
| `0x0324C` | `0x0C93` | MWV207REG_AHBX_TOTAL_WR_REQ_DDR | AHBX | 1 |  | jmgpu_ahbtransform.h:4370 |
| `0x03250` | `0x0C94` | MWV207REG_AHBX_TOTAL_WR_LAST_DDR | AHBX | 1 |  | jmgpu_ahbtransform.h:4389 |
| `0x03254` | `0x0C95` | MWV207REG_AHBX_TOTAL_RD_BURST_DDR | AHBX | 1 |  | jmgpu_ahbtransform.h:4408 |
| `0x03258` | `0x0C96` | MWV207REG_AHBX_TOTAL_RD_REQ_DDR | AHBX | 1 |  | jmgpu_ahbtransform.h:4427 |
| `0x0325C` | `0x0C97` | MWV207REG_AHBX_TOTAL_RD_LAST_DDR | AHBX | 1 |  | jmgpu_ahbtransform.h:4446 |
| `0x03260` | `0x0C98` | MWV207REG_AHBX_MEM_COUNT | AHBX | 1 |  | jmgpu_ahbtransform.h:4462 |
| `0x03264` | `0x0C99` | MWV207REG_AHBXDDR_RD_CK_TOTAL | AHBXDDR | 1 |  | jmgpu_ahbtransform.h:4478 |
| `0x03268` | `0x0C9A` | MWV207REG_AHBXDDR_WR_CK_TOTAL | AHBXDDR | 1 |  | jmgpu_ahbtransform.h:4494 |
| `0x0326C` | `0x0C9B` | MWV207REG_AHBX_PR_ACK | AHBX | 1 |  | jmgpu_ahbtransform.h:4510 |
| `0x03270` | `0x0C9C` | MWV207REG_AHBX_PU_ACK | AHBX | 1 |  | jmgpu_ahbtransform.h:4526 |
| `0x03274` | `0x0C9D` | MWV207REG_AHBX_CR_ACK | AHBX | 1 |  | jmgpu_ahbtransform.h:4542 |
| `0x03278` | `0x0C9E` | MWV207REG_AHBX_CU_ACK | AHBX | 1 |  | jmgpu_ahbtransform.h:4558 |
| `0x0327C` | `0x0C9F` | MWV207REG_AHBX_IN_FRM_START | AHBX | 1 |  | jmgpu_ahbtransform.h:4574 |
| `0x03280` | `0x0CA0` | MWV207REG_AHBX_OUT_FRM_START | AHBX | 1 |  | jmgpu_ahbtransform.h:4590 |
| `0x03284` | `0x0CA1` | MWV207REG_AHBX_FLEXA_PSI0 | AHBX | 1 |  | jmgpu_ahbtransform.h:4606 |
| `0x03288` | `0x0CA2` | MWV207REG_AHBX_FLEXA_PSI1 | AHBX | 1 |  | jmgpu_ahbtransform.h:4622 |
| `0x0328C` | `0x0CA3` | MWV207REG_AHBX_FLEXA_PSI2 | AHBX | 1 |  | jmgpu_ahbtransform.h:4638 |
| `0x03290` | `0x0CA4` | MWV207REG_AHBX_FLEXA_PSI3 | AHBX | 1 |  | jmgpu_ahbtransform.h:4654 |
| `0x03294` | `0x0CA5` | MWV207REG_AHBXVIP_SRAM_DEEP_SLEEP | AHBXVIP | 1 |  | jmgpu_ahbtransform.h:4670 |
| `0x03298` | `0x0CA6` | MWV207REG_AHBX_OCM_DEEP_SLEEP | AHBX | 1 |  | jmgpu_ahbtransform.h:4686 |
| `0x0329C` | `0x0CA7` | MWV207REG_AHBXVIP_WORK_DONE | AHBXVIP | 1 |  | jmgpu_ahbtransform.h:4702 |
| `0x032A0` | `0x0CA8` | MWV207REG_AHBXVIP0_TOTAL_WR_REQ | AHBXVIP0 | 1 |  | jmgpu_ahbtransform.h:4718 |
| `0x032A4` | `0x0CA9` | MWV207REG_AHBXVIP0_TOTAL_RD_REQ | AHBXVIP0 | 1 |  | jmgpu_ahbtransform.h:4734 |
| `0x032A8` | `0x0CAA` | MWV207REG_AHBXVIP1_TOTAL_WR_REQ | AHBXVIP1 | 1 |  | jmgpu_ahbtransform.h:4750 |
| `0x032AC` | `0x0CAB` | MWV207REG_AHBXVIP1_TOTAL_RD_REQ | AHBXVIP1 | 1 |  | jmgpu_ahbtransform.h:4766 |
| `0x032B0` | `0x0CAC` | MWV207REG_AHBXVIP2_TOTAL_WR_REQ | AHBXVIP2 | 1 |  | jmgpu_ahbtransform.h:4782 |
| `0x032B4` | `0x0CAD` | MWV207REG_AHBXVIP2_TOTAL_RD_REQ | AHBXVIP2 | 1 |  | jmgpu_ahbtransform.h:4798 |
| `0x032B8` | `0x0CAE` | MWV207REG_AHBXVIP3_TOTAL_WR_REQ | AHBXVIP3 | 1 |  | jmgpu_ahbtransform.h:4814 |
| `0x032BC` | `0x0CAF` | MWV207REG_AHBXVIP3_TOTAL_RD_REQ | AHBXVIP3 | 1 |  | jmgpu_ahbtransform.h:4830 |
| `0x032C0` | `0x0CB0` | MWV207REG_AHBXDMA0_TOTAL_WR_REQ | AHBXDMA0 | 1 |  | jmgpu_ahbtransform.h:4846 |
| `0x032C4` | `0x0CB1` | MWV207REG_AHBXDMA0_TOTAL_RD_REQ | AHBXDMA0 | 1 |  | jmgpu_ahbtransform.h:4862 |
| `0x032C8` | `0x0CB2` | MWV207REG_AHBXDMA2_TOTAL_WR_REQ | AHBXDMA2 | 1 |  | jmgpu_ahbtransform.h:4878 |
| `0x032CC` | `0x0CB3` | MWV207REG_AHBXDMA2_TOTAL_RD_REQ | AHBXDMA2 | 1 |  | jmgpu_ahbtransform.h:4894 |
| `0x032D0` | `0x0CB4` | MWV207REG_AHBX_OCM_TOTAL_WR_REQ | AHBX | 1 |  | jmgpu_ahbtransform.h:4910 |
| `0x032D4` | `0x0CB5` | MWV207REG_AHBX_OCM_TOTAL_RD_REQ | AHBX | 1 |  | jmgpu_ahbtransform.h:4926 |
| `0x032D8` | `0x0CB6` | MWV207REG_AHBX_SBI_TO_PSI_DBG0 | AHBX | 1 |  | jmgpu_ahbtransform.h:4942 |
| `0x032DC` | `0x0CB7` | MWV207REG_AHBX_SBI_TO_PSI_DBG1 | AHBX | 1 |  | jmgpu_ahbtransform.h:4958 |
| `0x032E0` | `0x0CB8` | MWV207REG_AHBX_SBI_TO_PSI_DBG2 | AHBX | 1 |  | jmgpu_ahbtransform.h:4974 |
| `0x032E4` | `0x0CB9` | MWV207REG_AHBX_PSI_TO_SBI_DBG0 | AHBX | 1 |  | jmgpu_ahbtransform.h:4990 |
| `0x032E8` | `0x0CBA` | MWV207REG_AHBX_PSI_TO_SBI_DBG1 | AHBX | 1 |  | jmgpu_ahbtransform.h:5006 |
| `0x032EC` | `0x0CBB` | MWV207REG_AHBX_PSI_TO_SBI_DBG2 | AHBX | 1 |  | jmgpu_ahbtransform.h:5022 |
| `0x032F0` | `0x0CBC` | MWV207REG_AHBX_TRANSMITTER_DBG0 | AHBX | 1 |  | jmgpu_ahbtransform.h:5038 |
| `0x032F4` | `0x0CBD` | MWV207REG_AHBX_TRANSMITTER_DBG1 | AHBX | 1 |  | jmgpu_ahbtransform.h:5054 |
| `0x032F8` | `0x0CBE` | MWV207REG_AHBX_TRANSMITTER_DBG2 | AHBX | 1 |  | jmgpu_ahbtransform.h:5070 |
| `0x032FC` | `0x0CBF` | MWV207REG_AHBX_RECEIVER_DBG0 | AHBX | 1 |  | jmgpu_ahbtransform.h:5086 |
| `0x03300` | `0x0CC0` | MWV207REG_AHBX_RECEIVER_DBG1 | AHBX | 1 |  | jmgpu_ahbtransform.h:5102 |
| `0x03304` | `0x0CC1` | MWV207REG_AHBX_RECEIVER_DBG2 | AHBX | 1 |  | jmgpu_ahbtransform.h:5118 |
| `0x03308` | `0x0CC2` | MWV207REG_AHBXDMA0_DBG0 | AHBXDMA0 | 1 |  | jmgpu_ahbtransform.h:5134 |
| `0x0330C` | `0x0CC3` | MWV207REG_AHBXDMA0_DBG1 | AHBXDMA0 | 1 |  | jmgpu_ahbtransform.h:5150 |
| `0x03310` | `0x0CC4` | MWV207REG_AHBXDMA0_DBG2 | AHBXDMA0 | 1 |  | jmgpu_ahbtransform.h:5166 |
| `0x03314` | `0x0CC5` | MWV207REG_AHBXDMA1_DBG0 | AHBXDMA1 | 1 |  | jmgpu_ahbtransform.h:5182 |
| `0x03318` | `0x0CC6` | MWV207REG_AHBXDMA1_DBG1 | AHBXDMA1 | 1 |  | jmgpu_ahbtransform.h:5198 |
| `0x0331C` | `0x0CC7` | MWV207REG_AHBXDMA1_DBG2 | AHBXDMA1 | 1 |  | jmgpu_ahbtransform.h:5214 |
| `0x03320` | `0x0CC8` | MWV207REG_AHBXDMA2_DBG0 | AHBXDMA2 | 1 |  | jmgpu_ahbtransform.h:5230 |
| `0x03324` | `0x0CC9` | MWV207REG_AHBXDMA2_DBG1 | AHBXDMA2 | 1 |  | jmgpu_ahbtransform.h:5246 |
| `0x03328` | `0x0CCA` | MWV207REG_AHBXDMA2_DBG2 | AHBXDMA2 | 1 |  | jmgpu_ahbtransform.h:5262 |
| `0x0332C` | `0x0CCB` | MWV207REG_AHBXDMA3_DBG0 | AHBXDMA3 | 1 |  | jmgpu_ahbtransform.h:5278 |
| `0x03330` | `0x0CCC` | MWV207REG_AHBXDMA3_DBG1 | AHBXDMA3 | 1 |  | jmgpu_ahbtransform.h:5294 |
| `0x03334` | `0x0CCD` | MWV207REG_AHBXDMA3_DBG2 | AHBXDMA3 | 1 |  | jmgpu_ahbtransform.h:5310 |
| `0x03338` | `0x0CCE` | MWV207REG_AHBXDMA4_DBG0 | AHBXDMA4 | 1 |  | jmgpu_ahbtransform.h:5326 |
| `0x0333C` | `0x0CCF` | MWV207REG_AHBXDMA4_DBG1 | AHBXDMA4 | 1 |  | jmgpu_ahbtransform.h:5342 |
| `0x03340` | `0x0CD0` | MWV207REG_AHBXDMA4_DBG2 | AHBXDMA4 | 1 |  | jmgpu_ahbtransform.h:5358 |
| `0x03818` | `0x0E06` | MWV207REG_MULTI_SAMPLE_CONFIG | MULTI | 14 |  | jmgpu_engine.h:431 |
| `0x0381C` | `0x0E07` | MWV207REG_VARYINGS | ? | 1 |  | jmgpu_engine.h:561 |
| `0x03820` | `0x0E08` | MWV207REG_VARYING_PACKING | VARYING | 8 |  | jmgpu_engine.h:580 |
| `0x03824` | `0x0E09` | MWV207REG_OCCLUSION_ADDRESS | OCCLUSION | 2 |  | jmgpu_engine.h:954 |
| `0x03828` | `0x0E0A` | MWV207REG_COMPONENT_TYPE_LOW | COMPONENT | 16 |  | jmgpu_engine.h:642 |
| `0x0382C` | `0x0E0B` | MWV207REG_COMPONENT_TYPE_HIGH | COMPONENT | 16 |  | jmgpu_engine.h:797 |
| `0x03830` | `0x0E0C` | MWV207REG_OCCLUSION_DATA | OCCLUSION | 1 |  | jmgpu_engine.h:979 |
| `0x03834` | `0x0E0D` | MWV207REG_VARYING_PACKING_EX | VARYING | 8 |  | jmgpu_engine.h:998 |
| `0x03838` | `0x0E0E` | MWV207REG_COMPONENT_TYPE_EX | COMPONENT | 16 |  | jmgpu_engine.h:1060 |
| `0x0383C` | `0x0E0F` | MWV207REG_WINDOW_CONTROL | WINDOW | 3 |  | jmgpu_engine.h:1218 |
| `0x03840` | `0x0E10` | MWV207REG_WINDOW_ORIGIN | WINDOW | 2 |  | jmgpu_engine.h:1256 |
| `0x03844` | `0x0E11` | MWV207REG_WINDOW_SIZE | WINDOW | 2 |  | jmgpu_engine.h:1282 |
| `0x03848` | `0x0E12` | MWV207REG_STORE_COUNTER | STORE | 17 |  | jmgpu_engine.h:1307 |
| `0x03850` | `0x0E14` | MWV207REG_CONTEXT | ? | 1 |  | jmgpu_engine.h:1431 |
| `0x03854` | `0x0E15` | MWV207REG_COMPONENT_TYPE_EX2 | COMPONENT | 16 |  | jmgpu_engine.h:1450 |
| `0x03858` | `0x0E16` | MWV207REG_PC_ADDRESS | PC | 1 |  | jmgpu_engine.h:1607 |
| `0x0385C` | `0x0E17` | MWV207REG_TX_CACHE | TX | 1 |  | jmgpu_engine.h:1626 |
| `0x03860` | `0x0E18` | MWV207REG_OCCLUSION_CONTROL | OCCLUSION | 1 |  | jmgpu_engine.h:1647 |
| `0x03864` | `0x0E19` | MWV207REG_CLOCK_CONTROL | CLOCK | 3 |  | jmgpu_engine.h:1678 |
| `0x03868` | `0x0E1A` | MWV207REG_FENCE_ADDRESS | FENCE | 2 |  | jmgpu_engine.h:1707 |
| `0x0386C` | `0x0E1B` | MWV207REG_FENCE_DATA | FENCE | 1 |  | jmgpu_engine.h:1733 |
| `0x03870` | `0x0E1C` | MWV207REG_PROBE_ADDRESS | PROBE | 2 |  | jmgpu_engine.h:1752 |
| `0x03874` | `0x0E1D` | MWV207REG_PROBE_DATA | PROBE | 1 |  | jmgpu_engine.h:1778 |
| `0x03878` | `0x0E1E` | MWV207REG_PROBE_SELECT | PROBE | 3 |  | jmgpu_engine.h:1797 |
| `0x0387C` | `0x0E1F` | MWV207REG_TOGGLE_SWITCH | TOGGLE | 4 |  | jmgpu_engine.h:1852 |
| `0x03880` | `0x0E20` | MWV207REG_PERFORMANCE | ? | 4 |  | jmgpu_engine.h:1895 |
| `0x03884` | `0x0E21` | MWV207REG_USC_CONTROL | USC | 6 |  | jmgpu_engine.h:1949 |
| `0x03888` | `0x0E22` | MWV207REG_SEMANTIC_LOCATION | SEMANTIC | 4 |  | jmgpu_engine.h:2202 |
| `0x0388C` | `0x0E23` | MWV207REG_DEPTH_SLICE | DEPTH | 1 |  | jmgpu_engine.h:2257 |
| `0x03890` | `0x0E24` | MWV207REG_SHADER_L1C_FLUSH_ONLY | SHADER | 5 |  | jmgpu_engine.h:2277 |
| `0x03894` | `0x0E25` | MWV207REG_SHADER_L1C_FLUSH_WAIT | SHADER | 5 |  | jmgpu_engine.h:2316 |
| `0x03898` | `0x0E26` | MWV207REG_FENCE_DATA_HIGH | FENCE | 1 |  | jmgpu_engine.h:2355 |
| `0x0389C` | `0x0E27` | MWV207REG_DRAW_ID | DRAW | 1 |  | jmgpu_engine.h:2374 |
| `0x038A0` | `0x0E28` | MWV207REG_COLOR_SLICE | COLOR | 1 |  | jmgpu_engine.h:2237 |
| `0x038C0` | `0x0E30` | MWV207REG_SEMANTICS | ? | 16 |  | jmgpu_engine.h:2036 |
| `0x03900` | `0x0E40` | MWV207REG_APP_CONTEXT_ID_LOW | APP | 1 |  | jmgpu_engine.h:2393 |
| `0x03904` | `0x0E41` | MWV207REG_APP_CONTEXT_ID_HIGH | APP | 1 |  | jmgpu_engine.h:2412 |
| `0x03908` | `0x0E42` | MWV207REG_CLUSTER_CHECKER_BOARD | CLUSTER | 1 |  | jmgpu_engine.h:2451 |
| `0x0390C` | `0x0E43` | MWV207REG_TRIGGER_NN | TRIGGER | 1 |  | jmgpu_engine.h:2497 |
| `0x03910` | `0x0E44` | MWV207REG_CLUSTER_CONTROL | CLUSTER | 1 |  | jmgpu_engine.h:2431 |
| `0x03920` | `0x0E48` | MWV207REG_POWER_CLUSTER_CONTROL | POWER | 1 |  | jmgpu_engine.h:2478 |
| `0x03930` | `0x0E4C` | MWV207REG_CONFIG_NN | CONFIG | 15 |  | jmgpu_engine.h:2516 |
| `0x03934` | `0x0E4D` | MWV207REG_TRIGGER_TP | TRIGGER | 1 |  | jmgpu_engine.h:2652 |
| `0x03938` | `0x0E4E` | MWV207REG_SRAM_REMAP_START_ADDRESS | SRAM | 1 |  | jmgpu_engine.h:2671 |
| `0x0393C` | `0x0E4F` | MWV207REG_ON_CHIP_BUFFER_REMAP_START_ADDRESS | ON | 1 |  | jmgpu_engine.h:2689 |
| `0x03940` | `0x0E50` | MWV207REG_ON_CHIP_BUFFER_REMAP_END_ADDRESS | ON | 1 |  | jmgpu_engine.h:2707 |
| `0x03944` | `0x0E51` | MWV207REG_SRAM_IMAGE_STREAMING_BASE_ADDRESS | SRAM | 1 |  | jmgpu_engine.h:2726 |
| `0x03948` | `0x0E52` | MWV207REG_SRAM_IMAGE_STREAMING_BUFFER_SIZE | SRAM | 1 |  | jmgpu_engine.h:2745 |
| `0x0394C` | `0x0E53` | MWV207REG_CONFIG_TP | CONFIG | 4 |  | jmgpu_engine.h:2764 |
| `0x03950` | `0x0E54` | MWV207REG_VIP_FLUSH | VIP | 4 |  | jmgpu_engine.h:2813 |
| `0x03954` | `0x0E55` | MWV207REG_PROBE_SELECT_EXTEND | PROBE | 3 |  | jmgpu_engine.h:2851 |
| `0x03958` | `0x0E56` | MWV207REG_AXI_SRAM_REMAP_VIRTUAL_START_ADDRESS | AXI | 1 |  | jmgpu_engine.h:2888 |
| `0x0395C` | `0x0E57` | MWV207REG_AXI_SRAM_REMAP_VIRTUAL_END_ADDRESS | AXI | 1 |  | jmgpu_engine.h:2907 |
| `0x03960` | `0x0E58` | MWV207REG_STENCIL_SLICE | STENCIL | 1 |  | jmgpu_engine.h:2926 |
| `0x03964` | `0x0E59` | MWV207REG_VIP40_BITS_VA | VIP40 | 4 |  | jmgpu_engine.h:2943 |
| `0x03968` | `0x0E5A` | MWV207REG_VIP_MISC | VIP | 8 |  | jmgpu_engine.h:2977 |
| `0x0396C` | `0x0E5B` | MWV207REG_SP_INSTR_MEM_ADDR | SP | 1 |  | jmgpu_engine.h:3081 |
| `0x03970` | `0x0E5C` | MWV207REG_SP_INSTR_MEM_DATA_LOW | SP | 1 |  | jmgpu_engine.h:3101 |
| `0x03974` | `0x0E5D` | MWV207REG_SP_INSTR_MEM_DATA_HIGH | SP | 1 |  | jmgpu_engine.h:3117 |
| `0x03978` | `0x0E5E` | MWV207REG_CMD_LOOP_NN_CMD_ITERATION | CMD | 1 |  | jmgpu_engine.h:3174 |
| `0x0397C` | `0x0E5F` | MWV207REG_CMD_LOOP_NN_REGION_BASE_ADDR0 | CMD | 1 |  | jmgpu_engine.h:3192 |
| `0x03980` | `0x0E60` | MWV207REG_CLUSTER_CONTROL1 | CLUSTER | 1 |  | jmgpu_engine.h:3136 |
| `0x03990` | `0x0E64` | MWV207REG_POWER_CLUSTER_CONTROL1 | POWER | 1 |  | jmgpu_engine.h:3156 |
| `0x039A0` | `0x0E68` | MWV207REG_CMD_LOOP_NN_REGION_BASE_ADDR1 | CMD | 1 |  | jmgpu_engine.h:3210 |
| `0x039A4` | `0x0E69` | MWV207REG_CMD_LOOP_NN_REGION_BASE_ADDR2 | CMD | 1 |  | jmgpu_engine.h:3228 |
| `0x039A8` | `0x0E6A` | MWV207REG_CMD_LOOP_NN_REGION_BASE_ADDR3 | CMD | 1 |  | jmgpu_engine.h:3246 |
| `0x039AC` | `0x0E6B` | MWV207REG_VIP_CLOCK_GATE | VIP | 28 |  | jmgpu_engine.h:3264 |
| `0x039B0` | `0x0E6C` | MWV207REG_CULL_DISTANCE_LOCATION0 | CULL | 4 |  | jmgpu_engine.h:3442 |
| `0x039B4` | `0x0E6D` | MWV207REG_CULL_DISTANCE_LOCATION1 | CULL | 4 |  | jmgpu_engine.h:3480 |
| `0x039B8` | `0x0E6E` | MWV207REG_CLIP_DISTANCE_LOCATION0 | CLIP | 4 |  | jmgpu_engine.h:3518 |
| `0x039BC` | `0x0E6F` | MWV207REG_CLIP_DISTANCE_LOCATION1 | CLIP | 4 |  | jmgpu_engine.h:3556 |
| `0x039C0` | `0x0E70` | MWV207REG_VIEWPORT_ARRAY_INDEX_LOCATION | VIEWPORT | 1 |  | jmgpu_engine.h:3594 |
| `0x039C4` | `0x0E71` | MWV207REG_MULTI_SAMPLE_ENABLE_EX | MULTI | 1 |  | jmgpu_engine.h:3614 |
| `0x039C8` | `0x0E72` | MWV207REG_CONFIG_NN2 | CONFIG | 1 |  | jmgpu_engine.h:3633 |
| `0x03A00` | `0x0E80` | MWV207REG_MULTI_CHIP_CONTROL | MULTI | 20 |  | jmgpu_deploy.h:29 |
| `0x03A04` | `0x0E81` | MWV207REG_MULTI_CHIP_TOP_LEFT | MULTI | 2 |  | jmgpu_deploy.h:239 |
| `0x03A08` | `0x0E82` | MWV207REG_MULTI_CHIP_BOTTOM_RIGHT | MULTI | 2 |  | jmgpu_deploy.h:263 |
| `0x03A0C` | `0x0E83` | MWV207REG_MULTI_CHIP_SET_MAPPING | MULTI | 4 |  | jmgpu_deploy.h:287 |
| `0x03A10` | `0x0E84` | MWV207REG_MULTI_CHIP_SET_MAPPING_EX | MULTI | 4 |  | jmgpu_deploy.h:325 |
| `0x08000` | `0x2000` | MWV207REG_PS_SHADER_CODE | PS | 1 |  | jmgpu_announce.h:2993 |
| `0x0C000` | `0x3000` | MWV207REG_VS_SHADER_CODE | VS | 1 |  | jmgpu_announce.h:617 |
| `0x10000` | `0x4000` | MWV207REG_TX_CONFIG | TX | 12 |  | jmgpu_neighbor.h:26 |
| `0x10080` | `0x4020` | MWV207REG_TX_SIZE | TX | 2 |  | jmgpu_neighbor.h:183 |
| `0x10100` | `0x4040` | MWV207REG_TX_LOG_SIZE | TX | 6 |  | jmgpu_neighbor.h:206 |
| `0x10180` | `0x4060` | MWV207REG_TX_LOD | TX | 4 |  | jmgpu_neighbor.h:269 |
| `0x10200` | `0x4080` | MWV207REG_TX_BORDER_COLOR | TX | 4 |  | jmgpu_neighbor.h:306 |
| `0x10280` | `0x40A0` | MWV207REG_TX_LINEAR_STRIDE | TX | 1 |  | jmgpu_neighbor.h:340 |
| `0x10300` | `0x40C0` | MWV207REG_TX3_D | TX3 | 3 |  | jmgpu_neighbor.h:360 |
| `0x10380` | `0x40E0` | MWV207REG_TX_EXT_CONFIG | TX | 16 |  | jmgpu_neighbor.h:396 |
| `0x10400` | `0x4100` | MWV207REG_TX_CONTROL_YUV_EX | TX | 15 |  | jmgpu_neighbor.h:649 |
| `0x10480` | `0x4120` | MWV207REG_TX_STRIDE_YUV_EX | TX | 2 |  | jmgpu_neighbor.h:798 |
| `0x10500` | `0x4140` | MWV207REG_TX_ASTC0_EX | TX | 8 |  | jmgpu_neighbor.h:824 |
| `0x10580` | `0x4160` | MWV207REG_TX_ASTC1_EX | TX | 8 |  | jmgpu_neighbor.h:950 |
| `0x10600` | `0x4180` | MWV207REG_TX_ASTC2_EX | TX | 8 |  | jmgpu_neighbor.h:1076 |
| `0x10680` | `0x41A0` | MWV207REG_TX_ASTC3_EX | TX | 8 |  | jmgpu_neighbor.h:1202 |
| `0x10700` | `0x41C0` | MWV207REG_TX_BASE_LOD | TX | 5 |  | jmgpu_neighbor.h:1328 |
| `0x10780` | `0x41E0` | MWV207REG_TX_CONFIG2 | TX | 12 |  | jmgpu_neighbor.h:1383 |
| `0x10800` | `0x4200` | MWV207REG_TX_ADDRESS | TX | 2 |  | jmgpu_neighbor.h:623 |
| `0x11000` | `0x4400` | MWV207REG_TX_SIZE_EXT | TX | 2 |  | jmgpu_neighbor.h:1494 |
| `0x11080` | `0x4420` | MWV207REG_TX_VOLUME_EXT | TX | 1 |  | jmgpu_neighbor.h:1517 |
| `0x11100` | `0x4440` | MWV207REG_TX_LOD_EXT | TX | 2 |  | jmgpu_neighbor.h:1534 |
| `0x11180` | `0x4460` | MWV207REG_TX_LOD_BIAS_EXT | TX | 2 |  | jmgpu_neighbor.h:1557 |
| `0x11200` | `0x4480` | MWV207REG_TX_SLICE | TX | 1 |  | jmgpu_neighbor.h:1583 |
| `0x11280` | `0x44A0` | MWV207REG_TX_CONFIG3 | TX | 6 |  | jmgpu_neighbor.h:1600 |
| `0x11300` | `0x44C0` | MWV207REG_TX_ANISO_CTRL | TX | 1 |  | jmgpu_neighbor.h:1671 |
| `0x11380` | `0x44E0` | MWV207REG_TX_BORDER_COLOR_RED32 | TX | 1 |  | jmgpu_neighbor.h:1691 |
| `0x11400` | `0x4500` | MWV207REG_TX_BORDER_COLOR_GREEN32 | TX | 1 |  | jmgpu_neighbor.h:1711 |
| `0x11480` | `0x4520` | MWV207REG_TX_BORDER_COLOR_BLUE32 | TX | 1 |  | jmgpu_neighbor.h:1731 |
| `0x11500` | `0x4540` | MWV207REG_TX_BORDER_COLOR_ALPHA32 | TX | 1 |  | jmgpu_neighbor.h:1751 |
| `0x12000` | `0x4800` | MWV207REG_TX_HORIZONTAL_KERNEL | TX | 4 |  | jmgpu_neighbor.h:1772 |
| `0x12400` | `0x4900` | MWV207REG_TX_VERTICAL_KERNEL | TX | 4 |  | jmgpu_neighbor.h:1810 |
| `0x12800` | `0x4A00` | MWV207REG_DE_SRC_ADDRESS | DE | 2 |  | jmgpu_interview.h:4765 |
| `0x12810` | `0x4A04` | MWV207REG_DE_SRC_STRIDE | DE | 1 |  | jmgpu_interview.h:4790 |
| `0x12820` | `0x4A08` | MWV207REG_DE_SRC_ROTATION_CONFIG | DE | 2 |  | jmgpu_interview.h:4808 |
| `0x12830` | `0x4A0C` | MWV207REG_DE_SRC_CONFIG | DE | 12 |  | jmgpu_interview.h:4834 |
| `0x12840` | `0x4A10` | MWV207REG_DE_SRC_ORIGIN | DE | 2 |  | jmgpu_interview.h:4984 |
| `0x12850` | `0x4A14` | MWV207REG_DE_SRC_SIZE | DE | 2 |  | jmgpu_interview.h:5007 |
| `0x12860` | `0x4A18` | MWV207REG_DE_SRC_COLOR_BG | DE | 4 |  | jmgpu_interview.h:5030 |
| `0x12870` | `0x4A1C` | MWV207REG_DE_ROP | DE | 3 |  | jmgpu_interview.h:5063 |
| `0x12880` | `0x4A20` | MWV207REG_DE_ALPHA_CONTROL | DE | 1 |  | jmgpu_interview.h:5096 |
| `0x12890` | `0x4A24` | MWV207REG_DE_ALPHA_MODES | DE | 8 |  | jmgpu_interview.h:5114 |
| `0x128A0` | `0x4A28` | MWV207REG_DE_ADDRESS_U | DE | 2 |  | jmgpu_interview.h:5200 |
| `0x128B0` | `0x4A2C` | MWV207REG_DE_STRIDE_U | DE | 1 |  | jmgpu_interview.h:5226 |
| `0x128C0` | `0x4A30` | MWV207REG_DE_ADDRESS_V | DE | 2 |  | jmgpu_interview.h:5245 |
| `0x128D0` | `0x4A34` | MWV207REG_DE_STRIDE_V | DE | 1 |  | jmgpu_interview.h:5271 |
| `0x128E0` | `0x4A38` | MWV207REG_DE_SRC_ROTATION_HEIGHT | DE | 1 |  | jmgpu_interview.h:5289 |
| `0x128F0` | `0x4A3C` | MWV207REG_DE_ROT_ANGLE | DE | 12 |  | jmgpu_interview.h:5307 |
| `0x12900` | `0x4A40` | MWV207REG_DE_GLOBAL_SRC_COLOR | DE | 4 |  | jmgpu_interview.h:5421 |
| `0x12910` | `0x4A44` | MWV207REG_DE_GLOBAL_DEST_COLOR | DE | 4 |  | jmgpu_interview.h:5455 |
| `0x12920` | `0x4A48` | MWV207REG_DE_COLOR_MULTIPLY_MODES | DE | 5 |  | jmgpu_interview.h:5488 |
| `0x12930` | `0x4A4C` | MWV207REG_PE_TRANSPARENCY | PE | 10 |  | jmgpu_interview.h:5535 |
| `0x12940` | `0x4A50` | MWV207REG_PE_CONTROL | PE | 10 |  | jmgpu_interview.h:5634 |
| `0x12950` | `0x4A54` | MWV207REG_DE_SRC_COLOR_KEY_HIGH | DE | 4 |  | jmgpu_interview.h:5723 |
| `0x12960` | `0x4A58` | MWV207REG_DE_SRC_EX_CONFIG | DE | 4 |  | jmgpu_interview.h:5754 |
| `0x12970` | `0x4A5C` | MWV207REG_DE_SRC_EX_ADDRESS | DE | 2 |  | jmgpu_interview.h:5800 |
| `0x12980` | `0x4A60` | MWV207REG_DE_SRC_PLANE0_ALIGN_CONFIG | DE | 1 |  | jmgpu_interview.h:5824 |
| `0x129A0` | `0x4A68` | MWV207REG_DE_SRC_PLANE1_ALIGN_CONFIG | DE | 1 |  | jmgpu_interview.h:5840 |
| `0x129C0` | `0x4A70` | MWV207REG_DE_SRC_PLANE2_ALIGN_CONFIG | DE | 1 |  | jmgpu_interview.h:5856 |
| `0x12A00` | `0x4A80` | MWV207REG_DE_SRC_ADDRESS_EX | DE | 2 |  | jmgpu_interview.h:5876 |
| `0x12A20` | `0x4A88` | MWV207REG_DE_SRC_STRIDE_EX | DE | 1 |  | jmgpu_interview.h:5901 |
| `0x12A40` | `0x4A90` | MWV207REG_DE_SRC_ROTATION_CONFIG_EX | DE | 2 |  | jmgpu_interview.h:5919 |
| `0x12A60` | `0x4A98` | MWV207REG_DE_SRC_CONFIG_EX | DE | 14 |  | jmgpu_interview.h:5945 |
| `0x12A80` | `0x4AA0` | MWV207REG_DE_SRC_ORIGIN_EX | DE | 2 |  | jmgpu_interview.h:6116 |
| `0x12AA0` | `0x4AA8` | MWV207REG_DE_SRC_SIZE_EX | DE | 2 |  | jmgpu_interview.h:6139 |
| `0x12AC0` | `0x4AB0` | MWV207REG_DE_SRC_COLOR_BG_EX | DE | 4 |  | jmgpu_interview.h:6162 |
| `0x12AE0` | `0x4AB8` | MWV207REG_DE_ROP_EX | DE | 3 |  | jmgpu_interview.h:6195 |
| `0x12B00` | `0x4AC0` | MWV207REG_DE_ALPHA_CONTROL_EX | DE | 1 |  | jmgpu_interview.h:6228 |
| `0x12B20` | `0x4AC8` | MWV207REG_DE_ALPHA_MODES_EX | DE | 8 |  | jmgpu_interview.h:6246 |
| `0x12B40` | `0x4AD0` | MWV207REG_DE_ADDRESS_UEX | DE | 2 |  | jmgpu_interview.h:6332 |
| `0x12B60` | `0x4AD8` | MWV207REG_DE_STRIDE_UEX | DE | 1 |  | jmgpu_interview.h:6358 |
| `0x12B80` | `0x4AE0` | MWV207REG_DE_ADDRESS_VEX | DE | 2 |  | jmgpu_interview.h:6377 |
| `0x12BA0` | `0x4AE8` | MWV207REG_DE_STRIDE_VEX | DE | 1 |  | jmgpu_interview.h:6403 |
| `0x12BC0` | `0x4AF0` | MWV207REG_DE_SRC_ROTATION_HEIGHT_EX | DE | 1 |  | jmgpu_interview.h:6421 |
| `0x12BE0` | `0x4AF8` | MWV207REG_DE_ROT_ANGLE_EX | DE | 12 |  | jmgpu_interview.h:6439 |
| `0x12C00` | `0x4B00` | MWV207REG_DE_GLOBAL_SRC_COLOR_EX | DE | 4 |  | jmgpu_interview.h:6553 |
| `0x12C20` | `0x4B08` | MWV207REG_DE_GLOBAL_DEST_COLOR_EX | DE | 4 |  | jmgpu_interview.h:6587 |
| `0x12C40` | `0x4B10` | MWV207REG_DE_COLOR_MULTIPLY_MODES_EX | DE | 5 |  | jmgpu_interview.h:6620 |
| `0x12C60` | `0x4B18` | MWV207REG_PE_TRANSPARENCY_EX | PE | 10 |  | jmgpu_interview.h:6667 |
| `0x12C80` | `0x4B20` | MWV207REG_PE_CONTROL_EX | PE | 10 |  | jmgpu_interview.h:6766 |
| `0x12CA0` | `0x4B28` | MWV207REG_DE_SRC_COLOR_KEY_HIGH_EX | DE | 4 |  | jmgpu_interview.h:6855 |
| `0x12CC0` | `0x4B30` | MWV207REG_DE_SRC_EX_CONFIG_EX | DE | 8 |  | jmgpu_interview.h:6886 |
| `0x12CE0` | `0x4B38` | MWV207REG_DE_SRC_EX_ADDRESS_EX | DE | 2 |  | jmgpu_interview.h:6962 |
| `0x12D00` | `0x4B40` | MWV207REG_DE_CLIP_TOP_LEFT | DE | 2 |  | jmgpu_interview.h:6987 |
| `0x12D20` | `0x4B48` | MWV207REG_DE_CLIP_BOTTOM_RIGHT | DE | 2 |  | jmgpu_interview.h:7010 |
| `0x12D40` | `0x4B50` | MWV207REG_DE_DEST_RECT_TOP_LEFT | DE | 2 |  | jmgpu_interview.h:7034 |
| `0x12D60` | `0x4B58` | MWV207REG_DE_DEST_RECT_BOTTOM_RIGHT | DE | 2 |  | jmgpu_interview.h:7058 |
| `0x12D80` | `0x4B60` | MWV207REG_DE_STRETCH_FACTOR_LOW | DE | 1 |  | jmgpu_interview.h:7079 |
| `0x12DA0` | `0x4B68` | MWV207REG_DE_STRETCH_FACTOR_HIGH | DE | 1 |  | jmgpu_interview.h:7095 |
| `0x12DC0` | `0x4B70` | MWV207REG_DE_SRC_VINITIAL_ERROR | DE | 1 |  | jmgpu_interview.h:7114 |
| `0x12DE0` | `0x4B78` | MWV207REG_DE_SRC_VSOUTH_DELTA | DE | 1 |  | jmgpu_interview.h:7133 |
| `0x12E00` | `0x4B80` | MWV207REG_DE_SRC_VNORTH_DELTA | DE | 1 |  | jmgpu_interview.h:7152 |
| `0x12E20` | `0x4B88` | MWV207REG_DE_SRC_HINITIAL_ERROR | DE | 1 |  | jmgpu_interview.h:7171 |
| `0x12E40` | `0x4B90` | MWV207REG_DE_SRC_HSOUTH_DELTA | DE | 1 |  | jmgpu_interview.h:7190 |
| `0x12E60` | `0x4B98` | MWV207REG_DE_SRC_HNORTH_DELTA | DE | 1 |  | jmgpu_interview.h:7209 |
| `0x12E80` | `0x4BA0` | MWV207REG_MULTI_SRC_CONFIG | MULTI | 6 |  | jmgpu_interview.h:7228 |
| `0x12EA0` | `0x4BA8` | MWV207REG_PATTERN_FG_COLOR | PATTERN | 4 |  | jmgpu_interview.h:7283 |
| `0x12EC0` | `0x4BB0` | MWV207REG_PATTERN_CONFIG | PATTERN | 1 |  | jmgpu_interview.h:7317 |
| `0x12EE0` | `0x4BB8` | MWV207REG_DE_SRC_TILE_STATUS_ADDRESS | DE | 2 |  | jmgpu_interview.h:7338 |
| `0x12F60` | `0x4BD8` | MWV207REG_TPCNV12_SRC_CONFIG | TPCNV12 | 6 |  | jmgpu_interview.h:7488 |
| `0x12F80` | `0x4BE0` | MWV207REG_DE_SRC_ORIGIN_FRACTION | DE | 2 |  | jmgpu_interview.h:7546 |
| `0x12FA0` | `0x4BE8` | MWV207REG_DE_SRC_TILE_STATUS_CLEAR | DE | 1 |  | jmgpu_interview.h:7570 |
| `0x12FC0` | `0x4BF0` | MWV207REG_DE_SRC_TILE_STATUS_CONFIG | DE | 8 |  | jmgpu_interview.h:7586 |
| `0x12FE0` | `0x4BF8` | MWV207REG_DE_SRC_TILE_STATUS_CLEAR64 | DE | 1 |  | jmgpu_interview.h:7676 |
| `0x13000` | `0x4C00` | MWV207REG_PC_TIME_STAMP | PC | 1 |  | jmgpu_discovery.h:29 |
| `0x13004` | `0x4C01` | MWV207REG_PC_COMAMD_BYTES_FETCHED | PC | 1 |  | jmgpu_discovery.h:48 |
| `0x13008` | `0x4C02` | MWV207REG_PC_DRAW_COMMANDS | PC | 1 |  | jmgpu_discovery.h:67 |
| `0x1300C` | `0x4C03` | MWV207REG_PC_INDEX_BYTES_FETCHED | PC | 1 |  | jmgpu_discovery.h:86 |
| `0x13010` | `0x4C04` | MWV207REG_PC_INDEX_CACHE_HIT | PC | 1 |  | jmgpu_discovery.h:105 |
| `0x13014` | `0x4C05` | MWV207REG_PC_VERTEX_COUNT | PC | 1 |  | jmgpu_discovery.h:124 |
| `0x13018` | `0x4C06` | MWV207REG_PC_VERTEX_BYTES_FETCHED | PC | 1 |  | jmgpu_discovery.h:143 |
| `0x1301C` | `0x4C07` | MWV207REG_PC_TOTAL_VERTEX | PC | 1 |  | jmgpu_discovery.h:162 |
| `0x13020` | `0x4C08` | MWV207REG_PC_TOTAL_PRIMITIVE | PC | 1 |  | jmgpu_discovery.h:181 |
| `0x13024` | `0x4C09` | MWV207REG_PC_TRIVIAL_REJECT_PRIMITIVE | PC | 1 |  | jmgpu_discovery.h:200 |
| `0x13028` | `0x4C0A` | MWV207REG_PC_CULLED_PRIMITIVE | PC | 1 |  | jmgpu_discovery.h:219 |
| `0x1302C` | `0x4C0B` | MWV207REG_PC_DEPTH_CLIPPED_PRIMITIVE | PC | 1 |  | jmgpu_discovery.h:238 |
| `0x13030` | `0x4C0C` | MWV207REG_PC_FRUSTUM_CLIPPED_PRIMITIVE | PC | 1 |  | jmgpu_discovery.h:257 |
| `0x13034` | `0x4C0D` | MWV207REG_PC_OUTPUT_PRIMITIVE | PC | 1 |  | jmgpu_discovery.h:276 |
| `0x13038` | `0x4C0E` | MWV207REG_PC_CULLED_TRIANGLES | PC | 1 |  | jmgpu_discovery.h:295 |
| `0x1303C` | `0x4C0F` | MWV207REG_PC_CULLED_LINES | PC | 1 |  | jmgpu_discovery.h:314 |
| `0x13040` | `0x4C10` | MWV207REG_PC_CULLED_RECTANGLES | PC | 1 |  | jmgpu_discovery.h:333 |
| `0x13044` | `0x4C11` | MWV207REG_PC_CLIPPED_TRIANGLES | PC | 1 |  | jmgpu_discovery.h:352 |
| `0x13048` | `0x4C12` | MWV207REG_PC_CLIPPED_LINES | PC | 1 |  | jmgpu_discovery.h:371 |
| `0x1304C` | `0x4C13` | MWV207REG_PC_CLIPPED_RECTANGLES | PC | 1 |  | jmgpu_discovery.h:390 |
| `0x13050` | `0x4C14` | MWV207REG_PC_RASTERIZED_PRIMITIVES | PC | 1 |  | jmgpu_discovery.h:409 |
| `0x13054` | `0x4C15` | MWV207REG_PC_TOTAL_QUADS | PC | 1 |  | jmgpu_discovery.h:428 |
| `0x13058` | `0x4C16` | MWV207REG_PC_CULLED_HZ | PC | 1 |  | jmgpu_discovery.h:447 |
| `0x1305C` | `0x4C17` | MWV207REG_PC_CULLED_EEZ | PC | 1 |  | jmgpu_discovery.h:466 |
| `0x13060` | `0x4C18` | MWV207REG_PC_VALID_QUADS | PC | 1 |  | jmgpu_discovery.h:485 |
| `0x13064` | `0x4C19` | MWV207REG_PC_VALID_PIXELS | PC | 1 |  | jmgpu_discovery.h:504 |
| `0x13068` | `0x4C1A` | MWV207REG_PCHZ_CACHE_REQUESTS | PCHZ | 1 |  | jmgpu_discovery.h:523 |
| `0x1306C` | `0x4C1B` | MWV207REG_PCHZ_CACHE_MISSES | PCHZ | 1 |  | jmgpu_discovery.h:542 |
| `0x13070` | `0x4C1C` | MWV207REG_PCHZ_BYTES_FETCHED | PCHZ | 1 |  | jmgpu_discovery.h:561 |
| `0x13074` | `0x4C1D` | MWV207REG_PCEZ_CACHE_REQUESTS | PCEZ | 1 |  | jmgpu_discovery.h:580 |
| `0x13078` | `0x4C1E` | MWV207REG_PCEZ_CACHE_MISSES | PCEZ | 1 |  | jmgpu_discovery.h:599 |
| `0x1307C` | `0x4C1F` | MWV207REG_PCEZ_BYTES_FETCHED | PCEZ | 1 |  | jmgpu_discovery.h:618 |
| `0x13080` | `0x4C20` | MWV207REG_PC_COLOR_KILLED_PIXELS | PC | 1 |  | jmgpu_discovery.h:637 |
| `0x130A0` | `0x4C28` | MWV207REG_PC_DEPTH_KILLED_PIXELS | PC | 1 |  | jmgpu_discovery.h:656 |
| `0x130C0` | `0x4C30` | MWV207REG_PC_STENCIL_KILLED_PIXELS | PC | 1 |  | jmgpu_discovery.h:675 |
| `0x130E0` | `0x4C38` | MWV207REG_PC_COLOR_RENDERED_PIXELS | PC | 1 |  | jmgpu_discovery.h:694 |
| `0x13100` | `0x4C40` | MWV207REG_PC_DEPTH_RENDERED_PIXELS | PC | 1 |  | jmgpu_discovery.h:713 |
| `0x13120` | `0x4C48` | MWV207REG_PCC_CACHE_REQUESTS | PCC | 1 |  | jmgpu_discovery.h:732 |
| `0x13140` | `0x4C50` | MWV207REG_PCC_CACHE_MISSES | PCC | 1 |  | jmgpu_discovery.h:751 |
| `0x13160` | `0x4C58` | MWV207REG_PCC_CACHE_BYTES_FETCHED | PCC | 1 |  | jmgpu_discovery.h:770 |
| `0x13180` | `0x4C60` | MWV207REG_PCC_CACHE_BYTES_WRITTEN | PCC | 1 |  | jmgpu_discovery.h:789 |
| `0x131A0` | `0x4C68` | MWV207REG_PCZ_CACHE_REQUESTS | PCZ | 1 |  | jmgpu_discovery.h:808 |
| `0x131C0` | `0x4C70` | MWV207REG_PCZ_CACHE_MISSES | PCZ | 1 |  | jmgpu_discovery.h:827 |
| `0x131E0` | `0x4C78` | MWV207REG_PCZ_CACHE_BYTES_FETCHED | PCZ | 1 |  | jmgpu_discovery.h:846 |
| `0x13200` | `0x4C80` | MWV207REG_PCZ_CACHE_BYTES_WRITTEN | PCZ | 1 |  | jmgpu_discovery.h:865 |
| `0x13220` | `0x4C88` | MWV207REG_PCHZ_BYTES_WRITTEN | PCHZ | 1 |  | jmgpu_discovery.h:884 |
| `0x13240` | `0x4C90` | MWV207REG_PC_TEXEL_REQUESTS | PC | 1 |  | jmgpu_discovery.h:903 |
| `0x13244` | `0x4C91` | MWV207REG_PC_BILINEAR_REQUESTS | PC | 1 |  | jmgpu_discovery.h:922 |
| `0x13248` | `0x4C92` | MWV207REG_PC_TRILINEAR_REQUESTS | PC | 1 |  | jmgpu_discovery.h:941 |
| `0x1324C` | `0x4C93` | MWV207REG_PC_DISCARDED_REQUESTS | PC | 1 |  | jmgpu_discovery.h:960 |
| `0x13250` | `0x4C94` | MWV207REG_PC_VERTEX_INSTRUCTIONS | PC | 1 |  | jmgpu_discovery.h:1036 |
| `0x13254` | `0x4C95` | MWV207REG_PC_VERTEX_BRANCHES | PC | 1 |  | jmgpu_discovery.h:1055 |
| `0x13258` | `0x4C96` | MWV207REG_PC_VERTEX_TEXTURES | PC | 1 |  | jmgpu_discovery.h:1074 |
| `0x1325C` | `0x4C97` | MWV207REG_PC_VERTICES_RENDERED | PC | 1 |  | jmgpu_discovery.h:1093 |
| `0x13260` | `0x4C98` | MWV207REG_PCTX_CACHE_REQUESTS | PCTX | 1 |  | jmgpu_discovery.h:979 |
| `0x13280` | `0x4CA0` | MWV207REG_PCTX_CACHE_MISSES | PCTX | 1 |  | jmgpu_discovery.h:998 |
| `0x132A0` | `0x4CA8` | MWV207REG_PCTX_BYTES_FETCHED | PCTX | 1 |  | jmgpu_discovery.h:1017 |
| `0x132C0` | `0x4CB0` | MWV207REG_PC_PIXEL_INSTRUCTIONS | PC | 1 |  | jmgpu_discovery.h:1112 |
| `0x132C4` | `0x4CB1` | MWV207REG_PC_PIXEL_BRANCHES | PC | 1 |  | jmgpu_discovery.h:1131 |
| `0x132C8` | `0x4CB2` | MWV207REG_PC_PIXEL_TEXTURES | PC | 1 |  | jmgpu_discovery.h:1150 |
| `0x132CC` | `0x4CB3` | MWV207REG_PC_PIXELS_RENDERED | PC | 1 |  | jmgpu_discovery.h:1169 |
| `0x132D0` | `0x4CB4` | MWV207REG_PC_LOAD_INSTRUCTIONS | PC | 1 |  | jmgpu_discovery.h:1188 |
| `0x132D4` | `0x4CB5` | MWV207REG_PC_STORE_INSTRUCTIONS | PC | 1 |  | jmgpu_discovery.h:1207 |
| `0x132D8` | `0x4CB6` | MWV207REG_PCL1_LOAD_REQUESTS | PCL1 | 1 |  | jmgpu_discovery.h:1226 |
| `0x132DC` | `0x4CB7` | MWV207REG_PCL1_STORE_REQUESTS | PCL1 | 1 |  | jmgpu_discovery.h:1245 |
| `0x132E0` | `0x4CB8` | MWV207REG_PCL1_MISSES | PCL1 | 1 |  | jmgpu_discovery.h:1264 |
| `0x132E4` | `0x4CB9` | MWV207REG_PCL1_BYTES_FETCHED | PCL1 | 1 |  | jmgpu_discovery.h:1283 |
| `0x132E8` | `0x4CBA` | MWV207REG_PCL1_BYTES_WRITTEN | PCL1 | 1 |  | jmgpu_discovery.h:1302 |
| `0x13800` | `0x4E00` | MWV207REG_DE_DEGAMMA_LOOKUP | DE | 3 |  | jmgpu_interview.h:7693 |
| `0x13C00` | `0x4F00` | MWV207REG_DE_GAMMA_LOOKUP | DE | 3 |  | jmgpu_interview.h:7722 |
| `0x14000` | `0x5000` | MWV207REG_BLT_SRC_ADDRESS | BLT | 1 |  | jmgpu_medial.h:29 |
| `0x14008` | `0x5002` | MWV207REG_BLT_SRC_CONFIG | BLT | 6 |  | jmgpu_medial.h:48 |
| `0x1400C` | `0x5003` | MWV207REG_BLT_SRC_CONFIG_EX | BLT | 18 |  | jmgpu_medial.h:159 |
| `0x14010` | `0x5004` | MWV207REG_BLT_SRC_TILE_STATUS_ADDRESS | BLT | 1 |  | jmgpu_medial.h:338 |
| `0x14014` | `0x5005` | MWV207REG_BLT_SRC_ORIGIN | BLT | 2 |  | jmgpu_medial.h:357 |
| `0x14018` | `0x5006` | MWV207REG_BLT_DEST_ADDRESS | BLT | 1 |  | jmgpu_medial.h:383 |
| `0x14020` | `0x5008` | MWV207REG_BLT_DEST_TILE_STATUS_ADDRESS | BLT | 1 |  | jmgpu_medial.h:402 |
| `0x14024` | `0x5009` | MWV207REG_BLT_DEST_CONFIG | BLT | 6 |  | jmgpu_medial.h:421 |
| `0x14028` | `0x500A` | MWV207REG_BLT_DEST_CONFIG_EX | BLT | 23 |  | jmgpu_medial.h:532 |
| `0x1402C` | `0x500B` | MWV207REG_BLT_DEST_ORIGIN | BLT | 2 |  | jmgpu_medial.h:753 |
| `0x14030` | `0x500C` | MWV207REG_BLT_WINDOW_SIZE | BLT | 2 |  | jmgpu_medial.h:779 |
| `0x14034` | `0x500D` | MWV207REG_BLT_SRC_CLEAR_VALUE | BLT | 1 |  | jmgpu_medial.h:805 |
| `0x14038` | `0x500E` | MWV207REG_BLT_SRC_CLEAR_VALUE64 | BLT | 1 |  | jmgpu_medial.h:825 |
| `0x1403C` | `0x500F` | MWV207REG_BLT_DST_CLEAR_VALUE | BLT | 1 |  | jmgpu_medial.h:844 |
| `0x14040` | `0x5010` | MWV207REG_BLT_DST_CLEAR_VALUE64 | BLT | 1 |  | jmgpu_medial.h:864 |
| `0x14044` | `0x5011` | MWV207REG_BLT_NEW_CLEAR_VALUE | BLT | 1 |  | jmgpu_medial.h:883 |
| `0x14048` | `0x5012` | MWV207REG_BLT_NEW_CLEAR_VALUE64 | BLT | 1 |  | jmgpu_medial.h:903 |
| `0x1404C` | `0x5013` | MWV207REG_BLT_CLEAR_MASK | BLT | 1 |  | jmgpu_medial.h:922 |
| `0x14050` | `0x5014` | MWV207REG_BLT_CLEAR_MASK64 | BLT | 1 |  | jmgpu_medial.h:942 |
| `0x14054` | `0x5015` | MWV207REG_BLT_COPY_SIZE | BLT | 1 |  | jmgpu_medial.h:1080 |
| `0x14058` | `0x5016` | MWV207REG_BLT_DITHER_LOW | BLT | 8 |  | jmgpu_medial.h:962 |
| `0x1405C` | `0x5017` | MWV207REG_BLT_DITHER_HIGH | BLT | 8 |  | jmgpu_medial.h:1021 |
| `0x14060` | `0x5018` | MWV207REG_BLT_EXECUTE | BLT | 7 |  | jmgpu_medial.h:1100 |
| `0x14064` | `0x5019` | MWV207REG_BLT_GENERAL_CONFIG | BLT | 17 |  | jmgpu_medial.h:1248 |
| `0x14068` | `0x501A` | MWV207REG_BLT_TILE_FILLER_COUNT | BLT | 1 |  | jmgpu_medial.h:1425 |
| `0x1406C` | `0x501B` | MWV207REG_BLT_YUV_TILER_CONFIG | BLT | 6 |  | jmgpu_medial.h:1445 |
| `0x14070` | `0x501C` | MWV207REG_BLT_YUV_TILER_SIZE | BLT | 2 |  | jmgpu_medial.h:1511 |
| `0x14074` | `0x501D` | MWV207REG_BLT_YUV_TILER_YBASE | BLT | 1 |  | jmgpu_medial.h:1535 |
| `0x14078` | `0x501E` | MWV207REG_BLT_YUV_TILER_YSTRIDE | BLT | 1 |  | jmgpu_medial.h:1554 |
| `0x1407C` | `0x501F` | MWV207REG_BLT_YUV_TILER_UBASE | BLT | 1 |  | jmgpu_medial.h:1573 |
| `0x14080` | `0x5020` | MWV207REG_BLT_YUV_TILER_USTRIDE | BLT | 1 |  | jmgpu_medial.h:1592 |
| `0x14084` | `0x5021` | MWV207REG_BLT_YUV_TILER_VBASE | BLT | 1 |  | jmgpu_medial.h:1611 |
| `0x14088` | `0x5022` | MWV207REG_BLT_YUV_TILER_VSTRIDE | BLT | 1 |  | jmgpu_medial.h:1630 |
| `0x1408C` | `0x5023` | MWV207REG_BLT_YUV_TILER_DEST_BASE | BLT | 1 |  | jmgpu_medial.h:1649 |
| `0x14090` | `0x5024` | MWV207REG_BLT_YUV_TILER_DEST_STRIDE | BLT | 1 |  | jmgpu_medial.h:1668 |
| `0x14094` | `0x5025` | MWV207REG_BLT_SRC_SURFACE_SIZE | BLT | 2 |  | jmgpu_medial.h:1687 |
| `0x14098` | `0x5026` | MWV207REG_BLT_SRC_ROT_CONFIG | BLT | 6 |  | jmgpu_medial.h:1713 |
| `0x1409C` | `0x5027` | MWV207REG_BLT_BLOCK_SIZE | BLT | 2 |  | jmgpu_medial.h:1773 |
| `0x140A0` | `0x5028` | MWV207REG_BLT_TILE_SIZE | BLT | 2 |  | jmgpu_medial.h:1799 |
| `0x140A4` | `0x5029` | MWV207REG_BLT_FENCE_ADDRESS | BLT | 2 |  | jmgpu_medial.h:1825 |
| `0x140A8` | `0x502A` | MWV207REG_BLT_FENCE_DATA | BLT | 1 |  | jmgpu_medial.h:1850 |
| `0x140AC` | `0x502B` | MWV207REG_BLT_CACHE_FLUSH | BLT | 2 |  | jmgpu_medial.h:1870 |
| `0x140B0` | `0x502C` | MWV207REG_BLT_LOD_CONFIG | BLT | 4 |  | jmgpu_medial.h:1939 |
| `0x140B4` | `0x502D` | MWV207REG_BLT_FENCE_DATA_HIGH | BLT | 1 |  | jmgpu_medial.h:1978 |
| `0x140B8` | `0x502E` | MWV207REG_BLT_GENERAL_CONTROL | BLT | 1 |  | jmgpu_medial.h:1997 |
| `0x140BC` | `0x502F` | MWV207REG_BLT_SWIZZLE_EX | BLT | 8 |  | jmgpu_medial.h:2018 |
| `0x140C0` | `0x5030` | MWV207REG_BLT_DEST_LOD_ADDRESS | BLT | 1 |  | jmgpu_medial.h:1900 |
| `0x140F4` | `0x503D` | MWV207REG_BLT_SRC_BUFFER_END_ADDRESS | BLT | 1 |  | jmgpu_medial.h:2128 |
| `0x140F8` | `0x503E` | MWV207REG_BLT_SRC_UBUFFER_END_ADDRESS | BLT | 1 |  | jmgpu_medial.h:2147 |
| `0x140FC` | `0x503F` | MWV207REG_BLT_SRC_VBUFFER_END_ADDRESS | BLT | 1 |  | jmgpu_medial.h:2166 |
| `0x14100` | `0x5040` | MWV207REG_BLT_GAMMA_TABLE | BLT | 4 |  | jmgpu_medial.h:1172 |
| `0x14200` | `0x5080` | MWV207REG_BLT_DEGAMMA_TABLE | BLT | 4 |  | jmgpu_medial.h:1210 |
| `0x14300` | `0x50C0` | MWV207REG_BLT_DEST_LOD_STRIDE | BLT | 1 |  | jmgpu_medial.h:1919 |
| `0x14334` | `0x50CD` | MWV207REG_BLT_DST_BUFFER_END_ADDRESS | BLT | 1 |  | jmgpu_medial.h:2185 |
| `0x14338` | `0x50CE` | MWV207REG_BLT_CLUSTER_CONTROL | BLT | 1 |  | jmgpu_medial.h:2223 |
| `0x1433C` | `0x50CF` | MWV207REG_BLT_DEST_FLIP_SIZE | BLT | 2 |  | jmgpu_medial.h:2243 |
| `0x14340` | `0x50D0` | MWV207REG_BLT_DEST_LOD_END_ADDRESS | BLT | 1 |  | jmgpu_medial.h:2204 |
| `0x14374` | `0x50DD` | MWV207REG_BLT_SRC_CLEAR_VALUE96 | BLT | 1 |  | jmgpu_medial.h:2269 |
| `0x14378` | `0x50DE` | MWV207REG_BLT_SRC_CLEAR_VALUE128 | BLT | 1 |  | jmgpu_medial.h:2289 |
| `0x1437C` | `0x50DF` | MWV207REG_BLT_DST_CLEAR_VALUE96 | BLT | 1 |  | jmgpu_medial.h:2308 |
| `0x14380` | `0x50E0` | MWV207REG_BLT_DST_CLEAR_VALUE128 | BLT | 1 |  | jmgpu_medial.h:2328 |
| `0x14384` | `0x50E1` | MWV207REG_BLT_NEW_CLEAR_VALUE96 | BLT | 1 |  | jmgpu_medial.h:2347 |
| `0x14388` | `0x50E2` | MWV207REG_BLT_NEW_CLEAR_VALUE128 | BLT | 1 |  | jmgpu_medial.h:2367 |
| `0x1438C` | `0x50E3` | MWV207REG_BLT_CLEAR_MASK96 | BLT | 1 |  | jmgpu_medial.h:2386 |
| `0x14390` | `0x50E4` | MWV207REG_BLT_CLEAR_MASK128 | BLT | 1 |  | jmgpu_medial.h:2406 |
| `0x14394` | `0x50E5` | MWV207REG_BLT_DEST_UPLANE_ADDRESS | BLT | 1 |  | jmgpu_medial.h:2426 |
| `0x14398` | `0x50E6` | MWV207REG_BLT_DEST_UPLANE_CONFIG | BLT | 1 |  | jmgpu_medial.h:2445 |
| `0x1439C` | `0x50E7` | MWV207REG_BLT_DEST_UPLANE_END_ADDRESS | BLT | 1 |  | jmgpu_medial.h:2464 |
| `0x143A0` | `0x50E8` | MWV207REG_BLT_DEST_VPLANE_ADDRESS | BLT | 1 |  | jmgpu_medial.h:2483 |
| `0x143A4` | `0x50E9` | MWV207REG_BLT_DEST_VPLANE_CONFIG | BLT | 1 |  | jmgpu_medial.h:2502 |
| `0x143A8` | `0x50EA` | MWV207REG_BLT_DEST_VPLANE_END_ADDRESS | BLT | 1 |  | jmgpu_medial.h:2521 |
| `0x143AC` | `0x50EB` | MWV207REG_BLT_QOS_CONFIG | BLT | 6 |  | jmgpu_medial.h:2540 |
| `0x143B0` | `0x50EC` | MWV207REG_BLT_SRC1_ADDRESS | BLT | 1 |  | jmgpu_medial.h:2590 |
| `0x143B8` | `0x50EE` | MWV207REG_BLT_SRC1_CONFIG | BLT | 6 |  | jmgpu_medial.h:2609 |
| `0x143BC` | `0x50EF` | MWV207REG_BLT_SRC1_CONFIG_EX | BLT | 17 |  | jmgpu_medial.h:2720 |
| `0x143C0` | `0x50F0` | MWV207REG_BLT_SRC1_TILE_STATUS_ADDRESS | BLT | 1 |  | jmgpu_medial.h:2891 |
| `0x143C4` | `0x50F1` | MWV207REG_BLT_SRC1_ORIGIN | BLT | 2 |  | jmgpu_medial.h:2910 |
| `0x143C8` | `0x50F2` | MWV207REG_BLT_SRC1_BUFFER_END_ADDRESS | BLT | 1 |  | jmgpu_medial.h:2936 |
| `0x143CC` | `0x50F3` | MWV207REG_BLT_DEST1_TILE_STATUS_ADDRESS | BLT | 1 |  | jmgpu_medial.h:2974 |
| `0x143D0` | `0x50F4` | MWV207REG_BLT_DEST1_ADDRESS | BLT | 1 |  | jmgpu_medial.h:2955 |
| `0x143D8` | `0x50F6` | MWV207REG_BLT_DEST1_CONFIG | BLT | 6 |  | jmgpu_medial.h:2993 |
| `0x143DC` | `0x50F7` | MWV207REG_BLT_DEST1_CONFIG_EX | BLT | 23 |  | jmgpu_medial.h:3104 |
| `0x143E0` | `0x50F8` | MWV207REG_BLT_DEST1_ORIGIN | BLT | 2 |  | jmgpu_medial.h:3324 |
| `0x143E4` | `0x50F9` | MWV207REG_BLT_DEST1_BUFFER_END_ADDRESS | BLT | 1 |  | jmgpu_medial.h:3350 |
| `0x143E8` | `0x50FA` | MWV207REG_BLT_TILE_FILLER_CLUSTER_TS_OFFSET | BLT | 1 |  | jmgpu_medial.h:3369 |
| `0x143EC` | `0x50FB` | MWV207REG_BLT_SRC_HIGH_ADDRESS | BLT | 1 |  | jmgpu_medial.h:3389 |
| `0x143F0` | `0x50FC` | MWV207REG_BLT_SRC_BUFFER_HIGH_END_ADDRESS | BLT | 1 |  | jmgpu_medial.h:3408 |
| `0x143F4` | `0x50FD` | MWV207REG_BLT_DEST_HIGH_ADDRESS | BLT | 1 |  | jmgpu_medial.h:3427 |
| `0x143F8` | `0x50FE` | MWV207REG_BLT_DEST_BUFFER_HIGH_END_ADDRESS | BLT | 1 |  | jmgpu_medial.h:3446 |
| `0x143FC` | `0x50FF` | MWV207REG_BLT_COPY_HIGH_SIZE | BLT | 1 |  | jmgpu_medial.h:3462 |
| `0x14400` | `0x5100` | MWV207REG_SO_CONTROL | SO | 7 |  | jmgpu_dynamic.h:29 |
| `0x14404` | `0x5101` | MWV207REG_SO_COLLECT | SO | 2 |  | jmgpu_dynamic.h:123 |
| `0x14408` | `0x5102` | MWV207REG_SO_BIN_ORIGIN | SO | 2 |  | jmgpu_dynamic.h:178 |
| `0x1440C` | `0x5103` | MWV207REG_SO_BIN_SIZE | SO | 4 |  | jmgpu_dynamic.h:204 |
| `0x14410` | `0x5104` | MWV207REG_SO_STREAM_SIZE | SO | 4 |  | jmgpu_dynamic.h:290 |
| `0x14440` | `0x5110` | MWV207REG_SO_ADDRESS | SO | 1 |  | jmgpu_dynamic.h:158 |
| `0x14480` | `0x5120` | MWV207REG_SO_WINDOW_ORIGIN | SO | 2 |  | jmgpu_dynamic.h:239 |
| `0x144C0` | `0x5130` | MWV207REG_SO_WINDOW_SIZE | SO | 2 |  | jmgpu_dynamic.h:264 |
| `0x14600` | `0x5180` | MWV207REG_FE_STREAM_ADDRESS | FE | 2 |  | jmgpu_exclusive.h:1099 |
| `0x14640` | `0x5190` | MWV207REG_FE_STREAM_STRIDE | FE | 2 |  | jmgpu_exclusive.h:1125 |
| `0x14680` | `0x51A0` | MWV207REG_FE_STREAM_DIVISOR | FE | 1 |  | jmgpu_exclusive.h:1153 |
| `0x146C0` | `0x51B0` | MWV207REG_FE_STREAM_END_ADDRESS | FE | 1 |  | jmgpu_exclusive.h:1172 |
| `0x14800` | `0x5200` | MWV207REG_PE_RENDER_TARGET_ADDRESS_EX | PE | 2 |  | jmgpu_toolkit.h:2541 |
| `0x14900` | `0x5240` | MWV207REG_PE_RENDER_TARGET_CONFIG_EX | PE | 7 |  | jmgpu_toolkit.h:2567 |
| `0x14920` | `0x5248` | MWV207REG_PE_RENDER_TARGET_EXTRA_CONFIG_EX | PE | 8 |  | jmgpu_toolkit.h:2679 |
| `0x14940` | `0x5250` | MWV207REG_PE_RENDER_TARGET_ALPHA_TEST | PE | 3 |  | jmgpu_toolkit.h:2758 |
| `0x14960` | `0x5258` | MWV207REG_PE_RENDER_TARGET_ALPHA_CONTROL | PE | 15 |  | jmgpu_toolkit.h:2796 |
| `0x14980` | `0x5260` | MWV207REG_PE_RENDER_TARGET_BLEND_COLOR_LOW | PE | 2 |  | jmgpu_toolkit.h:2989 |
| `0x149A0` | `0x5268` | MWV207REG_PE_RENDER_TARGET_BLEND_COLOR_HIGH | PE | 2 |  | jmgpu_toolkit.h:3012 |
| `0x149C0` | `0x5270` | MWV207REG_PE_RENDER_TARGET_END_ADDRES_EX | PE | 1 |  | jmgpu_toolkit.h:3038 |
| `0x149E0` | `0x5278` | MWV207REG_PE_RENDER_TARGET_FORMAT_EX | PE | 6 |  | jmgpu_toolkit.h:3054 |
| `0x14A00` | `0x5280` | MWV207REG_TCS_START | TCS | 1 |  | jmgpu_domain.h:29 |
| `0x14A04` | `0x5281` | MWV207REG_TCS_RELATIVE_END | TCS | 1 |  | jmgpu_domain.h:49 |
| `0x14A08` | `0x5282` | MWV207REG_TCS_INSTRUCTION | TCS | 1 |  | jmgpu_domain.h:67 |
| `0x14A0C` | `0x5283` | MWV207REG_TCS_INSTRUCTION_PREFETCH | TCS | 1 |  | jmgpu_domain.h:87 |
| `0x14A10` | `0x5284` | MWV207REG_TCS_INSTRUCTION_PREFETCH_RELATIVE_END | TCS | 1 |  | jmgpu_domain.h:105 |
| `0x14A14` | `0x5285` | MWV207REG_TCS_CONTROL | TCS | 4 |  | jmgpu_domain.h:124 |
| `0x14A18` | `0x5286` | MWV207REG_TCS_THROTTLE | TCS | 3 |  | jmgpu_domain.h:197 |
| `0x14A1C` | `0x5287` | MWV207REG_TCS_ATTRIBUTE | TCS | 4 |  | jmgpu_domain.h:226 |
| `0x14A20` | `0x5288` | MWV207REG_TCS_OUTPUT | TCS | 4 |  | jmgpu_domain.h:163 |
| `0x14A40` | `0x5290` | MWV207REG_TCS_ATTRIBUTE_EX | TCS | 1 |  | jmgpu_domain.h:265 |
| `0x14A44` | `0x5291` | MWV207REG_TCS_UNIFORM_BASE | TCS | 1 |  | jmgpu_domain.h:284 |
| `0x14A48` | `0x5292` | MWV207REG_TCS_UNIFORM_LOCK | TCS | 1 |  | jmgpu_domain.h:303 |
| `0x14A4C` | `0x5293` | MWV207REG_TCS_SAMPLER_BASE | TCS | 2 |  | jmgpu_domain.h:323 |
| `0x14A50` | `0x5294` | MWV207REG_TCS_SAMPLER_LOCK | TCS | 2 |  | jmgpu_domain.h:348 |
| `0x14A54` | `0x5295` | MWV207REG_TCS_ATTRIBUTE_GLOBAL_CONTROL | TCS | 3 |  | jmgpu_domain.h:374 |
| `0x14B00` | `0x52C0` | MWV207REG_TPG_CONTROL | TPG | 6 |  | jmgpu_domain.h:417 |
| `0x14B04` | `0x52C1` | MWV207REG_TES_START | TES | 1 |  | jmgpu_domain.h:489 |
| `0x14B08` | `0x52C2` | MWV207REG_TES_RELATIVE_END | TES | 1 |  | jmgpu_domain.h:509 |
| `0x14B0C` | `0x52C3` | MWV207REG_TES_INSTRUCTION | TES | 1 |  | jmgpu_domain.h:527 |
| `0x14B10` | `0x52C4` | MWV207REG_TES_INSTRUCTION_PREFETCH | TES | 1 |  | jmgpu_domain.h:547 |
| `0x14B14` | `0x52C5` | MWV207REG_TES_INSTRUCTION_PREFETCH_RELATIVE_END | TES | 1 |  | jmgpu_domain.h:565 |
| `0x14B18` | `0x52C6` | MWV207REG_TES_CONTROL | TES | 3 |  | jmgpu_domain.h:584 |
| `0x14B1C` | `0x52C7` | MWV207REG_TES_THROTTLE | TES | 3 |  | jmgpu_domain.h:620 |
| `0x14B20` | `0x52C8` | MWV207REG_TES_ATTRIBUTE | TES | 2 |  | jmgpu_domain.h:649 |
| `0x14B24` | `0x52C9` | MWV207REG_TES_UNIFORM_BASE | TES | 1 |  | jmgpu_domain.h:711 |
| `0x14B28` | `0x52CA` | MWV207REG_TES_UNIFORM_LOCK | TES | 1 |  | jmgpu_domain.h:730 |
| `0x14B2C` | `0x52CB` | MWV207REG_TES_SAMPLER_BASE | TES | 2 |  | jmgpu_domain.h:750 |
| `0x14B30` | `0x52CC` | MWV207REG_TES_SAMPLER_LOCK | TES | 2 |  | jmgpu_domain.h:775 |
| `0x14B34` | `0x52CD` | MWV207REG_TES_THROTTLE_EX | TES | 2 |  | jmgpu_domain.h:801 |
| `0x14B38` | `0x52CE` | MWV207REG_TES_ATTRIBUTE_GLOBAL_CONTROL | TES | 3 |  | jmgpu_domain.h:826 |
| `0x14B40` | `0x52D0` | MWV207REG_TES_OUTPUT | TES | 4 |  | jmgpu_domain.h:673 |
| `0x14C00` | `0x5300` | MWV207REG_TX_GUARDBAND | TX | 4 |  | jmgpu_neighbor.h:1849 |
| `0x14C40` | `0x5310` | MWV207REG_TX_GLOBAL_CONFIG | TX | 1 |  | jmgpu_neighbor.h:1884 |
| `0x14C44` | `0x5311` | MWV207REG_TX_GPIPE_COMMAND | TX | 3 |  | jmgpu_neighbor.h:1906 |
| `0x14C48` | `0x5312` | MWV207REG_TX_PIXEL_COMMAND | TX | 3 |  | jmgpu_neighbor.h:1941 |
| `0x14C4C` | `0x5313` | MWV207REG_TX_GPIPE_INVALIDATE_TEXTURE_DESCRIPTOR | TX | 1 |  | jmgpu_neighbor.h:1976 |
| `0x14C50` | `0x5314` | MWV207REG_TX_PIXEL_INVALIDATE_TEXTURE_DESCRIPTOR | TX | 1 |  | jmgpu_neighbor.h:1996 |
| `0x14C54` | `0x5315` | MWV207REG_TEXTURE_DESCRIPTOR_ADDR_TABLE_ADDR | TEXTURE | 1 |  | jmgpu_neighbor.h:2016 |
| `0x14C58` | `0x5316` | MWV207REG_SAMPLER_ADDR_TABLE_ADDR | SAMPLER | 1 |  | jmgpu_neighbor.h:2036 |
| `0x14C5C` | `0x5317` | MWV207REG_SAMPLER_COMMAND | SAMPLER | 3 |  | jmgpu_neighbor.h:2056 |
| `0x14C60` | `0x5318` | MWV207REG_SAMPLER_INVALIDATE | SAMPLER | 1 |  | jmgpu_neighbor.h:2091 |
| `0x14C80` | `0x5320` | MWV207REG_TPC_GENERAL_CONTROL | TPC | 1 |  | jmgpu_autofit.h:26 |
| `0x14C84` | `0x5321` | MWV207REG_TPC_ENC_HDR_ADDR | TPC | 1 |  | jmgpu_autofit.h:45 |
| `0x14C88` | `0x5322` | MWV207REG_TPC_ENC_BODY_ADDR | TPC | 1 |  | jmgpu_autofit.h:62 |
| `0x14C8C` | `0x5323` | MWV207REG_TPC_ENC_FORMAT | TPC | 5 |  | jmgpu_autofit.h:79 |
| `0x14C90` | `0x5324` | MWV207REG_TPC_ENC_SIZE | TPC | 2 |  | jmgpu_autofit.h:121 |
| `0x14C94` | `0x5325` | MWV207REG_TPC_ENC_CONF | TPC | 2 |  | jmgpu_autofit.h:144 |
| `0x14C98` | `0x5326` | MWV207REG_TPC_DEC_CONFIG | TPC | 3 |  | jmgpu_autofit.h:165 |
| `0x14C9C` | `0x5327` | MWV207REG_TPC_SURFACE_CFG | TPC | 9 |  | jmgpu_autofit.h:193 |
| `0x14CA0` | `0x5328` | MWV207REG_TPC_DEC_HOST_INIT | TPC | 1 |  | jmgpu_autofit.h:258 |
| `0x14CA4` | `0x5329` | MWV207REG_TPC_SURFACE0_HDR_ADDR | TPC | 1 |  | jmgpu_autofit.h:274 |
| `0x14CA8` | `0x532A` | MWV207REG_TPC_SURFACE0_FORMAT | TPC | 3 |  | jmgpu_autofit.h:291 |
| `0x14CAC` | `0x532B` | MWV207REG_TPC_SURFACE1_HDR_ADDR | TPC | 1 |  | jmgpu_autofit.h:322 |
| `0x14CB0` | `0x532C` | MWV207REG_TPC_SURFACE1_FORMAT | TPC | 3 |  | jmgpu_autofit.h:339 |
| `0x14CB4` | `0x532D` | MWV207REG_TPC_SURFACE2_HDR_ADDR | TPC | 1 |  | jmgpu_autofit.h:370 |
| `0x14CB8` | `0x532E` | MWV207REG_TPC_SURFACE2_FORMAT | TPC | 3 |  | jmgpu_autofit.h:387 |
| `0x14CBC` | `0x532F` | MWV207REG_TPC_SURFACE3_HDR_ADDR | TPC | 1 |  | jmgpu_autofit.h:414 |
| `0x14CC0` | `0x5330` | MWV207REG_TPC_SURFACE3_FORMAT | TPC | 3 |  | jmgpu_autofit.h:431 |
| `0x14CC4` | `0x5331` | MWV207REG_TPC_SURFACE4_HDR_ADDR | TPC | 1 |  | jmgpu_autofit.h:462 |
| `0x14CC8` | `0x5332` | MWV207REG_TPC_SURFACE4_FORMAT | TPC | 3 |  | jmgpu_autofit.h:479 |
| `0x14CCC` | `0x5333` | MWV207REG_TPC_SURFACE5_HDR_ADDR | TPC | 1 |  | jmgpu_autofit.h:510 |
| `0x14CD0` | `0x5334` | MWV207REG_TPC_SURFACE5_FORMAT | TPC | 3 |  | jmgpu_autofit.h:527 |
| `0x14CD4` | `0x5335` | MWV207REG_TPC_SURFACE6_HDR_ADDR | TPC | 1 |  | jmgpu_autofit.h:558 |
| `0x14CD8` | `0x5336` | MWV207REG_TPC_SURFACE6_FORMAT | TPC | 3 |  | jmgpu_autofit.h:575 |
| `0x14CDC` | `0x5337` | MWV207REG_TPC_SURFACE7_HDR_ADDR | TPC | 1 |  | jmgpu_autofit.h:606 |
| `0x14CE0` | `0x5338` | MWV207REG_TPC_SURFACE7_FORMAT | TPC | 3 |  | jmgpu_autofit.h:623 |
| `0x14CE4` | `0x5339` | MWV207REG_TPC_SURFACE8_HDR_ADDR | TPC | 1 |  | jmgpu_autofit.h:654 |
| `0x14CE8` | `0x533A` | MWV207REG_TPC_SURFACE8_FORMAT | TPC | 3 |  | jmgpu_autofit.h:671 |
| `0x15000` | `0x5400` | MWV207REG_FE_POS_ONLY_ATTRIBUTES | FE | 9 |  | jmgpu_exclusive.h:1410 |
| `0x15040` | `0x5410` | MWV207REG_FE_POS_ONLY_STREAM_ADDRESS | FE | 2 |  | jmgpu_exclusive.h:1501 |
| `0x15080` | `0x5420` | MWV207REG_FE_POS_ONLY_STREAM_STRIDE | FE | 2 |  | jmgpu_exclusive.h:1527 |
| `0x150C0` | `0x5430` | MWV207REG_FE_POS_ONLY_STREAM_DIVISOR | FE | 1 |  | jmgpu_exclusive.h:1555 |
| `0x15100` | `0x5440` | MWV207REG_FE_POS_ONLY_GENERICS_X | FE | 1 |  | jmgpu_exclusive.h:1574 |
| `0x15140` | `0x5450` | MWV207REG_FE_POS_ONLY_GENERICS_Y | FE | 1 |  | jmgpu_exclusive.h:1594 |
| `0x15180` | `0x5460` | MWV207REG_FE_POS_ONLY_GENERICS_Z | FE | 1 |  | jmgpu_exclusive.h:1614 |
| `0x151C0` | `0x5470` | MWV207REG_FE_POS_ONLY_GENERICS_W | FE | 1 |  | jmgpu_exclusive.h:1634 |
| `0x15200` | `0x5480` | MWV207REG_FE_POS_ONLY_PACKING | FE | 1 |  | jmgpu_exclusive.h:1654 |
| `0x15400` | `0x5500` | MWV207REG_VS_POS_ONLY_START_PC | VS | 1 |  | jmgpu_announce.h:5116 |
| `0x15404` | `0x5501` | MWV207REG_VS_POS_ONLY_END_PC | VS | 1 |  | jmgpu_announce.h:5135 |
| `0x15408` | `0x5502` | MWV207REG_VS_POS_ONLY_INSTRUCTION | VS | 1 |  | jmgpu_announce.h:5155 |
| `0x1540C` | `0x5503` | MWV207REG_VS_POS_ONLY_PREFETCH | VS | 1 |  | jmgpu_announce.h:5174 |
| `0x15410` | `0x5504` | MWV207REG_VS_POS_ONLY_PREFETCH_END | VS | 1 |  | jmgpu_announce.h:5193 |
| `0x15414` | `0x5505` | MWV207REG_VS_POS_ONLY_CONTROL | VS | 5 |  | jmgpu_announce.h:5213 |
| `0x15418` | `0x5506` | MWV207REG_VS_POS_ONLY_INPUT_CONTROL | VS | 2 |  | jmgpu_announce.h:5264 |
| `0x1541C` | `0x5507` | MWV207REG_VS_POS_ONLY_UNPACK | VS | 16 |  | jmgpu_announce.h:5325 |
| `0x15420` | `0x5508` | MWV207REG_VS_POS_ONLY_INPUT | VS | 4 |  | jmgpu_announce.h:5288 |
| `0x15434` | `0x550D` | MWV207REG_VS_POS_ONLY_ATTRIBUTE_BASE | VS | 1 |  | jmgpu_announce.h:5434 |
| `0x15438` | `0x550E` | MWV207REG_VS_POS_ONLY_ATTRIBUTE_SIZE | VS | 2 |  | jmgpu_announce.h:5452 |
| `0x1543C` | `0x550F` | MWV207REG_VS_POS_ONLY_THROTTLE | VS | 3 |  | jmgpu_announce.h:5473 |
| `0x15600` | `0x5580` | MWV207REG_SHADER_MISC_CONFIG | SHADER | 7 |  | jmgpu_announce.h:5505 |
| `0x15604` | `0x5581` | MWV207REG_VS_INSTRUCTION_PREFETCH_RELATIVE_END | VS | 1 |  | jmgpu_announce.h:5576 |
| `0x15608` | `0x5582` | MWV207REG_VS_THROTTLE_EX | VS | 1 |  | jmgpu_announce.h:5595 |
| `0x1560C` | `0x5583` | MWV207REG_VS_BUDDY | VS | 1 |  | jmgpu_announce.h:5614 |
| `0x15610` | `0x5584` | MWV207REG_TCS_BUDDY | TCS | 1 |  | jmgpu_announce.h:5634 |
| `0x15614` | `0x5585` | MWV207REG_TES_BUDDY | TES | 1 |  | jmgpu_announce.h:5654 |
| `0x15618` | `0x5586` | MWV207REG_GS_BUDDY | GS | 1 |  | jmgpu_announce.h:5674 |
| `0x1561C` | `0x5587` | MWV207REG_PS_BUDDY | PS | 1 |  | jmgpu_announce.h:5694 |
| `0x15620` | `0x5588` | MWV207REG_VS_LS_ALLOC | VS | 1 |  | jmgpu_announce.h:5714 |
| `0x15624` | `0x5589` | MWV207REG_TCS_LS_ALLOC | TCS | 1 |  | jmgpu_announce.h:5733 |
| `0x15628` | `0x558A` | MWV207REG_TES_LS_ALLOC | TES | 1 |  | jmgpu_announce.h:5752 |
| `0x1562C` | `0x558B` | MWV207REG_GS_LS_ALLOC | GS | 1 |  | jmgpu_announce.h:5771 |
| `0x15630` | `0x558C` | MWV207REG_PS_LS_ALLOC | PS | 1 |  | jmgpu_announce.h:5790 |
| `0x15634` | `0x558D` | MWV207REG_VS_ATTRIBUTE_GLOBAL_CONTROL | VS | 3 |  | jmgpu_announce.h:5809 |
| `0x15800` | `0x5600` | MWV207REG_TX_GPIPE_TEXTURE_DESCRIPTOR_ADDR | TX | 1 |  | jmgpu_neighbor.h:2112 |
| `0x15A00` | `0x5680` | MWV207REG_TX_GPIPE_TEXTURE_CTRL | TX | 5 |  | jmgpu_neighbor.h:2132 |
| `0x15C00` | `0x5700` | MWV207REG_TX_PIXEL_TEXTURE_DESCRIPTOR_ADDR | TX | 1 |  | jmgpu_neighbor.h:2186 |
| `0x15E00` | `0x5780` | MWV207REG_TX_PIXEL_TEXTURE_CTRL | TX | 5 |  | jmgpu_neighbor.h:2206 |
| `0x16000` | `0x5800` | MWV207REG_TX_GPIPE_SAMPLER_CTRL0 | TX | 13 |  | jmgpu_neighbor.h:2260 |
| `0x16200` | `0x5880` | MWV207REG_TX_GPIPE_SAMPLER_CTRL1 | TX | 5 |  | jmgpu_neighbor.h:2403 |
| `0x16400` | `0x5900` | MWV207REG_TX_GPIPE_SAMPLER_LOD_MAX_MIN | TX | 2 |  | jmgpu_neighbor.h:2451 |
| `0x16600` | `0x5980` | MWV207REG_TX_GPIPE_SAMPLER_LOD_BIAS | TX | 2 |  | jmgpu_neighbor.h:2474 |
| `0x16800` | `0x5A00` | MWV207REG_TX_GPIPE_SAMPLER_ANISO_CTRL | TX | 1 |  | jmgpu_neighbor.h:2497 |
| `0x16C00` | `0x5B00` | MWV207REG_TX_PIXEL_SAMPLER_CTRL0 | TX | 13 |  | jmgpu_neighbor.h:2518 |
| `0x16E00` | `0x5B80` | MWV207REG_TX_PIXEL_SAMPLER_CTRL1 | TX | 5 |  | jmgpu_neighbor.h:2661 |
| `0x17000` | `0x5C00` | MWV207REG_TX_PIXEL_SAMPLER_LOD_MAX_MIN | TX | 2 |  | jmgpu_neighbor.h:2709 |
| `0x17200` | `0x5C80` | MWV207REG_TX_PIXEL_SAMPLER_LOD_BIAS | TX | 2 |  | jmgpu_neighbor.h:2732 |
| `0x17400` | `0x5D00` | MWV207REG_TX_PIXEL_SAMPLER_ANISO_CTRL | TX | 1 |  | jmgpu_neighbor.h:2755 |
| `0x17800` | `0x5E00` | MWV207REG_FE_ATTRIBUTE | FE | 8 |  | jmgpu_exclusive.h:1192 |
| `0x17880` | `0x5E20` | MWV207REG_FE_GENERIC_X | FE | 1 |  | jmgpu_exclusive.h:1307 |
| `0x17900` | `0x5E40` | MWV207REG_FE_GENERIC_Y | FE | 1 |  | jmgpu_exclusive.h:1326 |
| `0x17980` | `0x5E60` | MWV207REG_FE_GENERIC_Z | FE | 1 |  | jmgpu_exclusive.h:1345 |
| `0x17A00` | `0x5E80` | MWV207REG_FE_GENERIC_W | FE | 1 |  | jmgpu_exclusive.h:1364 |
| `0x17A80` | `0x5EA0` | MWV207REG_FE_FETCH | FE | 2 |  | jmgpu_exclusive.h:1383 |
| `0x17C00` | `0x5F00` | MWV207REG_PE_RENDER_TARGET_ADDRESS_EX2 | PE | 2 |  | jmgpu_toolkit.h:3154 |
| `0x17C40` | `0x5F10` | MWV207REG_PE_RENDER_TARGET_CONFIG_EX2 | PE | 6 |  | jmgpu_toolkit.h:3180 |
| `0x17C80` | `0x5F20` | MWV207REG_PE_RENDER_TARGET_EXTRA_CONFIG_EX2 | PE | 11 |  | jmgpu_toolkit.h:3284 |
| `0x17CC0` | `0x5F30` | MWV207REG_PE_RENDER_TARGET_ALPHA_TEST_EX | PE | 3 |  | jmgpu_toolkit.h:3403 |
| `0x17D00` | `0x5F40` | MWV207REG_PE_RENDER_TARGET_ALPHA_CONTROL_EX | PE | 15 |  | jmgpu_toolkit.h:3441 |
| `0x17D40` | `0x5F50` | MWV207REG_PE_RENDER_TARGET_END_ADDRES_EX2 | PE | 1 |  | jmgpu_toolkit.h:3683 |
| `0x17D80` | `0x5F60` | MWV207REG_PE_RENDER_TARGET_BLEND_COLOR_EX_LOW | PE | 2 |  | jmgpu_toolkit.h:3634 |
| `0x17DC0` | `0x5F70` | MWV207REG_PE_RENDER_TARGET_BLEND_COLOR_EX_HIGH | PE | 2 |  | jmgpu_toolkit.h:3657 |
| `0x17E40` | `0x5F90` | MWV207REG_ROI_BASE_Y | ROI | 1 |  | jmgpu_better.h:28 |
| `0x17E44` | `0x5F91` | MWV207REG_ROI_BASE_U | ROI | 1 |  | jmgpu_better.h:47 |
| `0x17E48` | `0x5F92` | MWV207REG_ROI_BASE_V | ROI | 1 |  | jmgpu_better.h:65 |
| `0x17E4C` | `0x5F93` | MWV207REG_ROI | ? | 2 |  | jmgpu_better.h:83 |
| `0x17E50` | `0x5F94` | MWV207REG_OUT_IMAGE_BASE_R | OUT | 1 |  | jmgpu_better.h:106 |
| `0x17E54` | `0x5F95` | MWV207REG_OUT_IMAGE_BASE_G | OUT | 1 |  | jmgpu_better.h:122 |
| `0x17E58` | `0x5F96` | MWV207REG_OUT_IMAGE_BASE_B | OUT | 1 |  | jmgpu_better.h:138 |
| `0x17E5C` | `0x5F97` | MWV207REG_OUT_IMAGE | OUT | 2 |  | jmgpu_better.h:154 |
| `0x17E60` | `0x5F98` | MWV207REG_SCALE_X | SCALE | 1 |  | jmgpu_better.h:175 |
| `0x17E64` | `0x5F99` | MWV207REG_SCALE_Y | SCALE | 1 |  | jmgpu_better.h:192 |
| `0x17E68` | `0x5F9A` | MWV207REG_IN_IMAGE_INIT_ERROR | IN | 2 |  | jmgpu_better.h:211 |
| `0x17E6C` | `0x5F9B` | MWV207REG_TRIGGER_SCALER | TRIGGER | 6 |  | jmgpu_better.h:232 |
| `0x17E70` | `0x5F9C` | MWV207REG_OUT_STANDING_REQUST | OUT | 1 |  | jmgpu_better.h:289 |
| `0x17E74` | `0x5F9D` | MWV207REG_YUV2_RGB_COEF0 | YUV2 | 3 |  | jmgpu_better.h:305 |
| `0x17E78` | `0x5F9E` | MWV207REG_YUV2_RGB_COEF1 | YUV2 | 2 |  | jmgpu_better.h:331 |
| `0x17E7C` | `0x5F9F` | MWV207REG_YUV2_RGB_COEF2 | YUV2 | 1 |  | jmgpu_better.h:355 |
| `0x17E80` | `0x5FA0` | MWV207REG_YUV2_RGB_COEF3 | YUV2 | 1 |  | jmgpu_better.h:374 |
| `0x17E84` | `0x5FA1` | MWV207REG_YUV2_RGB_COEF4 | YUV2 | 1 |  | jmgpu_better.h:390 |
| `0x17E88` | `0x5FA2` | MWV207REG_STRIDE | ? | 2 |  | jmgpu_better.h:409 |
| `0x17E8C` | `0x5FA3` | MWV207REG_IN_IMAGE | IN | 2 |  | jmgpu_better.h:432 |
| `0x17E90` | `0x5FA4` | MWV207REG_ROI_TL_COOR | ROI | 2 |  | jmgpu_better.h:456 |
| `0x17E94` | `0x5FA5` | MWV207REG_IN_IMAGE_INIT_ERROR_INT | IN | 2 |  | jmgpu_better.h:480 |
| `0x17E98` | `0x5FA6` | MWV207REG_CLAMP_R | CLAMP | 2 |  | jmgpu_better.h:505 |
| `0x17E9C` | `0x5FA7` | MWV207REG_CLAMP_G | CLAMP | 2 |  | jmgpu_better.h:529 |
| `0x17EA0` | `0x5FA8` | MWV207REG_CLAMP_B | CLAMP | 2 |  | jmgpu_better.h:553 |
| `0x17EC0` | `0x5FB0` | MWV207REG_TTD_MISC | TTD | 19 |  | jmgpu_transform.h:26 |
| `0x17EC4` | `0x5FB1` | MWV207REG_COMPRESSOR | ? | 6 |  | jmgpu_transform.h:136 |
| `0x17EC8` | `0x5FB2` | MWV207REG_DECOMPRESSOR | ? | 6 |  | jmgpu_transform.h:194 |
| `0x17ECC` | `0x5FB3` | MWV207REG_VIP_TENSOR_IO | VIP | 1 |  | jmgpu_transform.h:251 |
| `0x17ED0` | `0x5FB4` | MWV207REG_VIP_TENSOR_IO_WR | VIP | 1 |  | jmgpu_transform.h:269 |
| `0x17ED4` | `0x5FB5` | MWV207REG_PTN_GEN_BAYER_IMAGE | PTN | 2 |  | jmgpu_transform.h:288 |
| `0x17ED8` | `0x5FB6` | MWV207REG_PTN_GEN_RED | PTN | 2 |  | jmgpu_transform.h:311 |
| `0x17EDC` | `0x5FB7` | MWV207REG_PTN_GEN_GREENS | PTN | 2 |  | jmgpu_transform.h:334 |
| `0x17EE0` | `0x5FB8` | MWV207REG_PTN_GEN_BLUE | PTN | 2 |  | jmgpu_transform.h:357 |
| `0x17EE4` | `0x5FB9` | MWV207REG_PTN_GEN_INC_Y0 | PTN | 2 |  | jmgpu_transform.h:383 |
| `0x17EE8` | `0x5FBA` | MWV207REG_PTN_GEN_INC_Y1 | PTN | 1 |  | jmgpu_transform.h:405 |
| `0x17EEC` | `0x5FBB` | MWV207REG_CHECKSUM_EXPECTED_CHECKSUM_RED | CHECKSUM | 1 |  | jmgpu_transform.h:425 |
| `0x17EF0` | `0x5FBC` | MWV207REG_CHECKSUM_EXPECTED_CHECKSUM_GREEN | CHECKSUM | 1 |  | jmgpu_transform.h:444 |
| `0x17EF4` | `0x5FBD` | MWV207REG_CHECKSUM_EXPECTED_CHECKSUM_BLUE | CHECKSUM | 1 |  | jmgpu_transform.h:463 |
| `0x17EF8` | `0x5FBE` | MWV207REG_PATTERNS | ? | 6 |  | jmgpu_transform.h:482 |
| `0x17EFC` | `0x5FBF` | MWV207REG_SW_CONTROL_VXSBI0 | SW | 2 |  | jmgpu_transform.h:532 |
| `0x17F00` | `0x5FC0` | MWV207REG_DENOISE_POST_PROCESS_C0 | DENOISE | 1 |  | jmgpu_transform.h:562 |
| `0x17F04` | `0x5FC1` | MWV207REG_DENOISE_POST_PROCESS_C1 | DENOISE | 1 |  | jmgpu_transform.h:581 |
| `0x17F08` | `0x5FC2` | MWV207REG_DENOISE_POST_PROCESS_C2 | DENOISE | 1 |  | jmgpu_transform.h:600 |
| `0x17F0C` | `0x5FC3` | MWV207REG_DENOISE_POST_PROCESS_C3 | DENOISE | 1 |  | jmgpu_transform.h:619 |
| `0x17F10` | `0x5FC4` | MWV207REG_DENOISE_POST_PROCESS_NN_OUT_SCALE | DENOISE | 1 |  | jmgpu_transform.h:637 |
| `0x17F14` | `0x5FC5` | MWV207REG_DENOISE_POST_PROCESS_CLAMP_MIN_MAX | DENOISE | 2 |  | jmgpu_transform.h:655 |
| `0x17F18` | `0x5FC6` | MWV207REG_XDEEP_SLEEP0 | XDEEP | 9 |  | jmgpu_transform.h:676 |
| `0x17F1C` | `0x5FC7` | MWV207REG_XDEEP_SLEEP1 | XDEEP | 4 |  | jmgpu_transform.h:766 |
| `0x17F20` | `0x5FC8` | MWV207REG_XDEEP_SLEEP2 | XDEEP | 1 |  | jmgpu_transform.h:813 |
| `0x17F24` | `0x5FC9` | MWV207REG_XCACHE | ? | 2 |  | jmgpu_transform.h:830 |
| `0x17F28` | `0x5FCA` | MWV207REG_VIP_DEBUG | VIP | 1 |  | jmgpu_transform.h:856 |
| `0x17F2C` | `0x5FCB` | MWV207REG_XVIP_DEBUG | XVIP | 1 |  | jmgpu_transform.h:875 |
| `0x17F30` | `0x5FCC` | MWV207REG_COMP_DECOMP | COMP | 2 |  | jmgpu_transform.h:894 |
| `0x17F40` | `0x5FD0` | MWV207REG_PE_BLEND_FACTOR_FP32_RED | PE | 1 |  | jmgpu_toolkit.h:3703 |
| `0x17F60` | `0x5FD8` | MWV207REG_PE_BLEND_FACTOR_FP32_GREEN | PE | 1 |  | jmgpu_toolkit.h:3723 |
| `0x17F80` | `0x5FE0` | MWV207REG_PE_BLEND_FACTOR_FP32_BLUE | PE | 1 |  | jmgpu_toolkit.h:3743 |
| `0x17FA0` | `0x5FE8` | MWV207REG_PE_BLEND_FACTOR_FP32_ALPHA | PE | 1 |  | jmgpu_toolkit.h:3763 |
| `0x18000` | `0x6000` | MWV207REG_DEC_READ_CONFIG | DEC | 10 |  | jmgpu_factory.h:29 |
| `0x18040` | `0x6010` | MWV207REG_DEC_WRITE_CONFIG | DEC | 7 |  | jmgpu_factory.h:168 |
| `0x18080` | `0x6020` | MWV207REG_DEC_READ_BUFFER_BASE | DEC | 1 |  | jmgpu_factory.h:274 |
| `0x180C0` | `0x6030` | MWV207REG_DEC_READ_CACHE_BASE | DEC | 1 |  | jmgpu_factory.h:293 |
| `0x18100` | `0x6040` | MWV207REG_DEC_WRITE_BUFFER_BASE | DEC | 1 |  | jmgpu_factory.h:312 |
| `0x18140` | `0x6050` | MWV207REG_DEC_WRITE_CACHE_BASE | DEC | 1 |  | jmgpu_factory.h:331 |
| `0x18180` | `0x6060` | MWV207REG_DEC_CONTROL | DEC | 17 |  | jmgpu_factory.h:350 |
| `0x18184` | `0x6061` | MWV207REG_DEC_INTR_ACKNOWLEDGE | DEC | 1 |  | jmgpu_factory.h:490 |
| `0x18188` | `0x6062` | MWV207REG_DEC_INTR_ENBL | DEC | 1 |  | jmgpu_factory.h:508 |
| `0x1818C` | `0x6063` | MWV207REG_DEC_TILE_STATUS_DEBUG | DEC | 1 |  | jmgpu_factory.h:527 |
| `0x18190` | `0x6064` | MWV207REG_DEC_ENCODER_DEBUG | DEC | 1 |  | jmgpu_factory.h:546 |
| `0x18194` | `0x6065` | MWV207REG_DEC_DECODER_DEBUG | DEC | 1 |  | jmgpu_factory.h:565 |
| `0x181D8` | `0x6076` | MWV207REG_DEC_DEBUG0 | DEC | 1 |  | jmgpu_factory.h:884 |
| `0x181DC` | `0x6077` | MWV207REG_DEC_DEBUG1 | DEC | 1 |  | jmgpu_factory.h:903 |
| `0x181E0` | `0x6078` | MWV207REG_DEC_DEBUG2 | DEC | 1 |  | jmgpu_factory.h:922 |
| `0x181E4` | `0x6079` | MWV207REG_DEC_DEBUG3 | DEC | 1 |  | jmgpu_factory.h:941 |
| `0x181E8` | `0x607A` | MWV207REG_DEC_CONTROL_EX | DEC | 1 |  | jmgpu_factory.h:1237 |
| `0x18200` | `0x6080` | MWV207REG_DEC_CLEAR | DEC | 1 |  | jmgpu_factory.h:960 |
| `0x18240` | `0x6090` | MWV207REG_DEC_READ_EX_CONFIG | DEC | 10 |  | jmgpu_factory.h:979 |
| `0x18280` | `0x60A0` | MWV207REG_DEC_READ_STRIDE | DEC | 1 |  | jmgpu_factory.h:1087 |
| `0x182C0` | `0x60B0` | MWV207REG_DEC_WRITE_EX_CONFIG | DEC | 10 |  | jmgpu_factory.h:1106 |
| `0x18300` | `0x60C0` | MWV207REG_DEC_WRITE_STRIDE | DEC | 1 |  | jmgpu_factory.h:1218 |
| `0x18340` | `0x60D0` | MWV207REG_DEC_READ_BUFFER_END | DEC | 1 |  | jmgpu_factory.h:1257 |
| `0x18380` | `0x60E0` | MWV207REG_DEC_WRITE_BUFFER_END | DEC | 1 |  | jmgpu_factory.h:1276 |
| `0x18400` | `0x6100` | MWV207REG_WD_CONFIG | WD | 4 |  | jmgpu_overflow.h:29 |
| `0x18404` | `0x6101` | MWV207REG_WD_STATIC_CONFIG | WD | 1 |  | jmgpu_overflow.h:69 |
| `0x185C0` | `0x6170` | MWV207REG_VERIF_PRIMITIVE_ID | VERIF | 1 |  | jmgpu_overflow.h:94 |
| `0x18600` | `0x6180` | MWV207REG_VIP_SRAM_IMAGE_STREAM_BASE_ADDRESS | VIP | 1 |  | jmgpu_available.h:26 |
| `0x18604` | `0x6181` | MWV207REG_VIP_SRAM_IMAGE_STREAM_BUFFER_SIZE | VIP | 1 |  | jmgpu_available.h:42 |
| `0x18608` | `0x6182` | MWV207REG_VIP_REMAP_START_ADDRESS | VIP | 1 |  | jmgpu_available.h:58 |
| `0x18800` | `0x6200` | MWV207REG_DEC400_CONTROL | DEC400 | 21 |  | jmgpu_tasking.h:29 |
| `0x18804` | `0x6201` | MWV207REG_DEC400_CONTROL_EX | DEC400 | 16 |  | jmgpu_tasking.h:204 |
| `0x18808` | `0x6202` | MWV207REG_DEC400_CONTROL_EX2 | DEC400 | 8 |  | jmgpu_tasking.h:341 |
| `0x1880C` | `0x6203` | MWV207REG_DEC400_INTR_ENBL | DEC400 | 1 |  | jmgpu_tasking.h:408 |
| `0x18810` | `0x6204` | MWV207REG_DEC400_INTR_ENBL_EX | DEC400 | 1 |  | jmgpu_tasking.h:427 |
| `0x18814` | `0x6205` | MWV207REG_DEC400_INTR_ENBL_EX2 | DEC400 | 1 |  | jmgpu_tasking.h:446 |
| `0x18818` | `0x6206` | MWV207REG_DEC400_INTR_ACKNOWLEDGE | DEC400 | 1 |  | jmgpu_tasking.h:465 |
| `0x1881C` | `0x6207` | MWV207REG_DEC400_INTR_ACKNOWLEDGE_EX | DEC400 | 1 |  | jmgpu_tasking.h:483 |
| `0x18820` | `0x6208` | MWV207REG_DEC400_INTR_ACKNOWLEDGE_EX2 | DEC400 | 1 |  | jmgpu_tasking.h:501 |
| `0x18824` | `0x6209` | MWV207REG_DEC400_TILE_STATUS_DEBUG | DEC400 | 1 |  | jmgpu_tasking.h:520 |
| `0x18828` | `0x620A` | MWV207REG_DEC400_ENCODER_DEBUG | DEC400 | 1 |  | jmgpu_tasking.h:540 |
| `0x1882C` | `0x620B` | MWV207REG_DEC400_DECODER_DEBUG | DEC400 | 1 |  | jmgpu_tasking.h:560 |
| `0x18830` | `0x620C` | MWV207REG_DEC400_TOTAL_READS_IN | DEC400 | 1 |  | jmgpu_tasking.h:579 |
| `0x18834` | `0x620D` | MWV207REG_DEC400_TOTAL_WRITES_IN | DEC400 | 1 |  | jmgpu_tasking.h:599 |
| `0x18838` | `0x620E` | MWV207REG_DEC400_TOTAL_READ_BURSTS_IN | DEC400 | 1 |  | jmgpu_tasking.h:619 |
| `0x1883C` | `0x620F` | MWV207REG_DEC400_TOTAL_WRITE_BURSTS_IN | DEC400 | 1 |  | jmgpu_tasking.h:639 |
| `0x18840` | `0x6210` | MWV207REG_DEC400_TOTAL_READS_REQ_IN | DEC400 | 1 |  | jmgpu_tasking.h:659 |
| `0x18844` | `0x6211` | MWV207REG_DEC400_TOTAL_WRITES_REQ_IN | DEC400 | 1 |  | jmgpu_tasking.h:679 |
| `0x18848` | `0x6212` | MWV207REG_DEC400_TOTAL_READ_LASTS_IN | DEC400 | 1 |  | jmgpu_tasking.h:699 |
| `0x1884C` | `0x6213` | MWV207REG_DEC400_TOTAL_WRITE_LASTS_IN | DEC400 | 1 |  | jmgpu_tasking.h:719 |
| `0x18850` | `0x6214` | MWV207REG_DEC400_TOTAL_WRITE_RESPONSE_IN | DEC400 | 1 |  | jmgpu_tasking.h:739 |
| `0x18854` | `0x6215` | MWV207REG_DEC400_TOTAL_READS_OUT | DEC400 | 1 |  | jmgpu_tasking.h:759 |
| `0x18858` | `0x6216` | MWV207REG_DEC400_TOTAL_WRITES_OUT | DEC400 | 1 |  | jmgpu_tasking.h:779 |
| `0x1885C` | `0x6217` | MWV207REG_DEC400_TOTAL_READ_BURSTS_OUT | DEC400 | 1 |  | jmgpu_tasking.h:799 |
| `0x18860` | `0x6218` | MWV207REG_DEC400_TOTAL_WRITE_BURSTS_OUT | DEC400 | 1 |  | jmgpu_tasking.h:819 |
| `0x18864` | `0x6219` | MWV207REG_DEC400_TOTAL_READS_REQ_OUT | DEC400 | 1 |  | jmgpu_tasking.h:839 |
| `0x18868` | `0x621A` | MWV207REG_DEC400_TOTAL_WRITES_REQ_OUT | DEC400 | 1 |  | jmgpu_tasking.h:859 |
| `0x1886C` | `0x621B` | MWV207REG_DEC400_TOTAL_READ_LASTS_OUT | DEC400 | 1 |  | jmgpu_tasking.h:879 |
| `0x18870` | `0x621C` | MWV207REG_DEC400_TOTAL_WRITE_LASTS_OUT | DEC400 | 1 |  | jmgpu_tasking.h:899 |
| `0x18874` | `0x621D` | MWV207REG_DEC400_TOTAL_WRITE_RESPONSE_OUT | DEC400 | 1 |  | jmgpu_tasking.h:919 |
| `0x18878` | `0x621E` | MWV207REG_DEC400_STATUS | DEC400 | 3 |  | jmgpu_tasking.h:939 |
| `0x1887C` | `0x621F` | MWV207REG_DEC400_DEBUG_INFO_SELECT | DEC400 | 2 |  | jmgpu_tasking.h:969 |
| `0x18880` | `0x6220` | MWV207REG_DEC400_READ_CONFIG | DEC400 | 9 |  | jmgpu_tasking.h:994 |
| `0x18900` | `0x6240` | MWV207REG_DEC400_READ_EX_CONFIG | DEC400 | 10 |  | jmgpu_tasking.h:1154 |
| `0x18980` | `0x6260` | MWV207REG_DEC400_WRITE_CONFIG | DEC400 | 8 |  | jmgpu_tasking.h:1257 |
| `0x18A00` | `0x6280` | MWV207REG_DEC400_WRITE_EX_CONFIG | DEC400 | 10 |  | jmgpu_tasking.h:1405 |
| `0x18A80` | `0x62A0` | MWV207REG_DEC400_READ_BUFFER_BASE | DEC400 | 1 |  | jmgpu_tasking.h:1514 |
| `0x18B00` | `0x62C0` | MWV207REG_DEC400_READ_BUFFER_BASE_EX | DEC400 | 1 |  | jmgpu_tasking.h:1534 |
| `0x18B80` | `0x62E0` | MWV207REG_DEC400_READ_BUFFER_END | DEC400 | 1 |  | jmgpu_tasking.h:1553 |
| `0x18C00` | `0x6300` | MWV207REG_DEC400_READ_BUFFER_END_EX | DEC400 | 1 |  | jmgpu_tasking.h:1572 |
| `0x18C80` | `0x6320` | MWV207REG_DEC400_READ_FLUSH_CACHE | DEC400 | 1 |  | jmgpu_tasking.h:1592 |
| `0x18D00` | `0x6340` | MWV207REG_DEC400_READ_FLUSH_CACHE_EX | DEC400 | 1 |  | jmgpu_tasking.h:1611 |
| `0x18D80` | `0x6360` | MWV207REG_DEC400_WRITE_BUFFER_BASE | DEC400 | 1 |  | jmgpu_tasking.h:1630 |
| `0x18E00` | `0x6380` | MWV207REG_DEC400_WRITE_BUFFER_BASE_EX | DEC400 | 1 |  | jmgpu_tasking.h:1649 |
| `0x18E80` | `0x63A0` | MWV207REG_DEC400_WRITE_BUFFER_END | DEC400 | 1 |  | jmgpu_tasking.h:1668 |
| `0x18F00` | `0x63C0` | MWV207REG_DEC400_WRITE_BUFFER_END_EX | DEC400 | 1 |  | jmgpu_tasking.h:1687 |
| `0x18F80` | `0x63E0` | MWV207REG_DEC400_WRITE_FLUSH_CACHE | DEC400 | 1 |  | jmgpu_tasking.h:1706 |
| `0x19000` | `0x6400` | MWV207REG_DEC400_WRITE_FLUSH_CACHE_EX | DEC400 | 1 |  | jmgpu_tasking.h:1725 |
| `0x19080` | `0x6420` | MWV207REG_DEC400_READ_CACHE_BASE | DEC400 | 1 |  | jmgpu_tasking.h:1744 |
| `0x19100` | `0x6440` | MWV207REG_DEC400_READ_CACHE_BASE_EX | DEC400 | 1 |  | jmgpu_tasking.h:1763 |
| `0x19180` | `0x6460` | MWV207REG_DEC400_WRITE_CACHE_BASE | DEC400 | 1 |  | jmgpu_tasking.h:1782 |
| `0x19200` | `0x6480` | MWV207REG_DEC400_WRITE_CACHE_BASE_EX | DEC400 | 1 |  | jmgpu_tasking.h:1801 |
| `0x19280` | `0x64A0` | MWV207REG_DEC400_DEBUG_INFO_OUT | DEC400 | 1 |  | jmgpu_tasking.h:1821 |
| `0x19284` | `0x64A1` | MWV207REG_DEC400_DEBUG0 | DEC400 | 1 |  | jmgpu_tasking.h:1841 |
| `0x19288` | `0x64A2` | MWV207REG_DEC400_DEBUG1 | DEC400 | 1 |  | jmgpu_tasking.h:1861 |
| `0x1928C` | `0x64A3` | MWV207REG_DEC400_DEBUG2 | DEC400 | 1 |  | jmgpu_tasking.h:1881 |
| `0x19290` | `0x64A4` | MWV207REG_DEC400_DEBUG3 | DEC400 | 1 |  | jmgpu_tasking.h:1901 |
| `0x19294` | `0x64A5` | MWV207REG_DEC400_DEBUG4 | DEC400 | 1 |  | jmgpu_tasking.h:1921 |
| `0x19298` | `0x64A6` | MWV207REG_DEC400_DEBUG5 | DEC400 | 1 |  | jmgpu_tasking.h:1941 |
| `0x1929C` | `0x64A7` | MWV207REG_DEC400_STATE_COMMIT | DEC400 | 1 |  | jmgpu_tasking.h:1960 |
| `0x192A0` | `0x64A8` | MWV207REG_DEC400_DEBUG6 | DEC400 | 1 |  | jmgpu_tasking.h:1980 |
| `0x192A4` | `0x64A9` | MWV207REG_DEC400_DEBUG7 | DEC400 | 1 |  | jmgpu_tasking.h:2000 |
| `0x192A8` | `0x64AA` | MWV207REG_DEC400_CR_DEBUG_TILE128_TYPE0 | DEC400 | 1 |  | jmgpu_tasking.h:2019 |
| `0x192AC` | `0x64AB` | MWV207REG_DEC400_CR_DEBUG_TILE128_TYPE1 | DEC400 | 1 |  | jmgpu_tasking.h:2038 |
| `0x192B0` | `0x64AC` | MWV207REG_DEC400_CR_DEBUG_TILE128_TYPE2 | DEC400 | 1 |  | jmgpu_tasking.h:2057 |
| `0x192B4` | `0x64AD` | MWV207REG_DEC400_CR_DEBUG_TILE128_TYPE3 | DEC400 | 1 |  | jmgpu_tasking.h:2076 |
| `0x192B8` | `0x64AE` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE0 | DEC400 | 1 |  | jmgpu_tasking.h:2095 |
| `0x192BC` | `0x64AF` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE1 | DEC400 | 1 |  | jmgpu_tasking.h:2114 |
| `0x192C0` | `0x64B0` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE2 | DEC400 | 1 |  | jmgpu_tasking.h:2133 |
| `0x192C4` | `0x64B1` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE3 | DEC400 | 1 |  | jmgpu_tasking.h:2152 |
| `0x192C8` | `0x64B2` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE4 | DEC400 | 1 |  | jmgpu_tasking.h:2171 |
| `0x192CC` | `0x64B3` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE5 | DEC400 | 1 |  | jmgpu_tasking.h:2190 |
| `0x192D0` | `0x64B4` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE6 | DEC400 | 1 |  | jmgpu_tasking.h:2209 |
| `0x192D4` | `0x64B5` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE7 | DEC400 | 1 |  | jmgpu_tasking.h:2228 |
| `0x192D8` | `0x64B6` | MWV207REG_DEC400_DEBUG_READ_GATE_DOMAIN_CLK_COUNTER | DEC400 | 1 |  | jmgpu_tasking.h:2247 |
| `0x192DC` | `0x64B7` | MWV207REG_DEC400_DEBUG_WRITE_GATE_DOMAIN_CLK_COUNTER | DEC400 | 1 |  | jmgpu_tasking.h:2266 |
| `0x192E0` | `0x64B8` | MWV207REG_DEC400_DEBUG_OTHER_GATE_DOMAIN_CLK_COUNTER | DEC400 | 1 |  | jmgpu_tasking.h:2285 |
| `0x19300` | `0x64C0` | MWV207REG_DEC400_FAST_CLEAR_VALUE | DEC400 | 1 |  | jmgpu_tasking.h:2304 |
| `0x19380` | `0x64E0` | MWV207REG_DEC400_FAST_CLEAR_VALUE_EX | DEC400 | 1 |  | jmgpu_tasking.h:2323 |
| `0x19400` | `0x6500` | MWV207REG_DEC400_IMAGE_SIZE | DEC400 | 2 |  | jmgpu_tasking.h:2343 |
| `0x19800` | `0x6600` | MWV207REG_DE_ADJUST_SAT_GAMMA_LUT | DE | 1 |  | jmgpu_interview.h:7784 |
| `0x19C00` | `0x6700` | MWV207REG_DE_ADJUST_VALUE_GAMMA_LUT | DE | 1 |  | jmgpu_interview.h:7802 |
| `0x1C000` | `0x7000` | MWV207REG_TFB_CONTROL | TFB | 2 |  | jmgpu_dynamic.h:329 |
| `0x1C004` | `0x7001` | MWV207REG_TFB_COMMAND | TFB | 1 |  | jmgpu_dynamic.h:358 |
| `0x1C008` | `0x7002` | MWV207REG_TFB_HEADER | TFB | 1 |  | jmgpu_dynamic.h:386 |
| `0x1C00C` | `0x7003` | MWV207REG_TFB_INVALIDATE | TFB | 1 |  | jmgpu_dynamic.h:534 |
| `0x1C010` | `0x7004` | MWV207REG_TFB_QUERY_COMMAND | TFB | 3 |  | jmgpu_dynamic.h:555 |
| `0x1C014` | `0x7005` | MWV207REG_TFB_QUERY_HEADER | TFB | 1 |  | jmgpu_dynamic.h:599 |
| `0x1C040` | `0x7010` | MWV207REG_TFB_BUFFER_ADDRESS | TFB | 1 |  | jmgpu_dynamic.h:404 |
| `0x1C080` | `0x7020` | MWV207REG_TFB_BUFFER_SIZE | TFB | 1 |  | jmgpu_dynamic.h:422 |
| `0x1C0C0` | `0x7030` | MWV207REG_TFB_BUFFER_STRIDE | TFB | 1 |  | jmgpu_dynamic.h:441 |
| `0x1C100` | `0x7040` | MWV207REG_TFB_DESCRIPTOR_COUNT | TFB | 1 |  | jmgpu_dynamic.h:460 |
| `0x1C800` | `0x7200` | MWV207REG_TFB_DESCRIPTOR | TFB | 5 |  | jmgpu_dynamic.h:479 |
| `0x1D000` | `0x7400` | MWV207REG_DE_DEGAMMA_LOOKUP_EX | DE | 3 |  | jmgpu_interview.h:7752 |
| `0x1E000` | `0x7800` | MWV207REG_PS_TRANSFER_DESCRIPTOR0 | PS | 4 |  | jmgpu_announce.h:5849 |
| `0x1E080` | `0x7820` | MWV207REG_PS_TRANSFER_DESCRIPTOR1 | PS | 5 |  | jmgpu_announce.h:5885 |
| `0x1E100` | `0x7840` | MWV207REG_PS_TRANSFER_DESCRIPTOR2 | PS | 3 |  | jmgpu_announce.h:5926 |
| `0x1E180` | `0x7860` | MWV207REG_PS_TRANSFER_DESCRIPTOR3 | PS | 2 |  | jmgpu_announce.h:5955 |
| `0x1E200` | `0x7880` | MWV207REG_PS_TRANSFER_DESCRIPTOR4 | PS | 3 |  | jmgpu_announce.h:5978 |
| `0x1E280` | `0x78A0` | MWV207REG_PS_TRANSFER_DESCRIPTOR5 | PS | 3 |  | jmgpu_announce.h:6007 |
| `0x1E300` | `0x78C0` | MWV207REG_PS_TRANSFER_DESCRIPTOR6 | PS | 2 |  | jmgpu_announce.h:6036 |
| `0x1E380` | `0x78E0` | MWV207REG_PS_TRANSFER_DESCRIPTOR7 | PS | 1 |  | jmgpu_announce.h:6059 |
| `0x1E400` | `0x7900` | MWV207REG_PS_TRANSFER_DESCRIPTOR8 | PS | 1 |  | jmgpu_announce.h:6076 |
| `0x1E480` | `0x7920` | MWV207REG_PS_TRANSFER_DESCRIPTOR9 | PS | 1 |  | jmgpu_announce.h:6093 |
| `0x1E500` | `0x7940` | MWV207REG_PS_TRANSFER_DESCRIPTOR10 | PS | 3 |  | jmgpu_announce.h:6110 |
| `0x1E580` | `0x7960` | MWV207REG_PS_TRANSFER_DESCRIPTOR11 | PS | 1 |  | jmgpu_announce.h:6141 |
| `0x1E600` | `0x7980` | MWV207REG_PS_TRANSFER_DESCRIPTOR12 | PS | 1 |  | jmgpu_announce.h:6158 |
| `0x1E800` | `0x7A00` | MWV207REG_VIEWPORT_XSCALE_EX | VIEWPORT | 1 |  | jmgpu_nearest.h:1198 |
| `0x1E840` | `0x7A10` | MWV207REG_VIEWPORT_YSCALE_EX | VIEWPORT | 1 |  | jmgpu_nearest.h:1215 |
| `0x1E880` | `0x7A20` | MWV207REG_VIEWPORT_ZSCALE_EX | VIEWPORT | 1 |  | jmgpu_nearest.h:1232 |
| `0x1E8C0` | `0x7A30` | MWV207REG_VIEWPORT_XOFFSET_EX | VIEWPORT | 1 |  | jmgpu_nearest.h:1249 |
| `0x1E900` | `0x7A40` | MWV207REG_VIEWPORT_YOFFSET_EX | VIEWPORT | 1 |  | jmgpu_nearest.h:1266 |
| `0x1E940` | `0x7A50` | MWV207REG_VIEWPORT_ZOFFSET_EX | VIEWPORT | 1 |  | jmgpu_nearest.h:1283 |
| `0x1E980` | `0x7A60` | MWV207REG_PA_CLIP_WEX | PA | 1 |  | jmgpu_nearest.h:1303 |
| `0x1E9C0` | `0x7A70` | MWV207REG_DEPTH_NEAR_EX | DEPTH | 1 |  | jmgpu_toolkit.h:3781 |
| `0x1EA00` | `0x7A80` | MWV207REG_DEPTH_FAR_EX | DEPTH | 1 |  | jmgpu_toolkit.h:3797 |
| `0x20000` | `0x8000` | MWV207REG_SH_INSTRUCTIONS | SH | 1 |  | jmgpu_announce.h:3094 |
| `0x30000` | `0xC000` | MWV207REG_SH_UNIFORMS | SH | 1 |  | jmgpu_announce.h:3037 |
| `0x34000` | `0xD000` | MWV207REG_GPIPE_UNIFORMS | GPIPE | 1 |  | jmgpu_announce.h:3056 |
| `0x36000` | `0xD800` | MWV207REG_PIXEL_UNIFORMS | PIXEL | 1 |  | jmgpu_announce.h:3075 |

## 按块索引

每个块的寄存器按地址升序。`reg_id` 即驱动侧使用的寄存器号。

### `?` — 32 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0024` | `0x00090` | MWV207REG_CONTROL2 | 9 |  |
| `0x0121` | `0x00484` | MWV207REG_ENDIANNESS0 | 1 |  |
| `0x0122` | `0x00488` | MWV207REG_ENDIANNESS1 | 1 |  |
| `0x0123` | `0x0048C` | MWV207REG_ENDIANNESS2 | 1 |  |
| `0x0140` | `0x00500` | MWV207REG_HDISPLAY | 2 |  |
| `0x0142` | `0x00508` | MWV207REG_HSYNC | 4 |  |
| `0x0144` | `0x00510` | MWV207REG_HCOUNTER1 | 4 |  |
| `0x0146` | `0x00518` | MWV207REG_HCOUNTER2 | 4 |  |
| `0x0148` | `0x00520` | MWV207REG_VDISPLAY | 2 |  |
| `0x014A` | `0x00528` | MWV207REG_VSYNC | 4 |  |
| `0x0150` | `0x00540` | MWV207REG_CONTROL | 3 |  |
| `0x0151` | `0x00544` | MWV207REG_COUNTER0 | 1 |  |
| `0x0152` | `0x00548` | MWV207REG_COUNTER1 | 1 |  |
| `0x0153` | `0x0054C` | MWV207REG_COUNTER2 | 1 |  |
| `0x0154` | `0x00550` | MWV207REG_COUNTER3 | 1 |  |
| `0x0156` | `0x00558` | MWV207REG_CONTROL0 | 11 |  |
| `0x0157` | `0x0055C` | MWV207REG_CONTROL1 | 7 |  |
| `0x0157` | `0x0055C` | MWV207REG_RESERVED | 1 |  |
| `0x0158` | `0x00560` | MWV207REG_STATUS0 | 1 |  |
| `0x015E` | `0x00578` | MWV207REG_CONTROL3 | 8 |  |
| `0x0173` | `0x005CC` | MWV207REG_RESERVED1 | 1 |  |
| `0x0390` | `0x00E40` | MWV207REG_CENTROIDS | 8 |  |
| `0x0E07` | `0x0381C` | MWV207REG_VARYINGS | 1 |  |
| `0x0E14` | `0x03850` | MWV207REG_CONTEXT | 1 |  |
| `0x0E20` | `0x03880` | MWV207REG_PERFORMANCE | 4 |  |
| `0x0E30` | `0x038C0` | MWV207REG_SEMANTICS | 16 |  |
| `0x5F93` | `0x17E4C` | MWV207REG_ROI | 2 |  |
| `0x5FA2` | `0x17E88` | MWV207REG_STRIDE | 2 |  |
| `0x5FB1` | `0x17EC4` | MWV207REG_COMPRESSOR | 6 |  |
| `0x5FB2` | `0x17EC8` | MWV207REG_DECOMPRESSOR | 6 |  |
| `0x5FBE` | `0x17EF8` | MWV207REG_PATTERNS | 6 |  |
| `0x5FC9` | `0x17F24` | MWV207REG_XCACHE | 2 |  |

### `HI` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0026` | `0x00098` | MWV207REG_HI_CHIP_PATCH_REV | 1 |  |
| `0x0027` | `0x0009C` | MWV207REG_HI_CHIP_SPECS4 | 4 |  |
| `0x00EA` | `0x003A8` | MWV207REG_HI_AHB_CONTROL | 5 |  |

### `AHB` — 45 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0055` | `0x00154` | MWV207REG_AHB_POWER_CONTROL0 | 4 |  |
| `0x0056` | `0x00158` | MWV207REG_AHB_POWER_CONTROL1 | 3 |  |
| `0x0057` | `0x0015C` | MWV207REG_AHB_POWER_CONTROL2 | 1 |  |
| `0x0058` | `0x00160` | MWV207REG_AHB_POWER_CONTROL3 | 1 |  |
| `0x0059` | `0x00164` | MWV207REG_AHB_POWER_CONTROL4 | 2 |  |
| `0x005A` | `0x00168` | MWV207REG_AHB_POWER_CONTROL5 | 2 |  |
| `0x005B` | `0x0016C` | MWV207REG_AHB_POWER_CONTROL6 | 2 |  |
| `0x005C` | `0x00170` | MWV207REG_AHB_POWER_CONTROL7 | 3 |  |
| `0x005D` | `0x00174` | MWV207REG_AHB_POWER_CONTROL8 | 1 |  |
| `0x005E` | `0x00178` | MWV207REG_AHB_POWER_CONTROL9 | 1 |  |
| `0x005F` | `0x0017C` | MWV207REG_AHB_POWER_CONTROL10 | 1 |  |
| `0x00EC` | `0x003B0` | MWV207REG_AHB_CONTROL0 | 13 |  |
| `0x017D` | `0x005F4` | MWV207REG_AHB_FULL_PIPELINE_CONFIG | 1 |  |
| `0x0B13` | `0x02C4C` | MWV207REG_AHB_SBI_IN_PUREQ_TO_PUREQ_MAX_CYCLE | 1 |  |
| `0x0B14` | `0x02C50` | MWV207REG_AHB_SBI_IN_PUREQ_TO_PUREQ_TIMEOUT | 1 |  |
| `0x0B15` | `0x02C54` | MWV207REG_AHB_SBI_OUT_CUREQ_TO_CUREQ_MAX_CYCLE | 1 |  |
| `0x0B16` | `0x02C58` | MWV207REG_AHB_SBI_OUT_CUREQ_TO_CUREQ_TIMEOUT | 1 |  |
| `0x0B17` | `0x02C5C` | MWV207REG_AHB_SBI_IN_PRREQ_TO_PRACK_MAX_CYCLE | 1 |  |
| `0x0B18` | `0x02C60` | MWV207REG_AHB_SBI_IN_PRREQ_TO_PRACK_TIMEOUT | 1 |  |
| `0x0B19` | `0x02C64` | MWV207REG_AHB_SBI_OUT_CRREQ_TO_CRACK_MAX_CYCLE | 1 |  |
| `0x0B1A` | `0x02C68` | MWV207REG_AHB_SBI_OUT_CRREQ_TO_CRACK_TIMEOUT | 1 |  |
| `0x0B1B` | `0x02C6C` | MWV207REG_AHB_OCM_REMAP_START_ADDRESS | 1 |  |
| `0x0B1C` | `0x02C70` | MWV207REG_AHB_OCM_REMAP_END_ADDRESS | 1 |  |
| `0x0B20` | `0x02C80` | MWV207REG_AHB_PTN_GEN_BAYER_IMAGE | 2 |  |
| `0x0B21` | `0x02C84` | MWV207REG_AHB_PTN_GEN_RED | 2 |  |
| `0x0B22` | `0x02C88` | MWV207REG_AHB_PTN_GEN_GREENS | 2 |  |
| `0x0B23` | `0x02C8C` | MWV207REG_AHB_PTN_GEN_BLUE | 2 |  |
| `0x0B24` | `0x02C90` | MWV207REG_AHB_PTN_GEN_INC_Y0 | 2 |  |
| `0x0B25` | `0x02C94` | MWV207REG_AHB_PTN_GEN_INC_Y1 | 1 |  |
| `0x0B26` | `0x02C98` | MWV207REG_AHB_CHECKSUM_ACTUAL_CHECKSUM_RED | 1 |  |
| `0x0B27` | `0x02C9C` | MWV207REG_AHB_CHECKSUM_ACTUAL_CHECKSUM_GREEN | 1 |  |
| `0x0B28` | `0x02CA0` | MWV207REG_AHB_CHECKSUM_ACTUAL_CHECKSUM_BLUE | 1 |  |
| `0x0B29` | `0x02CA4` | MWV207REG_AHB_CHECKSUM_EXPECTED_CHECKSUM_RED | 1 |  |
| `0x0B2A` | `0x02CA8` | MWV207REG_AHB_CHECKSUM_EXPECTED_CHECKSUM_GREEN | 1 |  |
| `0x0B2B` | `0x02CAC` | MWV207REG_AHB_CHECKSUM_EXPECTED_CHECKSUM_BLUE | 1 |  |
| `0x0B2C` | `0x02CB0` | MWV207REG_AHB_PATTERNS | 6 |  |
| `0x0B57` | `0x02D5C` | MWV207REG_AHB_COMPRESSOR | 6 |  |
| `0x0B58` | `0x02D60` | MWV207REG_AHB_DECOMPRESSOR | 6 |  |
| `0x0B59` | `0x02D64` | MWV207REG_AHB_DENOISE_POST_PROCESS_C0 | 1 |  |
| `0x0B5A` | `0x02D68` | MWV207REG_AHB_DENOISE_POST_PROCESS_C1 | 1 |  |
| `0x0B5B` | `0x02D6C` | MWV207REG_AHB_DENOISE_POST_PROCESS_C2 | 1 |  |
| `0x0B5C` | `0x02D70` | MWV207REG_AHB_DENOISE_POST_PROCESS_C3 | 1 |  |
| `0x0B5D` | `0x02D74` | MWV207REG_AHB_DENOISE_POST_PROCESS_NN_OUT_SCALE | 1 |  |
| `0x0B5E` | `0x02D78` | MWV207REG_AHB_DENOISE_POST_PROCESS_CLAMP_MIN_MAX | 2 |  |
| `0x0B69` | `0x02DA4` | MWV207REG_AHB_COMP_DECOMP | 2 |  |

### `MMU` — 9 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0060` | `0x00180` | MWV207REG_MMU_SAFE_ADDRESS | 1 |  |
| `0x0061` | `0x00184` | MWV207REG_MMU_CONFIGURATION | 6 |  |
| `0x0062` | `0x00188` | MWV207REG_MMU_STATUS | 4 |  |
| `0x0063` | `0x0018C` | MWV207REG_MMU_CONTROL | 5 |  |
| `0x0064` | `0x00190` | MWV207REG_MMU_EXCEPTION | 1 |  |
| `0x0068` | `0x001A0` | MWV207REG_MMU_CONFIGURATION_EXT | 8 |  |
| `0x006A` | `0x001A8` | MWV207REG_MMU_BARRIER | 3 |  |
| `0x006B` | `0x001AC` | MWV207REG_MMU_CONFIG | 8 |  |
| `0x006C` | `0x001B0` | MWV207REG_MMU_DC_CONTROL | 4 |  |

### `MMUAHB` — 11 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x00E0` | `0x00380` | MWV207REG_MMUAHB_EXCEPTION | 1 |  |
| `0x00E1` | `0x00384` | MWV207REG_MMUAHB_STATUS | 8 |  |
| `0x00E2` | `0x00388` | MWV207REG_MMUAHB_CONTROL | 7 |  |
| `0x00E3` | `0x0038C` | MWV207REG_MMUAHB_TABLE_ARRAY_BASE_ADDRESS_LOW | 1 |  |
| `0x00E4` | `0x00390` | MWV207REG_MMUAHB_TABLE_ARRAY_BASE_ADDRESS_HIGH | 3 |  |
| `0x00E5` | `0x00394` | MWV207REG_MMUAHB_TABLE_ARRAY_SIZE | 1 |  |
| `0x00E6` | `0x00398` | MWV207REG_MMUAHB_SAFE_NON_SECURE_ADDRESS | 1 |  |
| `0x00E7` | `0x0039C` | MWV207REG_MMUAHB_SAFE_SECURE_ADDRESS | 1 |  |
| `0x00E8` | `0x003A0` | MWV207REG_MMUAHB_SAFE_ADDRESS_EXT | 6 |  |
| `0x00ED` | `0x003B4` | MWV207REG_MMUAHB_CONTEXT_PD_ENTRY0 | 4 |  |
| `0x00EE` | `0x003B8` | MWV207REG_MMUAHB_EXCEPTION_HIGH | 1 |  |

### `CMD` — 6 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x00E9` | `0x003A4` | MWV207REG_CMD_BUFFER_AHB_CTRL | 3 |  |
| `0x0E5E` | `0x03978` | MWV207REG_CMD_LOOP_NN_CMD_ITERATION | 1 |  |
| `0x0E5F` | `0x0397C` | MWV207REG_CMD_LOOP_NN_REGION_BASE_ADDR0 | 1 |  |
| `0x0E68` | `0x039A0` | MWV207REG_CMD_LOOP_NN_REGION_BASE_ADDR1 | 1 |  |
| `0x0E69` | `0x039A4` | MWV207REG_CMD_LOOP_NN_REGION_BASE_ADDR2 | 1 |  |
| `0x0E6A` | `0x039A8` | MWV207REG_CMD_LOOP_NN_REGION_BASE_ADDR3 | 1 |  |

### `AXI` — 4 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x00EB` | `0x003AC` | MWV207REG_AXI_AHB_CONFIG | 7 |  |
| `0x00F0` | `0x003C0` | MWV207REG_AXI_AHB_CONFIG_EX | 8 |  |
| `0x0E56` | `0x03958` | MWV207REG_AXI_SRAM_REMAP_VIRTUAL_START_ADDRESS | 1 |  |
| `0x0E57` | `0x0395C` | MWV207REG_AXI_SRAM_REMAP_VIRTUAL_END_ADDRESS | 1 |  |

### `DRAW` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0124` | `0x00490` | MWV207REG_DRAW_PRIMITIVE_START_TIME_STAMP | 1 |  |
| `0x0125` | `0x00494` | MWV207REG_DRAW_PRIMITIVE_END_TIME_STAMP | 1 |  |
| `0x0E27` | `0x0389C` | MWV207REG_DRAW_ID | 1 |  |

### `FRAME` — 4 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0124` | `0x00490` | MWV207REG_FRAME_BUFFER_CONFIG | 10 |  |
| `0x0126` | `0x00498` | MWV207REG_FRAME_BUFFER_ADDRESS | 1 |  |
| `0x0128` | `0x004A0` | MWV207REG_FRAME_BUFFER_STRIDE | 1 |  |
| `0x012A` | `0x004A8` | MWV207REG_FRAME_BUFFER_ORIGIN | 2 |  |

### `REQ` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0126` | `0x00498` | MWV207REG_REQ_BANK_ADDR_MASK | 1 |  |
| `0x0127` | `0x0049C` | MWV207REG_REQ_ROW_ADDR_MASK | 1 |  |
| `0x0128` | `0x004A0` | MWV207REG_REQ_WEIGHT | 1 |  |

### `OVERLAY` — 5 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x012C` | `0x004B0` | MWV207REG_OVERLAY_CONFIG | 6 |  |
| `0x012E` | `0x004B8` | MWV207REG_OVERLAY_ADDRESS | 1 |  |
| `0x0130` | `0x004C0` | MWV207REG_OVERLAY_STRIDE | 1 |  |
| `0x0132` | `0x004C8` | MWV207REG_OVERLAY_TL | 2 |  |
| `0x0134` | `0x004D0` | MWV207REG_OVERLAY_BR | 2 |  |

### `RD` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0130` | `0x004C0` | MWV207REG_RD_REQ_AGING_THRESH | 1 |  |

### `DISPLAY` — 6 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0136` | `0x004D8` | MWV207REG_DISPLAY_DITHER_CONFIG | 4 |  |
| `0x0138` | `0x004E0` | MWV207REG_DISPLAY_DITHER_TABLE_LOW | 8 |  |
| `0x013A` | `0x004E8` | MWV207REG_DISPLAY_DITHER_TABLE_HIGH | 8 |  |
| `0x014C` | `0x00530` | MWV207REG_DISPLAY_CURRENT_LOCATION | 2 |  |
| `0x0160` | `0x00580` | MWV207REG_DISPLAY_INTR | 5 |  |
| `0x0161` | `0x00584` | MWV207REG_DISPLAY_INTR_ENABLE | 2 |  |

### `PANEL` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x013C` | `0x004F0` | MWV207REG_PANEL_CONFIG | 11 |  |
| `0x013E` | `0x004F8` | MWV207REG_PANEL_TIMING | 8 |  |

### `WR` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0140` | `0x00500` | MWV207REG_WR_REQ_AGING_THRESH | 1 |  |

### `GAMMA` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x014E` | `0x00538` | MWV207REG_GAMMA_INDEX | 1 |  |
| `0x0150` | `0x00540` | MWV207REG_GAMMA_DATA | 3 |  |

### `CURSOR` — 5 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0152` | `0x00548` | MWV207REG_CURSOR_CONFIG | 5 |  |
| `0x0153` | `0x0054C` | MWV207REG_CURSOR_ADDRESS | 1 |  |
| `0x0154` | `0x00550` | MWV207REG_CURSOR_LOCATION | 2 |  |
| `0x0155` | `0x00554` | MWV207REG_CURSOR_BACKGROUND | 3 |  |
| `0x0156` | `0x00558` | MWV207REG_CURSOR_FOREGROUND | 3 |  |

### `CE` — 28 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0155` | `0x00554` | MWV207REG_CE_TRIGGER_ASYNC | 1 |  |
| `0x0C00` | `0x03000` | MWV207REG_CE_ORIGIN | 2 |  |
| `0x0C01` | `0x03004` | MWV207REG_CE_SIZE | 2 |  |
| `0x0C02` | `0x03008` | MWV207REG_CE_STATE_MEMORY | 1 |  |
| `0x0C03` | `0x0300C` | MWV207REG_CE_EXECUTE | 6 |  |
| `0x0C04` | `0x03010` | MWV207REG_CE_PROGRAM | 2 |  |
| `0x0C05` | `0x03014` | MWV207REG_CE_CONFIG | 5 |  |
| `0x0C06` | `0x03018` | MWV207REG_CE_COUNT | 4 |  |
| `0x0C07` | `0x0301C` | MWV207REG_CE_SWIZZLE | 5 |  |
| `0x0C08` | `0x03020` | MWV207REG_CE_ADDRESS | 1 |  |
| `0x0C10` | `0x03040` | MWV207REG_CE_DITHER_LOW | 8 |  |
| `0x0C11` | `0x03044` | MWV207REG_CE_DITHER_HIGH | 8 |  |
| `0x0C12` | `0x03048` | MWV207REG_CE_CONST | 1 |  |
| `0x0C13` | `0x0304C` | MWV207REG_CE_INSTRUCTION | 1 |  |
| `0x0C18` | `0x03060` | MWV207REG_CE_ATTRIBUTE_X | 1 |  |
| `0x0C20` | `0x03080` | MWV207REG_CE_ATTRIBUTE_XDX | 1 |  |
| `0x0C28` | `0x030A0` | MWV207REG_CE_ATTRIBUTE_XDY | 1 |  |
| `0x0C30` | `0x030C0` | MWV207REG_CE_ATTRIBUTE_Y | 1 |  |
| `0x0C38` | `0x030E0` | MWV207REG_CE_ATTRIBUTE_YDX | 1 |  |
| `0x0C40` | `0x03100` | MWV207REG_CE_ATTRIBUTE_YDY | 1 |  |
| `0x0C48` | `0x03120` | MWV207REG_CE_SAMPLER_CONFIG | 6 |  |
| `0x0C50` | `0x03140` | MWV207REG_CE_SAMPLER_SIZE | 4 |  |
| `0x0C58` | `0x03160` | MWV207REG_CE_BORDER_X | 3 |  |
| `0x0C60` | `0x03180` | MWV207REG_CE_BORDER_Y | 2 |  |
| `0x0C68` | `0x031A0` | MWV207REG_CE_TILE_STATUS_CONTROL | 2 |  |
| `0x0C70` | `0x031C0` | MWV207REG_CE_TILE_STATUS_BUFFER | 1 |  |
| `0x0C78` | `0x031E0` | MWV207REG_CE_TILE_STATUS_CLEAR | 1 |  |
| `0x0C80` | `0x03200` | MWV207REG_CE_SAMPLER_ADDRESS | 1 |  |

### `RESERVED` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0158` | `0x00560` | MWV207REG_RESERVED_GROUP | 1 |  |

### `DBI` — 6 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0162` | `0x00588` | MWV207REG_DBI_CONFIG | 6 |  |
| `0x0164` | `0x00590` | MWV207REG_DBI_IF_RESET | 1 |  |
| `0x0166` | `0x00598` | MWV207REG_DBI_WR_CHAR1 | 3 |  |
| `0x0168` | `0x005A0` | MWV207REG_DBI_WR_CHAR2 | 2 |  |
| `0x016A` | `0x005A8` | MWV207REG_DBI_CMD | 2 |  |
| `0x017E` | `0x005F8` | MWV207REG_DBI_TYPEC_CFG | 4 |  |

### `MEMORY` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0163` | `0x0058C` | MWV207REG_MEMORY_DIAGNOSE | 2 |  |

### `TIME` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0164` | `0x00590` | MWV207REG_TIME_STAMP_ADDRESS | 2 |  |
| `0x0165` | `0x00594` | MWV207REG_TIME_STAMP_TRIGGER | 3 |  |

### `DRAM` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0169` | `0x005A4` | MWV207REG_DRAM_CONTROL | 3 |  |

### `DPI` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x016C` | `0x005B0` | MWV207REG_DPI_CONFIG | 1 |  |

### `DC` — 12 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x016E` | `0x005B8` | MWV207REG_DC_CHIP_ID | 1 |  |
| `0x016F` | `0x005BC` | MWV207REG_DC_CHIP_REV | 1 |  |
| `0x0170` | `0x005C0` | MWV207REG_DC_CHIP_DATE | 1 |  |
| `0x0171` | `0x005C4` | MWV207REG_DC_CHIP_TIME | 1 |  |
| `0x0172` | `0x005C8` | MWV207REG_DC_CHIP_PATCH_REV | 1 |  |
| `0x0174` | `0x005D0` | MWV207REG_DC_TILE_IN_CFG | 4 |  |
| `0x0176` | `0x005D8` | MWV207REG_DC_TILE_UV_FRAME_BUFFER_ADR | 1 |  |
| `0x0178` | `0x005E0` | MWV207REG_DC_TILE_UV_FRAME_BUFFER_STR | 1 |  |
| `0x017A` | `0x005E8` | MWV207REG_DC_CHIP_INFO | 1 |  |
| `0x017B` | `0x005EC` | MWV207REG_DC_PRODUCT_ID | 1 |  |
| `0x017C` | `0x005F0` | MWV207REG_DC_ECO_ID | 1 |  |
| `0x0180` | `0x00600` | MWV207REG_DC_STATUS | 1 |  |

### `PROBE` — 6 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x017B` | `0x005EC` | MWV207REG_PROBE_AXI_PROBE_CTRL | 3 |  |
| `0x017C` | `0x005F0` | MWV207REG_PROBE_AXI_PROBE_MODULE_ENABLE | 23 |  |
| `0x0E1C` | `0x03870` | MWV207REG_PROBE_ADDRESS | 2 |  |
| `0x0E1D` | `0x03874` | MWV207REG_PROBE_DATA | 1 |  |
| `0x0E1E` | `0x03878` | MWV207REG_PROBE_SELECT | 3 |  |
| `0x0E55` | `0x03954` | MWV207REG_PROBE_SELECT_EXTEND | 3 |  |

### `DEBUG` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0182` | `0x00608` | MWV207REG_DEBUG_COUNTER_SELECT | 1 |  |
| `0x0184` | `0x00610` | MWV207REG_DEBUG_COUNTER_VALUE | 1 |  |

### `FEDE` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x019C` | `0x00670` | MWV207REG_FEDE_TIMEOUT | 1 |  |

### `FE` — 42 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x019D` | `0x00674` | MWV207REG_FE_PRIMITIVE_RESTART | 1 |  |
| `0x019E` | `0x00678` | MWV207REG_FE_INDEX_MIN | 1 |  |
| `0x019F` | `0x0067C` | MWV207REG_FE_INDEX_MAX | 1 |  |
| `0x01A0` | `0x00680` | MWV207REG_FE_VERTEX_ADDRESS | 2 |  |
| `0x01A8` | `0x006A0` | MWV207REG_FE_VERTEX_STRIDE | 3 |  |
| `0x01B0` | `0x006C0` | MWV207REG_FE_GENERICS_X | 1 |  |
| `0x01C0` | `0x00700` | MWV207REG_FE_GENERICS_Y | 1 |  |
| `0x01D0` | `0x00740` | MWV207REG_FE_GENERICS_Z | 1 |  |
| `0x01E0` | `0x00780` | MWV207REG_FE_GENERICS_W | 1 |  |
| `0x01F0` | `0x007C0` | MWV207REG_FE_PATCH_CONTROL | 1 |  |
| `0x01F1` | `0x007C4` | MWV207REG_FE_ID | 4 |  |
| `0x01F2` | `0x007C8` | MWV207REG_FE_RESET_ATTRIBUTES | 1 |  |
| `0x01F3` | `0x007CC` | MWV207REG_FE_COMPUTE_INDIRECT | 2 |  |
| `0x01F4` | `0x007D0` | MWV207REG_FE_PACKING | 16 |  |
| `0x01F6` | `0x007D8` | MWV207REG_FE_CONTROL | 2 |  |
| `0x01F7` | `0x007DC` | MWV207REG_FE_DESCRIPTOR_START_ADDRESS | 1 |  |
| `0x01F8` | `0x007E0` | MWV207REG_FE_DESCRIPTOR_END_ADDRESS | 1 |  |
| `0x01F9` | `0x007E4` | MWV207REG_FE_ASYNC_STATUS | 1 |  |
| `0x01FA` | `0x007E8` | MWV207REG_FE_FENCE_DATA | 1 |  |
| `0x01FB` | `0x007EC` | MWV207REG_FE_BASE_INSTANCE | 1 |  |
| `0x01FC` | `0x007F0` | MWV207REG_FE_ASYNC_CONFIG | 1 |  |
| `0x01FD` | `0x007F4` | MWV207REG_FE_FENCE_DATA_HIGH | 1 |  |
| `0x01FE` | `0x007F8` | MWV207REG_FE_INDEX_END_ADDRESS | 1 |  |
| `0x5180` | `0x14600` | MWV207REG_FE_STREAM_ADDRESS | 2 |  |
| `0x5190` | `0x14640` | MWV207REG_FE_STREAM_STRIDE | 2 |  |
| `0x51A0` | `0x14680` | MWV207REG_FE_STREAM_DIVISOR | 1 |  |
| `0x51B0` | `0x146C0` | MWV207REG_FE_STREAM_END_ADDRESS | 1 |  |
| `0x5400` | `0x15000` | MWV207REG_FE_POS_ONLY_ATTRIBUTES | 9 |  |
| `0x5410` | `0x15040` | MWV207REG_FE_POS_ONLY_STREAM_ADDRESS | 2 |  |
| `0x5420` | `0x15080` | MWV207REG_FE_POS_ONLY_STREAM_STRIDE | 2 |  |
| `0x5430` | `0x150C0` | MWV207REG_FE_POS_ONLY_STREAM_DIVISOR | 1 |  |
| `0x5440` | `0x15100` | MWV207REG_FE_POS_ONLY_GENERICS_X | 1 |  |
| `0x5450` | `0x15140` | MWV207REG_FE_POS_ONLY_GENERICS_Y | 1 |  |
| `0x5460` | `0x15180` | MWV207REG_FE_POS_ONLY_GENERICS_Z | 1 |  |
| `0x5470` | `0x151C0` | MWV207REG_FE_POS_ONLY_GENERICS_W | 1 |  |
| `0x5480` | `0x15200` | MWV207REG_FE_POS_ONLY_PACKING | 1 |  |
| `0x5E00` | `0x17800` | MWV207REG_FE_ATTRIBUTE | 8 |  |
| `0x5E20` | `0x17880` | MWV207REG_FE_GENERIC_X | 1 |  |
| `0x5E40` | `0x17900` | MWV207REG_FE_GENERIC_Y | 1 |  |
| `0x5E60` | `0x17980` | MWV207REG_FE_GENERIC_Z | 1 |  |
| `0x5E80` | `0x17A00` | MWV207REG_FE_GENERIC_W | 1 |  |
| `0x5EA0` | `0x17A80` | MWV207REG_FE_FETCH | 2 |  |

### `FA` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x01FF` | `0x007FC` | MWV207REG_FA_CONTROL | 1 |  |

### `AHBDEC` — 79 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0200` | `0x00800` | MWV207REG_AHBDEC_CONTROL | 21 |  |
| `0x0201` | `0x00804` | MWV207REG_AHBDEC_CONTROL_EX | 16 |  |
| `0x0202` | `0x00808` | MWV207REG_AHBDEC_CONTROL_EX2 | 8 |  |
| `0x0203` | `0x0080C` | MWV207REG_AHBDEC_INTR_ENBL | 1 |  |
| `0x0204` | `0x00810` | MWV207REG_AHBDEC_INTR_ENBL_EX | 1 |  |
| `0x0205` | `0x00814` | MWV207REG_AHBDEC_INTR_ENBL_EX2 | 1 |  |
| `0x0206` | `0x00818` | MWV207REG_AHBDEC_INTR_ACKNOWLEDGE | 1 |  |
| `0x0207` | `0x0081C` | MWV207REG_AHBDEC_INTR_ACKNOWLEDGE_EX | 1 |  |
| `0x0208` | `0x00820` | MWV207REG_AHBDEC_INTR_ACKNOWLEDGE_EX2 | 1 |  |
| `0x0209` | `0x00824` | MWV207REG_AHBDEC_TILE_STATUS_DEBUG | 1 |  |
| `0x020A` | `0x00828` | MWV207REG_AHBDEC_ENCODER_DEBUG | 1 |  |
| `0x020B` | `0x0082C` | MWV207REG_AHBDEC_DECODER_DEBUG | 1 |  |
| `0x020C` | `0x00830` | MWV207REG_AHBDEC_TOTAL_READS_IN | 1 |  |
| `0x020D` | `0x00834` | MWV207REG_AHBDEC_TOTAL_WRITES_IN | 1 |  |
| `0x020E` | `0x00838` | MWV207REG_AHBDEC_TOTAL_READ_BURSTS_IN | 1 |  |
| `0x020F` | `0x0083C` | MWV207REG_AHBDEC_TOTAL_WRITE_BURSTS_IN | 1 |  |
| `0x0210` | `0x00840` | MWV207REG_AHBDEC_TOTAL_READS_REQ_IN | 1 |  |
| `0x0211` | `0x00844` | MWV207REG_AHBDEC_TOTAL_WRITES_REQ_IN | 1 |  |
| `0x0212` | `0x00848` | MWV207REG_AHBDEC_TOTAL_READ_LASTS_IN | 1 |  |
| `0x0213` | `0x0084C` | MWV207REG_AHBDEC_TOTAL_WRITE_LASTS_IN | 1 |  |
| `0x0214` | `0x00850` | MWV207REG_AHBDEC_TOTAL_WRITE_RESPONSE_IN | 1 |  |
| `0x0215` | `0x00854` | MWV207REG_AHBDEC_TOTAL_READS_OUT | 1 |  |
| `0x0216` | `0x00858` | MWV207REG_AHBDEC_TOTAL_WRITES_OUT | 1 |  |
| `0x0217` | `0x0085C` | MWV207REG_AHBDEC_TOTAL_READ_BURSTS_OUT | 1 |  |
| `0x0218` | `0x00860` | MWV207REG_AHBDEC_TOTAL_WRITE_BURSTS_OUT | 1 |  |
| `0x0219` | `0x00864` | MWV207REG_AHBDEC_TOTAL_READS_REQ_OUT | 1 |  |
| `0x021A` | `0x00868` | MWV207REG_AHBDEC_TOTAL_WRITES_REQ_OUT | 1 |  |
| `0x021B` | `0x0086C` | MWV207REG_AHBDEC_TOTAL_READ_LASTS_OUT | 1 |  |
| `0x021C` | `0x00870` | MWV207REG_AHBDEC_TOTAL_WRITE_LASTS_OUT | 1 |  |
| `0x021D` | `0x00874` | MWV207REG_AHBDEC_TOTAL_WRITE_RESPONSE_OUT | 1 |  |
| `0x021E` | `0x00878` | MWV207REG_AHBDEC_STATUS | 3 |  |
| `0x021F` | `0x0087C` | MWV207REG_AHBDEC_DEBUG_INFO_SELECT | 2 |  |
| `0x0220` | `0x00880` | MWV207REG_AHBDEC_READ_CONFIG | 9 |  |
| `0x0240` | `0x00900` | MWV207REG_AHBDEC_READ_EX_CONFIG | 10 |  |
| `0x0260` | `0x00980` | MWV207REG_AHBDEC_WRITE_CONFIG | 8 |  |
| `0x0280` | `0x00A00` | MWV207REG_AHBDEC_WRITE_EX_CONFIG | 10 |  |
| `0x02A0` | `0x00A80` | MWV207REG_AHBDEC_READ_BUFFER_BASE | 1 |  |
| `0x02C0` | `0x00B00` | MWV207REG_AHBDEC_READ_BUFFER_BASE_EX | 1 |  |
| `0x02E0` | `0x00B80` | MWV207REG_AHBDEC_READ_BUFFER_END | 1 |  |
| `0x0300` | `0x00C00` | MWV207REG_AHBDEC_READ_BUFFER_END_EX | 1 |  |
| `0x0320` | `0x00C80` | MWV207REG_AHBDEC_READ_FLUSH_CACHE | 1 |  |
| `0x0340` | `0x00D00` | MWV207REG_AHBDEC_READ_FLUSH_CACHE_EX | 1 |  |
| `0x0360` | `0x00D80` | MWV207REG_AHBDEC_WRITE_BUFFER_BASE | 1 |  |
| `0x0380` | `0x00E00` | MWV207REG_AHBDEC_WRITE_BUFFER_BASE_EX | 1 |  |
| `0x03A0` | `0x00E80` | MWV207REG_AHBDEC_WRITE_BUFFER_END | 1 |  |
| `0x03C0` | `0x00F00` | MWV207REG_AHBDEC_WRITE_BUFFER_END_EX | 1 |  |
| `0x03E0` | `0x00F80` | MWV207REG_AHBDEC_WRITE_FLUSH_CACHE | 1 |  |
| `0x0400` | `0x01000` | MWV207REG_AHBDEC_WRITE_FLUSH_CACHE_EX | 1 |  |
| `0x0420` | `0x01080` | MWV207REG_AHBDEC_READ_CACHE_BASE | 1 |  |
| `0x0440` | `0x01100` | MWV207REG_AHBDEC_READ_CACHE_BASE_EX | 1 |  |
| `0x0460` | `0x01180` | MWV207REG_AHBDEC_WRITE_CACHE_BASE | 1 |  |
| `0x0480` | `0x01200` | MWV207REG_AHBDEC_WRITE_CACHE_BASE_EX | 1 |  |
| `0x04A0` | `0x01280` | MWV207REG_AHBDEC_DEBUG_INFO_OUT | 1 |  |
| `0x04A1` | `0x01284` | MWV207REG_AHBDEC_DEBUG0 | 1 |  |
| `0x04A2` | `0x01288` | MWV207REG_AHBDEC_DEBUG1 | 1 |  |
| `0x04A3` | `0x0128C` | MWV207REG_AHBDEC_DEBUG2 | 1 |  |
| `0x04A4` | `0x01290` | MWV207REG_AHBDEC_DEBUG3 | 1 |  |
| `0x04A5` | `0x01294` | MWV207REG_AHBDEC_DEBUG4 | 1 |  |
| `0x04A6` | `0x01298` | MWV207REG_AHBDEC_DEBUG5 | 1 |  |
| `0x04A7` | `0x0129C` | MWV207REG_AHBDEC_STATE_COMMIT | 1 |  |
| `0x04A8` | `0x012A0` | MWV207REG_AHBDEC_DEBUG6 | 1 |  |
| `0x04A9` | `0x012A4` | MWV207REG_AHBDEC_DEBUG7 | 1 |  |
| `0x04AA` | `0x012A8` | MWV207REG_AHBDEC_CR_DEBUG_TILE128_TYPE0 | 1 |  |
| `0x04AB` | `0x012AC` | MWV207REG_AHBDEC_CR_DEBUG_TILE128_TYPE1 | 1 |  |
| `0x04AC` | `0x012B0` | MWV207REG_AHBDEC_CR_DEBUG_TILE128_TYPE2 | 1 |  |
| `0x04AD` | `0x012B4` | MWV207REG_AHBDEC_CR_DEBUG_TILE128_TYPE3 | 1 |  |
| `0x04AE` | `0x012B8` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE0 | 1 |  |
| `0x04AF` | `0x012BC` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE1 | 1 |  |
| `0x04B0` | `0x012C0` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE2 | 1 |  |
| `0x04B1` | `0x012C4` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE3 | 1 |  |
| `0x04B2` | `0x012C8` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE4 | 1 |  |
| `0x04B3` | `0x012CC` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE5 | 1 |  |
| `0x04B4` | `0x012D0` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE6 | 1 |  |
| `0x04B5` | `0x012D4` | MWV207REG_AHBDEC_CR_DEBUG_TILE256_TYPE7 | 1 |  |
| `0x04B6` | `0x012D8` | MWV207REG_AHBDEC_DEBUG_READ_GATE_DOMAIN_CLK_COUNTER | 1 |  |
| `0x04B7` | `0x012DC` | MWV207REG_AHBDEC_DEBUG_WRITE_GATE_DOMAIN_CLK_COUNTER | 1 |  |
| `0x04B8` | `0x012E0` | MWV207REG_AHBDEC_DEBUG_OTHER_GATE_DOMAIN_CLK_COUNTER | 1 |  |
| `0x04C0` | `0x01300` | MWV207REG_AHBDEC_FAST_CLEAR_VALUE | 1 |  |
| `0x04E0` | `0x01380` | MWV207REG_AHBDEC_FAST_CLEAR_VALUE_EX | 1 |  |

### `VS` — 36 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0214` | `0x00850` | MWV207REG_VS_BALANCE_INTERVAL | 1 |  |
| `0x0215` | `0x00854` | MWV207REG_VS_BALANCE_THRESHOLD | 1 |  |
| `0x0216` | `0x00858` | MWV207REG_VS_BALANCE_SHIFT | 4 |  |
| `0x0217` | `0x0085C` | MWV207REG_VS_PROGRAM | 2 |  |
| `0x021B` | `0x0086C` | MWV207REG_VS_INSTRUCTION | 1 |  |
| `0x021C` | `0x00870` | MWV207REG_VS_ATTRIBUTE | 2 |  |
| `0x021D` | `0x00874` | MWV207REG_VS_START_PC | 1 |  |
| `0x021E` | `0x00878` | MWV207REG_VS_END_PC | 1 |  |
| `0x0223` | `0x0088C` | MWV207REG_VS_INSTRUCTION_PREFETCH | 1 |  |
| `0x0224` | `0x00890` | MWV207REG_VS_INSTRUCTION_PREFETCH_END | 1 |  |
| `0x0226` | `0x00898` | MWV207REG_VS_UNPACK | 16 |  |
| `0x0228` | `0x008A0` | MWV207REG_VS_THROTTLE | 3 |  |
| `0x0229` | `0x008A4` | MWV207REG_VS_UNIFORM_LOCK | 1 |  |
| `0x022A` | `0x008A8` | MWV207REG_VS_SAMPLER_BASE | 2 |  |
| `0x022B` | `0x008AC` | MWV207REG_VS_SAMPLER_LOCK | 2 |  |
| `0x022F` | `0x008BC` | MWV207REG_VS_RELATIVE_END | 1 |  |
| `0x0230` | `0x008C0` | MWV207REG_VS_INPUT | 4 |  |
| `0x0238` | `0x008E0` | MWV207REG_VS_OUTPUT | 4 |  |
| `0x3000` | `0x0C000` | MWV207REG_VS_SHADER_CODE | 1 |  |
| `0x5500` | `0x15400` | MWV207REG_VS_POS_ONLY_START_PC | 1 |  |
| `0x5501` | `0x15404` | MWV207REG_VS_POS_ONLY_END_PC | 1 |  |
| `0x5502` | `0x15408` | MWV207REG_VS_POS_ONLY_INSTRUCTION | 1 |  |
| `0x5503` | `0x1540C` | MWV207REG_VS_POS_ONLY_PREFETCH | 1 |  |
| `0x5504` | `0x15410` | MWV207REG_VS_POS_ONLY_PREFETCH_END | 1 |  |
| `0x5505` | `0x15414` | MWV207REG_VS_POS_ONLY_CONTROL | 5 |  |
| `0x5506` | `0x15418` | MWV207REG_VS_POS_ONLY_INPUT_CONTROL | 2 |  |
| `0x5507` | `0x1541C` | MWV207REG_VS_POS_ONLY_UNPACK | 16 |  |
| `0x5508` | `0x15420` | MWV207REG_VS_POS_ONLY_INPUT | 4 |  |
| `0x550D` | `0x15434` | MWV207REG_VS_POS_ONLY_ATTRIBUTE_BASE | 1 |  |
| `0x550E` | `0x15438` | MWV207REG_VS_POS_ONLY_ATTRIBUTE_SIZE | 2 |  |
| `0x550F` | `0x1543C` | MWV207REG_VS_POS_ONLY_THROTTLE | 3 |  |
| `0x5581` | `0x15604` | MWV207REG_VS_INSTRUCTION_PREFETCH_RELATIVE_END | 1 |  |
| `0x5582` | `0x15608` | MWV207REG_VS_THROTTLE_EX | 1 |  |
| `0x5583` | `0x1560C` | MWV207REG_VS_BUDDY | 1 |  |
| `0x5588` | `0x15620` | MWV207REG_VS_LS_ALLOC | 1 |  |
| `0x558D` | `0x15634` | MWV207REG_VS_ATTRIBUTE_GLOBAL_CONTROL | 3 |  |

### `SHADER` — 7 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0218` | `0x00860` | MWV207REG_SHADER_CONFIG | 17 |  |
| `0x0225` | `0x00894` | MWV207REG_SHADER_CONFIG2 | 1 |  |
| `0x022D` | `0x008B4` | MWV207REG_SHADER_STEERING | 1 |  |
| `0x022E` | `0x008B8` | MWV207REG_SHADER_STATIC | 12 |  |
| `0x0E24` | `0x03890` | MWV207REG_SHADER_L1C_FLUSH_ONLY | 5 |  |
| `0x0E25` | `0x03894` | MWV207REG_SHADER_L1C_FLUSH_WAIT | 5 |  |
| `0x5580` | `0x15600` | MWV207REG_SHADER_MISC_CONFIG | 7 |  |

### `VERTEX` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0219` | `0x00864` | MWV207REG_VERTEX_SHADER_CONST | 1 |  |

### `SH` — 12 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x021A` | `0x00868` | MWV207REG_SH_CACHE_CONTROL | 7 |  |
| `0x0221` | `0x00884` | MWV207REG_SH_SAMPLER_ARBITRATION | 2 |  |
| `0x0222` | `0x00888` | MWV207REG_SH_POWER_MANAGEMENT | 2 |  |
| `0x022C` | `0x008B0` | MWV207REG_SH_ICACHE_INVALIDATE | 5 |  |
| `0x042B` | `0x010AC` | MWV207REG_SH_ALLOC_UNIFORMS | 3 |  |
| `0x042C` | `0x010B0` | MWV207REG_SH_ALLOC_SAMPLERS | 3 |  |
| `0x042D` | `0x010B4` | MWV207REG_SH_ALLOC_TEXTURES | 3 |  |
| `0x0430` | `0x010C0` | MWV207REG_SH_STATE_CONTROL | 2 |  |
| `0x0434` | `0x010D0` | MWV207REG_SH_INTERNAL_CONTROL | 1 |  |
| `0x0437` | `0x010DC` | MWV207REG_SH_INTERNAL_CONTROL1 | 2 |  |
| `0x8000` | `0x20000` | MWV207REG_SH_INSTRUCTIONS | 1 |  |
| `0xC000` | `0x30000` | MWV207REG_SH_UNIFORMS | 1 |  |

### `PS` — 46 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x021F` | `0x0087C` | MWV207REG_PS_START_PC | 1 |  |
| `0x0220` | `0x00880` | MWV207REG_PS_END_PC | 1 |  |
| `0x0407` | `0x0101C` | MWV207REG_PS_PROGRAM | 2 |  |
| `0x040A` | `0x01028` | MWV207REG_PS_INSTRUCTION | 1 |  |
| `0x040B` | `0x0102C` | MWV207REG_PS_OUTPUT_CONTROL | 12 |  |
| `0x040C` | `0x01030` | MWV207REG_PS_OUTPUT_MODE | 8 |  |
| `0x040D` | `0x01034` | MWV207REG_PS_OUTPUT_PRECISION | 8 |  |
| `0x040E` | `0x01038` | MWV207REG_PS_OUTPUT_PRECISION_EX | 8 |  |
| `0x040F` | `0x0103C` | MWV207REG_PS_OUTPUT_MAP | 8 |  |
| `0x0410` | `0x01040` | MWV207REG_PS_INPUT_TYPE | 32 |  |
| `0x0412` | `0x01048` | MWV207REG_PS_INSTRUCTION_PREFETCH | 1 |  |
| `0x0413` | `0x0104C` | MWV207REG_PS_INSTRUCTION_PREFETCH_END | 1 |  |
| `0x0414` | `0x01050` | MWV207REG_PS_UNIFORM_LOCK | 1 |  |
| `0x0415` | `0x01054` | MWV207REG_PS_SAMPLE_COVERAGE_CTRL | 12 |  |
| `0x0416` | `0x01058` | MWV207REG_PS_SAMPLER_BASE | 2 |  |
| `0x0417` | `0x0105C` | MWV207REG_PS_SAMPLER_LOCK | 2 |  |
| `0x0418` | `0x01060` | MWV207REG_PS_SAMPLE_DITHER_TABLE | 8 |  |
| `0x0420` | `0x01080` | MWV207REG_PS_UNPACK | 8 |  |
| `0x0424` | `0x01090` | MWV207REG_PS_RELATIVE_END | 1 |  |
| `0x0425` | `0x01094` | MWV207REG_PS_INSTRUCTION_PREFETCH_RELATIVE_END | 1 |  |
| `0x0426` | `0x01098` | MWV207REG_PS_MAX | 3 |  |
| `0x0428` | `0x010A0` | MWV207REG_PS_TRIGGER_NN | 2 |  |
| `0x0429` | `0x010A4` | MWV207REG_PS_WAIT_FOR_EVENT | 1 |  |
| `0x042E` | `0x010B8` | MWV207REG_PS_TRIGGER_TP | 2 |  |
| `0x042F` | `0x010BC` | MWV207REG_PS_TRIGGER_CUSTOMER_SUPER_INSTRUCTION | 2 |  |
| `0x0432` | `0x010C8` | MWV207REG_PS_OUTPUT_CONTROL1 | 8 |  |
| `0x0433` | `0x010CC` | MWV207REG_PS_OUTPUT_CONTROL2 | 8 |  |
| `0x0435` | `0x010D4` | MWV207REG_PS_INPUT_HPVECFMT | 32 |  |
| `0x0436` | `0x010D8` | MWV207REG_PS_OUTPUT_PRECVECFMT | 8 |  |
| `0x0439` | `0x010E4` | MWV207REG_PS_SAMPLE_COVERAGE_CTRL_EX | 1 |  |
| `0x2000` | `0x08000` | MWV207REG_PS_SHADER_CODE | 1 |  |
| `0x5587` | `0x1561C` | MWV207REG_PS_BUDDY | 1 |  |
| `0x558C` | `0x15630` | MWV207REG_PS_LS_ALLOC | 1 |  |
| `0x7800` | `0x1E000` | MWV207REG_PS_TRANSFER_DESCRIPTOR0 | 4 |  |
| `0x7820` | `0x1E080` | MWV207REG_PS_TRANSFER_DESCRIPTOR1 | 5 |  |
| `0x7840` | `0x1E100` | MWV207REG_PS_TRANSFER_DESCRIPTOR2 | 3 |  |
| `0x7860` | `0x1E180` | MWV207REG_PS_TRANSFER_DESCRIPTOR3 | 2 |  |
| `0x7880` | `0x1E200` | MWV207REG_PS_TRANSFER_DESCRIPTOR4 | 3 |  |
| `0x78A0` | `0x1E280` | MWV207REG_PS_TRANSFER_DESCRIPTOR5 | 3 |  |
| `0x78C0` | `0x1E300` | MWV207REG_PS_TRANSFER_DESCRIPTOR6 | 2 |  |
| `0x78E0` | `0x1E380` | MWV207REG_PS_TRANSFER_DESCRIPTOR7 | 1 |  |
| `0x7900` | `0x1E400` | MWV207REG_PS_TRANSFER_DESCRIPTOR8 | 1 |  |
| `0x7920` | `0x1E480` | MWV207REG_PS_TRANSFER_DESCRIPTOR9 | 1 |  |
| `0x7940` | `0x1E500` | MWV207REG_PS_TRANSFER_DESCRIPTOR10 | 3 |  |
| `0x7960` | `0x1E580` | MWV207REG_PS_TRANSFER_DESCRIPTOR11 | 1 |  |
| `0x7980` | `0x1E600` | MWV207REG_PS_TRANSFER_DESCRIPTOR12 | 1 |  |

### `TW` — 25 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0240` | `0x00900` | MWV207REG_TW_CONFIG | 11 |  |
| `0x0241` | `0x00904` | MWV207REG_TW_INFO_X | 2 |  |
| `0x0242` | `0x00908` | MWV207REG_TW_INFO_Y | 2 |  |
| `0x0243` | `0x0090C` | MWV207REG_TW_INFO_Z | 2 |  |
| `0x0244` | `0x00910` | MWV207REG_TW_WORK_GROUP_X | 2 |  |
| `0x0245` | `0x00914` | MWV207REG_TW_WORK_GROUP_Y | 2 |  |
| `0x0246` | `0x00918` | MWV207REG_TW_WORK_GROUP_Z | 2 |  |
| `0x0247` | `0x0091C` | MWV207REG_TW_SHADER_INFO | 1 |  |
| `0x0248` | `0x00920` | MWV207REG_TW_TRIGGER | 1 |  |
| `0x0249` | `0x00924` | MWV207REG_TW_SHADER_INFO2 | 4 |  |
| `0x024A` | `0x00928` | MWV207REG_TW_INFO_GLOBAL_SIZE_X | 1 |  |
| `0x024B` | `0x0092C` | MWV207REG_TW_INFO_GLOBAL_OFFSET_X | 1 |  |
| `0x024C` | `0x00930` | MWV207REG_TW_INFO_GLOBAL_SIZE_Y | 1 |  |
| `0x024D` | `0x00934` | MWV207REG_TW_INFO_GLOBAL_OFFSET_Y | 1 |  |
| `0x024E` | `0x00938` | MWV207REG_TW_INFO_GLOBAL_SIZE_Z | 1 |  |
| `0x024F` | `0x0093C` | MWV207REG_TW_INFO_GLOBAL_OFFSET_Z | 1 |  |
| `0x0250` | `0x00940` | MWV207REG_TW_WORK_GROUP_COUNT_X | 1 |  |
| `0x0251` | `0x00944` | MWV207REG_TW_WORK_GROUP_COUNT_Y | 1 |  |
| `0x0252` | `0x00948` | MWV207REG_TW_WORK_GROUP_COUNT_Z | 1 |  |
| `0x0253` | `0x0094C` | MWV207REG_TW_WORKGROUP_SIZE_X | 1 |  |
| `0x0254` | `0x00950` | MWV207REG_TW_WORKGROUP_SIZE_Y | 1 |  |
| `0x0255` | `0x00954` | MWV207REG_TW_WORKGROUP_SIZE_Z | 1 |  |
| `0x0256` | `0x00958` | MWV207REG_TW_INFO_GLOBAL_SCALE_X | 1 |  |
| `0x0257` | `0x0095C` | MWV207REG_TW_INFO_GLOBAL_SCALE_Y | 1 |  |
| `0x0258` | `0x00960` | MWV207REG_TW_INFO_GLOBAL_SCALE_Z | 1 |  |

### `PA` — 11 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x028E` | `0x00A38` | MWV207REG_PA_LINE_ADJUST_SUB | 1 |  |
| `0x028F` | `0x00A3C` | MWV207REG_PA_LINE_ADJUST_ADD | 1 |  |
| `0x02A0` | `0x00A80` | MWV207REG_PA_CLIP_W | 1 |  |
| `0x02A1` | `0x00A84` | MWV207REG_PA_MAX_POINT_SIZE | 1 |  |
| `0x02A2` | `0x00A88` | MWV207REG_PA_CLIP_CONTROL | 10 |  |
| `0x02A3` | `0x00A8C` | MWV207REG_PA_SMALL_W | 1 |  |
| `0x02A4` | `0x00A90` | MWV207REG_PA_PACK | 8 |  |
| `0x02AA` | `0x00AA8` | MWV207REG_PA_CONTROL | 3 |  |
| `0x02AC` | `0x00AB0` | MWV207REG_PA_CONTROL_EX | 6 |  |
| `0x02AE` | `0x00AB8` | MWV207REG_PA_DEBUG_MODE | 4 |  |
| `0x7A60` | `0x1E980` | MWV207REG_PA_CLIP_WEX | 1 |  |

### `FINAL` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x02AB` | `0x00AAC` | MWV207REG_FINAL_PA_CONTROL | 1 |  |

### `POS` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x02AD` | `0x00AB4` | MWV207REG_POS_PA_CLUSTER_CTRL | 1 |  |

### `SE` — 10 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0307` | `0x00C1C` | MWV207REG_SE_AREA_THRESHOLD | 1 |  |
| `0x0308` | `0x00C20` | MWV207REG_SE_CLIP_COMPARE_RIGHT | 1 |  |
| `0x0309` | `0x00C24` | MWV207REG_SE_CLIP_COMPARE_BOTTOM | 1 |  |
| `0x030A` | `0x00C28` | MWV207REG_SE_DEPTH_BIAS_CLAMP | 1 |  |
| `0x0310` | `0x00C40` | MWV207REG_SE_CLIP_LEFT_EX | 1 |  |
| `0x0320` | `0x00C80` | MWV207REG_SE_CLIP_TOP_EX | 1 |  |
| `0x0330` | `0x00CC0` | MWV207REG_SE_CLIP_RIGHT_EX | 1 |  |
| `0x0340` | `0x00D00` | MWV207REG_SE_CLIP_BOTTOM_EX | 1 |  |
| `0x0350` | `0x00D40` | MWV207REG_SE_CLIP_COMPARE_RIGHT_EX | 1 |  |
| `0x0360` | `0x00D80` | MWV207REG_SE_CLIP_COMPARE_BOTTOM_EX | 1 |  |

### `MULTI` — 10 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0381` | `0x00E04` | MWV207REG_MULTI_SAMPLE_JITTER | 16 |  |
| `0x0384` | `0x00E10` | MWV207REG_MULTI_SAMPLE_COORDS | 8 |  |
| `0x0E06` | `0x03818` | MWV207REG_MULTI_SAMPLE_CONFIG | 14 |  |
| `0x0E71` | `0x039C4` | MWV207REG_MULTI_SAMPLE_ENABLE_EX | 1 |  |
| `0x0E80` | `0x03A00` | MWV207REG_MULTI_CHIP_CONTROL | 20 |  |
| `0x0E81` | `0x03A04` | MWV207REG_MULTI_CHIP_TOP_LEFT | 2 |  |
| `0x0E82` | `0x03A08` | MWV207REG_MULTI_CHIP_BOTTOM_RIGHT | 2 |  |
| `0x0E83` | `0x03A0C` | MWV207REG_MULTI_CHIP_SET_MAPPING | 4 |  |
| `0x0E84` | `0x03A10` | MWV207REG_MULTI_CHIP_SET_MAPPING_EX | 4 |  |
| `0x4BA0` | `0x12E80` | MWV207REG_MULTI_SRC_CONFIG | 6 |  |

### `RA` — 6 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0382` | `0x00E08` | MWV207REG_RA_CONTROL | 13 |  |
| `0x0383` | `0x00E0C` | MWV207REG_RA_MEDIUMP_MASK | 2 |  |
| `0x0388` | `0x00E20` | MWV207REG_RA_CONTROL_HZ | 6 |  |
| `0x038B` | `0x00E2C` | MWV207REG_RA_DEBUG | 3 |  |
| `0x038C` | `0x00E30` | MWV207REG_RA_TIME_OUT | 1 |  |
| `0x038D` | `0x00E34` | MWV207REG_RA_CONTROL_EX | 5 |  |

### `HZ` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0389` | `0x00E24` | MWV207REG_HZ_BASE_ADDRESS1 | 1 |  |
| `0x038A` | `0x00E28` | MWV207REG_HZ_TILE_STATUS1 | 1 |  |

### `OCL` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0408` | `0x01020` | MWV207REG_OCL_FEATURE_CONTROL | 1 |  |

### `PIXEL` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0409` | `0x01024` | MWV207REG_PIXEL_SHADER_CONST | 1 |  |
| `0xD800` | `0x36000` | MWV207REG_PIXEL_UNIFORMS | 1 |  |

### `PSCS` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0427` | `0x0109C` | MWV207REG_PSCS_THROTTLE | 3 |  |

### `GPIPE` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x042A` | `0x010A8` | MWV207REG_GPIPE_LOAD_BALANCING | 3 |  |
| `0xD000` | `0x34000` | MWV207REG_GPIPE_UNIFORMS | 1 |  |

### `SHL1` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0431` | `0x010C4` | MWV207REG_SHL1_LS_CONTROL | 4 |  |

### `HW` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0438` | `0x010E0` | MWV207REG_HW_INTERNAL_CONTROL | 1 |  |

### `GS` — 20 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0440` | `0x01100` | MWV207REG_GS_CONTROL | 9 |  |
| `0x0441` | `0x01104` | MWV207REG_GS_TYPE | 1 |  |
| `0x0442` | `0x01108` | MWV207REG_GS_COUNTERS | 2 |  |
| `0x0443` | `0x0110C` | MWV207REG_GS_START | 1 |  |
| `0x0444` | `0x01110` | MWV207REG_GS_RELATIVE_END | 1 |  |
| `0x0445` | `0x01114` | MWV207REG_GS_INSTRUCTION | 1 |  |
| `0x0446` | `0x01118` | MWV207REG_GS_INSTRUCTION_PREFETCH | 1 |  |
| `0x0447` | `0x0111C` | MWV207REG_GS_INSTRUCTION_PREFETCH_RELATIVE_END | 1 |  |
| `0x0448` | `0x01120` | MWV207REG_GS_OUTPUT | 4 |  |
| `0x0450` | `0x01140` | MWV207REG_GS_THROTTLE | 3 |  |
| `0x0451` | `0x01144` | MWV207REG_GS_ATTRIBUTE | 3 |  |
| `0x0452` | `0x01148` | MWV207REG_GS_ATTRIBUTE_EX | 1 |  |
| `0x0453` | `0x0114C` | MWV207REG_GS_UNIFORM_BASE | 1 |  |
| `0x0454` | `0x01150` | MWV207REG_GS_UNIFORM_LOCK | 1 |  |
| `0x0455` | `0x01154` | MWV207REG_GS_SAMPLER_BASE | 2 |  |
| `0x0456` | `0x01158` | MWV207REG_GS_SAMPLER_LOCK | 2 |  |
| `0x0457` | `0x0115C` | MWV207REG_GS_THROTTLE_EX | 1 |  |
| `0x0458` | `0x01160` | MWV207REG_GS_ATTRIBUTE_GLOBAL_CONTROL | 3 |  |
| `0x5586` | `0x15618` | MWV207REG_GS_BUDDY | 1 |  |
| `0x558B` | `0x1562C` | MWV207REG_GS_LS_ALLOC | 1 |  |

### `DE` — 113 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x04C2` | `0x01308` | MWV207REG_DE_MULTI_SOURCE | 6 |  |
| `0x04C4` | `0x01310` | MWV207REG_DE_PLANE2_ADDRESS | 2 |  |
| `0x04C5` | `0x01314` | MWV207REG_DE_PLANE2_STRIDE | 1 |  |
| `0x04C6` | `0x01318` | MWV207REG_DE_PLANE3_ADDRESS | 2 |  |
| `0x04C7` | `0x0131C` | MWV207REG_DE_PLANE3_STRIDE | 1 |  |
| `0x04C8` | `0x01320` | MWV207REG_DE_STALL_DE | 1 |  |
| `0x04C9` | `0x01324` | MWV207REG_DE_BLOCK_SIZE | 2 |  |
| `0x04CA` | `0x01328` | MWV207REG_DE_GENERAL_CONFIG | 16 |  |
| `0x04CC` | `0x01330` | MWV207REG_DE_PATTERN_COLOR | 4 |  |
| `0x04E0` | `0x01380` | MWV207REG_DE_WALKER_CONFIG | 4 |  |
| `0x04E1` | `0x01384` | MWV207REG_DE_DST_TILE_STATUS_ADDRESS | 2 |  |
| `0x04E2` | `0x01388` | MWV207REG_DE_REORDER_CONTROL | 2 |  |
| `0x04E6` | `0x01398` | MWV207REG_DE_DST_TILE_STATUS_CLEAR | 1 |  |
| `0x04E7` | `0x0139C` | MWV207REG_DE_DST_TILE_STATUS_CONFIG | 5 |  |
| `0x04E8` | `0x013A0` | MWV207REG_DE_DST_PLANE0_ALIGN_CONFIG | 1 |  |
| `0x04E9` | `0x013A4` | MWV207REG_DE_DST_PLANE1_ALIGN_CONFIG | 1 |  |
| `0x04EA` | `0x013A8` | MWV207REG_DE_DST_PLANE2_ALIGN_CONFIG | 1 |  |
| `0x04F0` | `0x013C0` | MWV207REG_DE_DST_SCALE_FACTOR | 1 |  |
| `0x04F1` | `0x013C4` | MWV207REG_DE_DST_MEAN_FOR_CHANNEL_R | 1 |  |
| `0x04F2` | `0x013C8` | MWV207REG_DE_DST_MEAN_FOR_CHANNEL_G | 1 |  |
| `0x04F3` | `0x013CC` | MWV207REG_DE_DST_MEAN_FOR_CHANNEL_B | 1 |  |
| `0x04F4` | `0x013D0` | MWV207REG_DE_DST_STD_RECIPROCAL_FOR_CHANNEL_R | 1 |  |
| `0x04F5` | `0x013D4` | MWV207REG_DE_DST_STD_RECIPROCAL_FOR_CHANNEL_G | 1 |  |
| `0x04F6` | `0x013D8` | MWV207REG_DE_DST_STD_RECIPROCAL_FOR_CHANNEL_B | 1 |  |
| `0x04F7` | `0x013DC` | MWV207REG_DE_DST_NORMALIZATION_MODE | 1 |  |
| `0x04F8` | `0x013E0` | MWV207REG_DE_DST_QUANTIZATION_BYPASS | 1 |  |
| `0x04F9` | `0x013E4` | MWV207REG_DE_DST_STEP_RECIPROCAL | 1 |  |
| `0x04FA` | `0x013E8` | MWV207REG_DE_DST_MIN_VALUE_FOR_CHANNEL_R | 1 |  |
| `0x04FB` | `0x013EC` | MWV207REG_DE_DST_MIN_VALUE_FOR_CHANNEL_G | 1 |  |
| `0x04FC` | `0x013F0` | MWV207REG_DE_DST_MIN_VALUE_FOR_CHANNEL_B | 1 |  |
| `0x04FD` | `0x013F4` | MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_R | 1 |  |
| `0x04FE` | `0x013F8` | MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_G | 1 |  |
| `0x04FF` | `0x013FC` | MWV207REG_DE_DST_MAX_MIN_RECIPROCAL_FOR_CHANNEL_B | 1 |  |
| `0x0570` | `0x015C0` | MWV207REG_DE_ADJUST_CONTROL | 3 |  |
| `0x0571` | `0x015C4` | MWV207REG_DE_ADJUST_FACTOR1 | 1 |  |
| `0x0572` | `0x015C8` | MWV207REG_DE_ADJUST_FACTOR2 | 1 |  |
| `0x0573` | `0x015CC` | MWV207REG_DE_HIST_CONTROL | 17 |  |
| `0x0574` | `0x015D0` | MWV207REG_DE_HIST_EQUAL_MN | 1 |  |
| `0x0575` | `0x015D4` | MWV207REG_DE_HIST_SOBEL_MATRIX_GX0 | 3 |  |
| `0x0576` | `0x015D8` | MWV207REG_DE_HIST_SOBEL_MATRIX_GX1 | 3 |  |
| `0x0577` | `0x015DC` | MWV207REG_DE_HIST_SOBEL_MATRIX_GX2 | 3 |  |
| `0x0578` | `0x015E0` | MWV207REG_DE_HIST_SOBEL_MATRIX_GY0 | 3 |  |
| `0x0579` | `0x015E4` | MWV207REG_DE_HIST_SOBEL_MATRIX_GY1 | 3 |  |
| `0x057A` | `0x015E8` | MWV207REG_DE_HIST_SOBEL_MATRIX_GY2 | 3 |  |
| `0x4A00` | `0x12800` | MWV207REG_DE_SRC_ADDRESS | 2 |  |
| `0x4A04` | `0x12810` | MWV207REG_DE_SRC_STRIDE | 1 |  |
| `0x4A08` | `0x12820` | MWV207REG_DE_SRC_ROTATION_CONFIG | 2 |  |
| `0x4A0C` | `0x12830` | MWV207REG_DE_SRC_CONFIG | 12 |  |
| `0x4A10` | `0x12840` | MWV207REG_DE_SRC_ORIGIN | 2 |  |
| `0x4A14` | `0x12850` | MWV207REG_DE_SRC_SIZE | 2 |  |
| `0x4A18` | `0x12860` | MWV207REG_DE_SRC_COLOR_BG | 4 |  |
| `0x4A1C` | `0x12870` | MWV207REG_DE_ROP | 3 |  |
| `0x4A20` | `0x12880` | MWV207REG_DE_ALPHA_CONTROL | 1 |  |
| `0x4A24` | `0x12890` | MWV207REG_DE_ALPHA_MODES | 8 |  |
| `0x4A28` | `0x128A0` | MWV207REG_DE_ADDRESS_U | 2 |  |
| `0x4A2C` | `0x128B0` | MWV207REG_DE_STRIDE_U | 1 |  |
| `0x4A30` | `0x128C0` | MWV207REG_DE_ADDRESS_V | 2 |  |
| `0x4A34` | `0x128D0` | MWV207REG_DE_STRIDE_V | 1 |  |
| `0x4A38` | `0x128E0` | MWV207REG_DE_SRC_ROTATION_HEIGHT | 1 |  |
| `0x4A3C` | `0x128F0` | MWV207REG_DE_ROT_ANGLE | 12 |  |
| `0x4A40` | `0x12900` | MWV207REG_DE_GLOBAL_SRC_COLOR | 4 |  |
| `0x4A44` | `0x12910` | MWV207REG_DE_GLOBAL_DEST_COLOR | 4 |  |
| `0x4A48` | `0x12920` | MWV207REG_DE_COLOR_MULTIPLY_MODES | 5 |  |
| `0x4A54` | `0x12950` | MWV207REG_DE_SRC_COLOR_KEY_HIGH | 4 |  |
| `0x4A58` | `0x12960` | MWV207REG_DE_SRC_EX_CONFIG | 4 |  |
| `0x4A5C` | `0x12970` | MWV207REG_DE_SRC_EX_ADDRESS | 2 |  |
| `0x4A60` | `0x12980` | MWV207REG_DE_SRC_PLANE0_ALIGN_CONFIG | 1 |  |
| `0x4A68` | `0x129A0` | MWV207REG_DE_SRC_PLANE1_ALIGN_CONFIG | 1 |  |
| `0x4A70` | `0x129C0` | MWV207REG_DE_SRC_PLANE2_ALIGN_CONFIG | 1 |  |
| `0x4A80` | `0x12A00` | MWV207REG_DE_SRC_ADDRESS_EX | 2 |  |
| `0x4A88` | `0x12A20` | MWV207REG_DE_SRC_STRIDE_EX | 1 |  |
| `0x4A90` | `0x12A40` | MWV207REG_DE_SRC_ROTATION_CONFIG_EX | 2 |  |
| `0x4A98` | `0x12A60` | MWV207REG_DE_SRC_CONFIG_EX | 14 |  |
| `0x4AA0` | `0x12A80` | MWV207REG_DE_SRC_ORIGIN_EX | 2 |  |
| `0x4AA8` | `0x12AA0` | MWV207REG_DE_SRC_SIZE_EX | 2 |  |
| `0x4AB0` | `0x12AC0` | MWV207REG_DE_SRC_COLOR_BG_EX | 4 |  |
| `0x4AB8` | `0x12AE0` | MWV207REG_DE_ROP_EX | 3 |  |
| `0x4AC0` | `0x12B00` | MWV207REG_DE_ALPHA_CONTROL_EX | 1 |  |
| `0x4AC8` | `0x12B20` | MWV207REG_DE_ALPHA_MODES_EX | 8 |  |
| `0x4AD0` | `0x12B40` | MWV207REG_DE_ADDRESS_UEX | 2 |  |
| `0x4AD8` | `0x12B60` | MWV207REG_DE_STRIDE_UEX | 1 |  |
| `0x4AE0` | `0x12B80` | MWV207REG_DE_ADDRESS_VEX | 2 |  |
| `0x4AE8` | `0x12BA0` | MWV207REG_DE_STRIDE_VEX | 1 |  |
| `0x4AF0` | `0x12BC0` | MWV207REG_DE_SRC_ROTATION_HEIGHT_EX | 1 |  |
| `0x4AF8` | `0x12BE0` | MWV207REG_DE_ROT_ANGLE_EX | 12 |  |
| `0x4B00` | `0x12C00` | MWV207REG_DE_GLOBAL_SRC_COLOR_EX | 4 |  |
| `0x4B08` | `0x12C20` | MWV207REG_DE_GLOBAL_DEST_COLOR_EX | 4 |  |
| `0x4B10` | `0x12C40` | MWV207REG_DE_COLOR_MULTIPLY_MODES_EX | 5 |  |
| `0x4B28` | `0x12CA0` | MWV207REG_DE_SRC_COLOR_KEY_HIGH_EX | 4 |  |
| `0x4B30` | `0x12CC0` | MWV207REG_DE_SRC_EX_CONFIG_EX | 8 |  |
| `0x4B38` | `0x12CE0` | MWV207REG_DE_SRC_EX_ADDRESS_EX | 2 |  |
| `0x4B40` | `0x12D00` | MWV207REG_DE_CLIP_TOP_LEFT | 2 |  |
| `0x4B48` | `0x12D20` | MWV207REG_DE_CLIP_BOTTOM_RIGHT | 2 |  |
| `0x4B50` | `0x12D40` | MWV207REG_DE_DEST_RECT_TOP_LEFT | 2 |  |
| `0x4B58` | `0x12D60` | MWV207REG_DE_DEST_RECT_BOTTOM_RIGHT | 2 |  |
| `0x4B60` | `0x12D80` | MWV207REG_DE_STRETCH_FACTOR_LOW | 1 |  |
| `0x4B68` | `0x12DA0` | MWV207REG_DE_STRETCH_FACTOR_HIGH | 1 |  |
| `0x4B70` | `0x12DC0` | MWV207REG_DE_SRC_VINITIAL_ERROR | 1 |  |
| `0x4B78` | `0x12DE0` | MWV207REG_DE_SRC_VSOUTH_DELTA | 1 |  |
| `0x4B80` | `0x12E00` | MWV207REG_DE_SRC_VNORTH_DELTA | 1 |  |
| `0x4B88` | `0x12E20` | MWV207REG_DE_SRC_HINITIAL_ERROR | 1 |  |
| `0x4B90` | `0x12E40` | MWV207REG_DE_SRC_HSOUTH_DELTA | 1 |  |
| `0x4B98` | `0x12E60` | MWV207REG_DE_SRC_HNORTH_DELTA | 1 |  |
| `0x4BB8` | `0x12EE0` | MWV207REG_DE_SRC_TILE_STATUS_ADDRESS | 2 |  |
| `0x4BE0` | `0x12F80` | MWV207REG_DE_SRC_ORIGIN_FRACTION | 2 |  |
| `0x4BE8` | `0x12FA0` | MWV207REG_DE_SRC_TILE_STATUS_CLEAR | 1 |  |
| `0x4BF0` | `0x12FC0` | MWV207REG_DE_SRC_TILE_STATUS_CONFIG | 8 |  |
| `0x4BF8` | `0x12FE0` | MWV207REG_DE_SRC_TILE_STATUS_CLEAR64 | 1 |  |
| `0x4E00` | `0x13800` | MWV207REG_DE_DEGAMMA_LOOKUP | 3 |  |
| `0x4F00` | `0x13C00` | MWV207REG_DE_GAMMA_LOOKUP | 3 |  |
| `0x6600` | `0x19800` | MWV207REG_DE_ADJUST_SAT_GAMMA_LUT | 1 |  |
| `0x6700` | `0x19C00` | MWV207REG_DE_ADJUST_VALUE_GAMMA_LUT | 1 |  |
| `0x7400` | `0x1D000` | MWV207REG_DE_DEGAMMA_LOOKUP_EX | 3 |  |

### `DEYUV` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x04C3` | `0x0130C` | MWV207REG_DEYUV_CONVERSION | 16 |  |

### `DEST` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x04CB` | `0x0132C` | MWV207REG_DEST_CONFIG_EX | 16 |  |

### `DEYUV2` — 8 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x04D0` | `0x01340` | MWV207REG_DEYUV2_RGB_COEF0 | 2 |  |
| `0x04D1` | `0x01344` | MWV207REG_DEYUV2_RGB_COEF1 | 2 |  |
| `0x04D2` | `0x01348` | MWV207REG_DEYUV2_RGB_COEF2 | 2 |  |
| `0x04D3` | `0x0134C` | MWV207REG_DEYUV2_RGB_COEF3 | 2 |  |
| `0x04D4` | `0x01350` | MWV207REG_DEYUV2_RGB_COEF4 | 2 |  |
| `0x04D5` | `0x01354` | MWV207REG_DEYUV2_RGB_COEF_D0 | 1 |  |
| `0x04D6` | `0x01358` | MWV207REG_DEYUV2_RGB_COEF_D1 | 1 |  |
| `0x04D7` | `0x0135C` | MWV207REG_DEYUV2_RGB_COEF_D2 | 1 |  |

### `DERGB2` — 8 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x04D8` | `0x01360` | MWV207REG_DERGB2_YUV_COEF0 | 2 |  |
| `0x04D9` | `0x01364` | MWV207REG_DERGB2_YUV_COEF1 | 2 |  |
| `0x04DA` | `0x01368` | MWV207REG_DERGB2_YUV_COEF2 | 2 |  |
| `0x04DB` | `0x0136C` | MWV207REG_DERGB2_YUV_COEF3 | 2 |  |
| `0x04DC` | `0x01370` | MWV207REG_DERGB2_YUV_COEF4 | 2 |  |
| `0x04DD` | `0x01374` | MWV207REG_DERGB2_YUV_COEF_D0 | 1 |  |
| `0x04DE` | `0x01378` | MWV207REG_DERGB2_YUV_COEF_D1 | 1 |  |
| `0x04DF` | `0x0137C` | MWV207REG_DERGB2_YUV_COEF_D2 | 1 |  |

### `PE` — 43 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0515` | `0x01454` | MWV207REG_PE_HIERARCHICAL_CONTROL | 2 |  |
| `0x0516` | `0x01458` | MWV207REG_PE_HIERARCHICAL_ADDRESS | 2 |  |
| `0x0517` | `0x0145C` | MWV207REG_PE_VAA_CONTROL | 1 |  |
| `0x0518` | `0x01460` | MWV207REG_PE_COLOR_BASE_ADDRESS | 2 |  |
| `0x0520` | `0x01480` | MWV207REG_PE_DEPTH_BASE_ADDRESS | 2 |  |
| `0x0528` | `0x014A0` | MWV207REG_PE_EXTRA_REFERENCE | 4 |  |
| `0x0529` | `0x014A4` | MWV207REG_PE_CONFIG | 21 |  |
| `0x052A` | `0x014A8` | MWV207REG_PE_DITHER_LOW | 8 |  |
| `0x052B` | `0x014AC` | MWV207REG_PE_DITHER_HIGH | 8 |  |
| `0x052C` | `0x014B0` | MWV207REG_PE_BLEND_FACTOR_LOW | 2 |  |
| `0x052D` | `0x014B4` | MWV207REG_PE_BLEND_FACTOR_HIGH | 2 |  |
| `0x052E` | `0x014B8` | MWV207REG_PE_BACKFACE_STENCIL | 2 |  |
| `0x052F` | `0x014BC` | MWV207REG_PE_CONFIG_EX | 13 |  |
| `0x0530` | `0x014C0` | MWV207REG_PE_ALPHA_CONFIG | 15 |  |
| `0x0532` | `0x014C8` | MWV207REG_PE_RENDER_TARGET_FORMAT | 6 |  |
| `0x0540` | `0x01500` | MWV207REG_PE_RENDER_TARGET_ADDRESS | 2 |  |
| `0x0560` | `0x01580` | MWV207REG_PE_RENDER_TARGET_CONFIG | 4 |  |
| `0x0564` | `0x01590` | MWV207REG_PE_RENDER_TARGET_EXTRA_CONFIG | 1 |  |
| `0x4A4C` | `0x12930` | MWV207REG_PE_TRANSPARENCY | 10 |  |
| `0x4A50` | `0x12940` | MWV207REG_PE_CONTROL | 10 |  |
| `0x4B18` | `0x12C60` | MWV207REG_PE_TRANSPARENCY_EX | 10 |  |
| `0x4B20` | `0x12C80` | MWV207REG_PE_CONTROL_EX | 10 |  |
| `0x5200` | `0x14800` | MWV207REG_PE_RENDER_TARGET_ADDRESS_EX | 2 |  |
| `0x5240` | `0x14900` | MWV207REG_PE_RENDER_TARGET_CONFIG_EX | 7 |  |
| `0x5248` | `0x14920` | MWV207REG_PE_RENDER_TARGET_EXTRA_CONFIG_EX | 8 |  |
| `0x5250` | `0x14940` | MWV207REG_PE_RENDER_TARGET_ALPHA_TEST | 3 |  |
| `0x5258` | `0x14960` | MWV207REG_PE_RENDER_TARGET_ALPHA_CONTROL | 15 |  |
| `0x5260` | `0x14980` | MWV207REG_PE_RENDER_TARGET_BLEND_COLOR_LOW | 2 |  |
| `0x5268` | `0x149A0` | MWV207REG_PE_RENDER_TARGET_BLEND_COLOR_HIGH | 2 |  |
| `0x5270` | `0x149C0` | MWV207REG_PE_RENDER_TARGET_END_ADDRES_EX | 1 |  |
| `0x5278` | `0x149E0` | MWV207REG_PE_RENDER_TARGET_FORMAT_EX | 6 |  |
| `0x5F00` | `0x17C00` | MWV207REG_PE_RENDER_TARGET_ADDRESS_EX2 | 2 |  |
| `0x5F10` | `0x17C40` | MWV207REG_PE_RENDER_TARGET_CONFIG_EX2 | 6 |  |
| `0x5F20` | `0x17C80` | MWV207REG_PE_RENDER_TARGET_EXTRA_CONFIG_EX2 | 11 |  |
| `0x5F30` | `0x17CC0` | MWV207REG_PE_RENDER_TARGET_ALPHA_TEST_EX | 3 |  |
| `0x5F40` | `0x17D00` | MWV207REG_PE_RENDER_TARGET_ALPHA_CONTROL_EX | 15 |  |
| `0x5F50` | `0x17D40` | MWV207REG_PE_RENDER_TARGET_END_ADDRES_EX2 | 1 |  |
| `0x5F60` | `0x17D80` | MWV207REG_PE_RENDER_TARGET_BLEND_COLOR_EX_LOW | 2 |  |
| `0x5F70` | `0x17DC0` | MWV207REG_PE_RENDER_TARGET_BLEND_COLOR_EX_HIGH | 2 |  |
| `0x5FD0` | `0x17F40` | MWV207REG_PE_BLEND_FACTOR_FP32_RED | 1 |  |
| `0x5FD8` | `0x17F60` | MWV207REG_PE_BLEND_FACTOR_FP32_GREEN | 1 |  |
| `0x5FE0` | `0x17F80` | MWV207REG_PE_BLEND_FACTOR_FP32_BLUE | 1 |  |
| `0x5FE8` | `0x17FA0` | MWV207REG_PE_BLEND_FACTOR_FP32_ALPHA | 1 |  |

### `DEPTH` — 4 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0531` | `0x014C4` | MWV207REG_DEPTH_BUFFER_END_ADDRESS | 1 |  |
| `0x0E23` | `0x0388C` | MWV207REG_DEPTH_SLICE | 1 |  |
| `0x7A70` | `0x1E9C0` | MWV207REG_DEPTH_NEAR_EX | 1 |  |
| `0x7A80` | `0x1EA00` | MWV207REG_DEPTH_FAR_EX | 1 |  |

### `STENCIL` — 4 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0533` | `0x014CC` | MWV207REG_STENCIL_STRIDE | 1 |  |
| `0x0534` | `0x014D0` | MWV207REG_STENCIL_BASE_ADDRESS | 2 |  |
| `0x0535` | `0x014D4` | MWV207REG_STENCIL_BUFFER_END_ADDRESS | 1 |  |
| `0x0E58` | `0x03960` | MWV207REG_STENCIL_SLICE | 1 |  |

### `HIST` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x057B` | `0x015EC` | MWV207REG_HIST_BASE_ADDRESS0 | 2 |  |
| `0x057C` | `0x015F0` | MWV207REG_HIST_BASE_ADDRESS1 | 2 |  |
| `0x057D` | `0x015F4` | MWV207REG_HIST_BASE_ADDRESS2 | 2 |  |

### `RS` — 18 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x05A8` | `0x016A0` | MWV207REG_RS_CONFIG | 14 |  |
| `0x05AC` | `0x016B0` | MWV207REG_RS_TILE_FILLER_TRIGGER | 1 |  |
| `0x05AD` | `0x016B4` | MWV207REG_RS_INTERLEAVE_ADDRESS | 1 |  |
| `0x05AE` | `0x016B8` | MWV207REG_RS_CONTROL | 2 |  |
| `0x05B0` | `0x016C0` | MWV207REG_RS_SOURCE_ADDRESS | 2 |  |
| `0x05B8` | `0x016E0` | MWV207REG_RS_TARGET_ADDRESS | 2 |  |
| `0x05C0` | `0x01700` | MWV207REG_RS_OFFSET | 2 |  |
| `0x0698` | `0x01A60` | MWV207REG_RS_SRC_SURFACE_SIZE | 2 |  |
| `0x0699` | `0x01A64` | MWV207REG_RS_SRC_ROTATION_CONFIG | 6 |  |
| `0x069A` | `0x01A68` | MWV207REG_RS_SRC_BUFFER_END_ADDRESS | 1 |  |
| `0x069B` | `0x01A6C` | MWV207REG_RS_SRC_UBUFFER_END_ADDRESS | 1 |  |
| `0x06B8` | `0x01AE0` | MWV207REG_RS_SRC_VBUFFER_END_ADDRESS | 1 |  |
| `0x06B9` | `0x01AE4` | MWV207REG_RS_DST_BUFFER_END_ADDRESS | 1 |  |
| `0x06BA` | `0x01AE8` | MWV207REG_RS_DST_CONFIG_EX | 2 |  |
| `0x06BB` | `0x01AEC` | MWV207REG_RS_DST_ADDRESS1 | 2 |  |
| `0x06BC` | `0x01AF0` | MWV207REG_RS_DST_STRIDE1 | 1 |  |
| `0x06BD` | `0x01AF4` | MWV207REG_RS_DST_ADDRESS2 | 2 |  |
| `0x06BE` | `0x01AF8` | MWV207REG_RS_DST_STRIDE2 | 1 |  |

### `MC` — 18 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x05A9` | `0x016A4` | MWV207REG_MC_CACHE_HZ | 2 |  |
| `0x05AA` | `0x016A8` | MWV207REG_MC_CLEAR_VALUE_HZ | 1 |  |
| `0x05AB` | `0x016AC` | MWV207REG_MC_COUNTER_HZ | 1 |  |
| `0x05AF` | `0x016BC` | MWV207REG_MC_COLOR_CLEAR64 | 1 |  |
| `0x05C8` | `0x01720` | MWV207REG_MC_TEXTURE_CONFIG | 15 |  |
| `0x05D0` | `0x01740` | MWV207REG_MC_TEXTURE_BUFFER | 1 |  |
| `0x05D8` | `0x01760` | MWV207REG_MC_TEXTURE_CLEAR | 1 |  |
| `0x05E0` | `0x01780` | MWV207REG_MC_TEXTURE_CLEAR64 | 1 |  |
| `0x069C` | `0x01A70` | MWV207REG_MC_SHADER_CONFIG | 7 |  |
| `0x06A0` | `0x01A80` | MWV207REG_MC_TEXTURE_BASE_BUFFER | 1 |  |
| `0x06A8` | `0x01AA0` | MWV207REG_MC_SHADER_BUFFER | 1 |  |
| `0x06AC` | `0x01AB0` | MWV207REG_MC_SHADER_CLEAR | 1 |  |
| `0x06B0` | `0x01AC0` | MWV207REG_MC_SHADER_CLEAR64 | 1 |  |
| `0x06B4` | `0x01AD0` | MWV207REG_MC_SHADER_BASE_BUFFER | 1 |  |
| `0x06D4` | `0x01B50` | MWV207REG_MC_SHADER_CLEAR96 | 1 |  |
| `0x06D8` | `0x01B60` | MWV207REG_MC_SHADER_CLEAR128 | 1 |  |
| `0x06DC` | `0x01B70` | MWV207REG_MC_TEXTURE_CLEAR96 | 1 |  |
| `0x06E0` | `0x01B80` | MWV207REG_MC_TEXTURE_CLEAR128 | 1 |  |

### `MRT` — 7 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x05E8` | `0x017A0` | MWV207REG_MRT_MEMORY_CONFIG | 9 |  |
| `0x05F0` | `0x017C0` | MWV207REG_MRT_COLOR_CACHE | 2 |  |
| `0x05F8` | `0x017E0` | MWV207REG_MRT_COLOR_BASE | 2 |  |
| `0x0680` | `0x01A00` | MWV207REG_MRT_COLOR_CLEAR_VALUE | 1 |  |
| `0x0690` | `0x01A40` | MWV207REG_MRT_TILE_STATUS_COUNTER_C | 1 |  |
| `0x06C0` | `0x01B00` | MWV207REG_MRT_COLOR_CLEAR_VALUE96 | 1 |  |
| `0x06C8` | `0x01B20` | MWV207REG_MRT_COLOR_CLEAR_VALUE128 | 1 |  |

### `MRTMC` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0688` | `0x01A20` | MWV207REG_MRTMC_COLOR_CLEAR64 | 1 |  |

### `TX` — 60 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0850` | `0x02140` | MWV207REG_TX_STRIDE | 1 |  |
| `0x0860` | `0x02180` | MWV207REG_TX_VOLUME | 3 |  |
| `0x0870` | `0x021C0` | MWV207REG_TX_MODE | 17 |  |
| `0x0880` | `0x02200` | MWV207REG_TX_CONTROL_YUV | 15 |  |
| `0x0890` | `0x02240` | MWV207REG_TX_STRIDE_YUV | 2 |  |
| `0x08A0` | `0x02280` | MWV207REG_TX_ASTC0 | 8 |  |
| `0x08B0` | `0x022C0` | MWV207REG_TX_ASTC1 | 8 |  |
| `0x08C0` | `0x02300` | MWV207REG_TX_ASTC2 | 8 |  |
| `0x08D0` | `0x02340` | MWV207REG_TX_ASTC3 | 8 |  |
| `0x0B00` | `0x02C00` | MWV207REG_TX_SAMPLER_STRIDE | 2 |  |
| `0x0E17` | `0x0385C` | MWV207REG_TX_CACHE | 1 |  |
| `0x4000` | `0x10000` | MWV207REG_TX_CONFIG | 12 |  |
| `0x4020` | `0x10080` | MWV207REG_TX_SIZE | 2 |  |
| `0x4040` | `0x10100` | MWV207REG_TX_LOG_SIZE | 6 |  |
| `0x4060` | `0x10180` | MWV207REG_TX_LOD | 4 |  |
| `0x4080` | `0x10200` | MWV207REG_TX_BORDER_COLOR | 4 |  |
| `0x40A0` | `0x10280` | MWV207REG_TX_LINEAR_STRIDE | 1 |  |
| `0x40E0` | `0x10380` | MWV207REG_TX_EXT_CONFIG | 16 |  |
| `0x4100` | `0x10400` | MWV207REG_TX_CONTROL_YUV_EX | 15 |  |
| `0x4120` | `0x10480` | MWV207REG_TX_STRIDE_YUV_EX | 2 |  |
| `0x4140` | `0x10500` | MWV207REG_TX_ASTC0_EX | 8 |  |
| `0x4160` | `0x10580` | MWV207REG_TX_ASTC1_EX | 8 |  |
| `0x4180` | `0x10600` | MWV207REG_TX_ASTC2_EX | 8 |  |
| `0x41A0` | `0x10680` | MWV207REG_TX_ASTC3_EX | 8 |  |
| `0x41C0` | `0x10700` | MWV207REG_TX_BASE_LOD | 5 |  |
| `0x41E0` | `0x10780` | MWV207REG_TX_CONFIG2 | 12 |  |
| `0x4200` | `0x10800` | MWV207REG_TX_ADDRESS | 2 |  |
| `0x4400` | `0x11000` | MWV207REG_TX_SIZE_EXT | 2 |  |
| `0x4420` | `0x11080` | MWV207REG_TX_VOLUME_EXT | 1 |  |
| `0x4440` | `0x11100` | MWV207REG_TX_LOD_EXT | 2 |  |
| `0x4460` | `0x11180` | MWV207REG_TX_LOD_BIAS_EXT | 2 |  |
| `0x4480` | `0x11200` | MWV207REG_TX_SLICE | 1 |  |
| `0x44A0` | `0x11280` | MWV207REG_TX_CONFIG3 | 6 |  |
| `0x44C0` | `0x11300` | MWV207REG_TX_ANISO_CTRL | 1 |  |
| `0x44E0` | `0x11380` | MWV207REG_TX_BORDER_COLOR_RED32 | 1 |  |
| `0x4500` | `0x11400` | MWV207REG_TX_BORDER_COLOR_GREEN32 | 1 |  |
| `0x4520` | `0x11480` | MWV207REG_TX_BORDER_COLOR_BLUE32 | 1 |  |
| `0x4540` | `0x11500` | MWV207REG_TX_BORDER_COLOR_ALPHA32 | 1 |  |
| `0x4800` | `0x12000` | MWV207REG_TX_HORIZONTAL_KERNEL | 4 |  |
| `0x4900` | `0x12400` | MWV207REG_TX_VERTICAL_KERNEL | 4 |  |
| `0x5300` | `0x14C00` | MWV207REG_TX_GUARDBAND | 4 |  |
| `0x5310` | `0x14C40` | MWV207REG_TX_GLOBAL_CONFIG | 1 |  |
| `0x5311` | `0x14C44` | MWV207REG_TX_GPIPE_COMMAND | 3 |  |
| `0x5312` | `0x14C48` | MWV207REG_TX_PIXEL_COMMAND | 3 |  |
| `0x5313` | `0x14C4C` | MWV207REG_TX_GPIPE_INVALIDATE_TEXTURE_DESCRIPTOR | 1 |  |
| `0x5314` | `0x14C50` | MWV207REG_TX_PIXEL_INVALIDATE_TEXTURE_DESCRIPTOR | 1 |  |
| `0x5600` | `0x15800` | MWV207REG_TX_GPIPE_TEXTURE_DESCRIPTOR_ADDR | 1 |  |
| `0x5680` | `0x15A00` | MWV207REG_TX_GPIPE_TEXTURE_CTRL | 5 |  |
| `0x5700` | `0x15C00` | MWV207REG_TX_PIXEL_TEXTURE_DESCRIPTOR_ADDR | 1 |  |
| `0x5780` | `0x15E00` | MWV207REG_TX_PIXEL_TEXTURE_CTRL | 5 |  |
| `0x5800` | `0x16000` | MWV207REG_TX_GPIPE_SAMPLER_CTRL0 | 13 |  |
| `0x5880` | `0x16200` | MWV207REG_TX_GPIPE_SAMPLER_CTRL1 | 5 |  |
| `0x5900` | `0x16400` | MWV207REG_TX_GPIPE_SAMPLER_LOD_MAX_MIN | 2 |  |
| `0x5980` | `0x16600` | MWV207REG_TX_GPIPE_SAMPLER_LOD_BIAS | 2 |  |
| `0x5A00` | `0x16800` | MWV207REG_TX_GPIPE_SAMPLER_ANISO_CTRL | 1 |  |
| `0x5B00` | `0x16C00` | MWV207REG_TX_PIXEL_SAMPLER_CTRL0 | 13 |  |
| `0x5B80` | `0x16E00` | MWV207REG_TX_PIXEL_SAMPLER_CTRL1 | 5 |  |
| `0x5C00` | `0x17000` | MWV207REG_TX_PIXEL_SAMPLER_LOD_MAX_MIN | 2 |  |
| `0x5C80` | `0x17200` | MWV207REG_TX_PIXEL_SAMPLER_LOD_BIAS | 2 |  |
| `0x5D00` | `0x17400` | MWV207REG_TX_PIXEL_SAMPLER_ANISO_CTRL | 1 |  |

### `MCFE` — 8 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0900` | `0x02400` | MWV207REG_MCFE_STD_DESC_RING_BUF_START_ADDR | 1 |  |
| `0x0940` | `0x02500` | MWV207REG_MCFE_STD_DESC_FIFO_DEPTH_EXP | 1 |  |
| `0x0980` | `0x02600` | MWV207REG_MCFE_STD_DESC_FIFO_WR_PTR | 1 |  |
| `0x09C0` | `0x02700` | MWV207REG_MCFE_STD_DESC_FIFO_RD_PTR | 1 |  |
| `0x0A00` | `0x02800` | MWV207REG_MCFE_PRI_DESC_RING_BUF_START_ADDR | 1 |  |
| `0x0A40` | `0x02900` | MWV207REG_MCFE_PRI_DESC_FIFO_DEPTH_EXP | 1 |  |
| `0x0A80` | `0x02A00` | MWV207REG_MCFE_PRI_DESC_FIFO_WR_PTR | 1 |  |
| `0x0AC0` | `0x02B00` | MWV207REG_MCFE_PRI_DESC_FIFO_RD_PTR | 1 |  |

### `AHBTT` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0B00` | `0x02C00` | MWV207REG_AHBTT_SYNC | 1 |  |

### `AHBTTD` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0B01` | `0x02C04` | MWV207REG_AHBTTD_MISC | 7 |  |

### `AHBVIP` — 4 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0B02` | `0x02C08` | MWV207REG_AHBVIP_TENSOR_IO | 1 |  |
| `0x0B03` | `0x02C0C` | MWV207REG_AHBVIP_TENSOR_IO_WR | 1 |  |
| `0x0B07` | `0x02C1C` | MWV207REG_AHBVIP_CLOCK_GATE | 28 |  |
| `0x0B08` | `0x02C20` | MWV207REG_AHBVIP_DEBUG | 1 |  |

### `AHBVIPDMA` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0B04` | `0x02C10` | MWV207REG_AHBVIPDMA_RAW_STATUS | 3 |  |
| `0x0B05` | `0x02C14` | MWV207REG_AHBVIPDMA_INTR_EN | 3 |  |
| `0x0B06` | `0x02C18` | MWV207REG_AHBVIPDMA_INTR_STATUS | 3 |  |

### `AHBXTTD` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0B10` | `0x02C40` | MWV207REG_AHBXTTD_MISC | 19 |  |
| `0x0B68` | `0x02DA0` | MWV207REG_AHBXTTD_MISC2 | 6 |  |

### `AHBX` — 60 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0B11` | `0x02C44` | MWV207REG_AHBX_TENSOR_IO | 1 |  |
| `0x0B12` | `0x02C48` | MWV207REG_AHBX_TENSOR_IO_WR | 1 |  |
| `0x0B5F` | `0x02D7C` | MWV207REG_AHBX_AXI_QOS0 | 8 |  |
| `0x0B60` | `0x02D80` | MWV207REG_AHBX_AXI_QOS1 | 8 |  |
| `0x0B61` | `0x02D84` | MWV207REG_AHBX_AXI_QOS2 | 8 |  |
| `0x0B62` | `0x02D88` | MWV207REG_AHBX_AXI_QOS3 | 8 |  |
| `0x0B63` | `0x02D8C` | MWV207REG_AHBX_DEEP_SLEEP0 | 13 |  |
| `0x0B64` | `0x02D90` | MWV207REG_AHBX_DEEP_SLEEP1 | 12 |  |
| `0x0B65` | `0x02D94` | MWV207REG_AHBX_DEEP_SLEEP2 | 8 |  |
| `0x0B66` | `0x02D98` | MWV207REG_AHBX_CACHE | 2 |  |
| `0x0C80` | `0x03200` | MWV207REG_AHBX_TOTAL_RD_CPU | 1 |  |
| `0x0C81` | `0x03204` | MWV207REG_AHBX_TOTAL_WR_CPU | 1 |  |
| `0x0C82` | `0x03208` | MWV207REG_AHBX_TOTAL_WR_BURST_CPU | 1 |  |
| `0x0C83` | `0x0320C` | MWV207REG_AHBX_TOTAL_WR_REQ_CPU | 1 |  |
| `0x0C84` | `0x03210` | MWV207REG_AHBX_TOTAL_WR_LAST_CPU | 1 |  |
| `0x0C85` | `0x03214` | MWV207REG_AHBX_TOTAL_RD_BURST_CPU | 1 |  |
| `0x0C86` | `0x03218` | MWV207REG_AHBX_TOTAL_RD_REQ_CPU | 1 |  |
| `0x0C87` | `0x0321C` | MWV207REG_AHBX_TOTAL_RD_LAST_CPU | 1 |  |
| `0x0C88` | `0x03220` | MWV207REG_AHBX_TOTAL_RD_ISP | 1 |  |
| `0x0C89` | `0x03224` | MWV207REG_AHBX_TOTAL_WR_ISP | 1 |  |
| `0x0C8A` | `0x03228` | MWV207REG_AHBX_TOTAL_WR_BURST_ISP | 1 |  |
| `0x0C8B` | `0x0322C` | MWV207REG_AHBX_TOTAL_WR_REQ_ISP | 1 |  |
| `0x0C8C` | `0x03230` | MWV207REG_AHBX_TOTAL_WR_LAST_ISP | 1 |  |
| `0x0C8D` | `0x03234` | MWV207REG_AHBX_TOTAL_RD_BURST_ISP | 1 |  |
| `0x0C8E` | `0x03238` | MWV207REG_AHBX_TOTAL_RD_REQ_ISP | 1 |  |
| `0x0C8F` | `0x0323C` | MWV207REG_AHBX_TOTAL_RD_LAST_ISP | 1 |  |
| `0x0C90` | `0x03240` | MWV207REG_AHBX_TOTAL_RD_DDR | 1 |  |
| `0x0C91` | `0x03244` | MWV207REG_AHBX_TOTAL_WR_DDR | 1 |  |
| `0x0C92` | `0x03248` | MWV207REG_AHBX_TOTAL_WR_BURST_DDR | 1 |  |
| `0x0C93` | `0x0324C` | MWV207REG_AHBX_TOTAL_WR_REQ_DDR | 1 |  |
| `0x0C94` | `0x03250` | MWV207REG_AHBX_TOTAL_WR_LAST_DDR | 1 |  |
| `0x0C95` | `0x03254` | MWV207REG_AHBX_TOTAL_RD_BURST_DDR | 1 |  |
| `0x0C96` | `0x03258` | MWV207REG_AHBX_TOTAL_RD_REQ_DDR | 1 |  |
| `0x0C97` | `0x0325C` | MWV207REG_AHBX_TOTAL_RD_LAST_DDR | 1 |  |
| `0x0C98` | `0x03260` | MWV207REG_AHBX_MEM_COUNT | 1 |  |
| `0x0C9B` | `0x0326C` | MWV207REG_AHBX_PR_ACK | 1 |  |
| `0x0C9C` | `0x03270` | MWV207REG_AHBX_PU_ACK | 1 |  |
| `0x0C9D` | `0x03274` | MWV207REG_AHBX_CR_ACK | 1 |  |
| `0x0C9E` | `0x03278` | MWV207REG_AHBX_CU_ACK | 1 |  |
| `0x0C9F` | `0x0327C` | MWV207REG_AHBX_IN_FRM_START | 1 |  |
| `0x0CA0` | `0x03280` | MWV207REG_AHBX_OUT_FRM_START | 1 |  |
| `0x0CA1` | `0x03284` | MWV207REG_AHBX_FLEXA_PSI0 | 1 |  |
| `0x0CA2` | `0x03288` | MWV207REG_AHBX_FLEXA_PSI1 | 1 |  |
| `0x0CA3` | `0x0328C` | MWV207REG_AHBX_FLEXA_PSI2 | 1 |  |
| `0x0CA4` | `0x03290` | MWV207REG_AHBX_FLEXA_PSI3 | 1 |  |
| `0x0CA6` | `0x03298` | MWV207REG_AHBX_OCM_DEEP_SLEEP | 1 |  |
| `0x0CB4` | `0x032D0` | MWV207REG_AHBX_OCM_TOTAL_WR_REQ | 1 |  |
| `0x0CB5` | `0x032D4` | MWV207REG_AHBX_OCM_TOTAL_RD_REQ | 1 |  |
| `0x0CB6` | `0x032D8` | MWV207REG_AHBX_SBI_TO_PSI_DBG0 | 1 |  |
| `0x0CB7` | `0x032DC` | MWV207REG_AHBX_SBI_TO_PSI_DBG1 | 1 |  |
| `0x0CB8` | `0x032E0` | MWV207REG_AHBX_SBI_TO_PSI_DBG2 | 1 |  |
| `0x0CB9` | `0x032E4` | MWV207REG_AHBX_PSI_TO_SBI_DBG0 | 1 |  |
| `0x0CBA` | `0x032E8` | MWV207REG_AHBX_PSI_TO_SBI_DBG1 | 1 |  |
| `0x0CBB` | `0x032EC` | MWV207REG_AHBX_PSI_TO_SBI_DBG2 | 1 |  |
| `0x0CBC` | `0x032F0` | MWV207REG_AHBX_TRANSMITTER_DBG0 | 1 |  |
| `0x0CBD` | `0x032F4` | MWV207REG_AHBX_TRANSMITTER_DBG1 | 1 |  |
| `0x0CBE` | `0x032F8` | MWV207REG_AHBX_TRANSMITTER_DBG2 | 1 |  |
| `0x0CBF` | `0x032FC` | MWV207REG_AHBX_RECEIVER_DBG0 | 1 |  |
| `0x0CC0` | `0x03300` | MWV207REG_AHBX_RECEIVER_DBG1 | 1 |  |
| `0x0CC1` | `0x03304` | MWV207REG_AHBX_RECEIVER_DBG2 | 1 |  |

### `AHBSW` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0B1D` | `0x02C74` | MWV207REG_AHBSW_CONTROL_VXSBI0 | 4 |  |
| `0x0B1E` | `0x02C78` | MWV207REG_AHBSW_CONTROL_VXSBI1 | 4 |  |
| `0x0B1F` | `0x02C7C` | MWV207REG_AHBSW_CONTROL_VXSBI2 | 4 |  |

### `AHBXDMA` — 6 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0B51` | `0x02D44` | MWV207REG_AHBXDMA_RAW_STATUS | 27 |  |
| `0x0B52` | `0x02D48` | MWV207REG_AHBXDMA_RAW_STATUS2 | 18 |  |
| `0x0B53` | `0x02D4C` | MWV207REG_AHBXDMA_INTR_EN | 27 |  |
| `0x0B54` | `0x02D50` | MWV207REG_AHBXDMA_INTR_EN2 | 18 |  |
| `0x0B55` | `0x02D54` | MWV207REG_AHBXDMA_INTR_STATUS | 27 |  |
| `0x0B56` | `0x02D58` | MWV207REG_AHBXDMA_INTR_STATUS2 | 18 |  |

### `AHBXVIP` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0B67` | `0x02D9C` | MWV207REG_AHBXVIP_DEBUG | 1 |  |
| `0x0CA5` | `0x03294` | MWV207REG_AHBXVIP_SRAM_DEEP_SLEEP | 1 |  |
| `0x0CA7` | `0x0329C` | MWV207REG_AHBXVIP_WORK_DONE | 1 |  |

### `AHBXDDR` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0C99` | `0x03264` | MWV207REG_AHBXDDR_RD_CK_TOTAL | 1 |  |
| `0x0C9A` | `0x03268` | MWV207REG_AHBXDDR_WR_CK_TOTAL | 1 |  |

### `AHBXVIP0` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0CA8` | `0x032A0` | MWV207REG_AHBXVIP0_TOTAL_WR_REQ | 1 |  |
| `0x0CA9` | `0x032A4` | MWV207REG_AHBXVIP0_TOTAL_RD_REQ | 1 |  |

### `AHBXVIP1` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0CAA` | `0x032A8` | MWV207REG_AHBXVIP1_TOTAL_WR_REQ | 1 |  |
| `0x0CAB` | `0x032AC` | MWV207REG_AHBXVIP1_TOTAL_RD_REQ | 1 |  |

### `AHBXVIP2` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0CAC` | `0x032B0` | MWV207REG_AHBXVIP2_TOTAL_WR_REQ | 1 |  |
| `0x0CAD` | `0x032B4` | MWV207REG_AHBXVIP2_TOTAL_RD_REQ | 1 |  |

### `AHBXVIP3` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0CAE` | `0x032B8` | MWV207REG_AHBXVIP3_TOTAL_WR_REQ | 1 |  |
| `0x0CAF` | `0x032BC` | MWV207REG_AHBXVIP3_TOTAL_RD_REQ | 1 |  |

### `AHBXDMA0` — 5 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0CB0` | `0x032C0` | MWV207REG_AHBXDMA0_TOTAL_WR_REQ | 1 |  |
| `0x0CB1` | `0x032C4` | MWV207REG_AHBXDMA0_TOTAL_RD_REQ | 1 |  |
| `0x0CC2` | `0x03308` | MWV207REG_AHBXDMA0_DBG0 | 1 |  |
| `0x0CC3` | `0x0330C` | MWV207REG_AHBXDMA0_DBG1 | 1 |  |
| `0x0CC4` | `0x03310` | MWV207REG_AHBXDMA0_DBG2 | 1 |  |

### `AHBXDMA2` — 5 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0CB2` | `0x032C8` | MWV207REG_AHBXDMA2_TOTAL_WR_REQ | 1 |  |
| `0x0CB3` | `0x032CC` | MWV207REG_AHBXDMA2_TOTAL_RD_REQ | 1 |  |
| `0x0CC8` | `0x03320` | MWV207REG_AHBXDMA2_DBG0 | 1 |  |
| `0x0CC9` | `0x03324` | MWV207REG_AHBXDMA2_DBG1 | 1 |  |
| `0x0CCA` | `0x03328` | MWV207REG_AHBXDMA2_DBG2 | 1 |  |

### `AHBXDMA1` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0CC5` | `0x03314` | MWV207REG_AHBXDMA1_DBG0 | 1 |  |
| `0x0CC6` | `0x03318` | MWV207REG_AHBXDMA1_DBG1 | 1 |  |
| `0x0CC7` | `0x0331C` | MWV207REG_AHBXDMA1_DBG2 | 1 |  |

### `AHBXDMA3` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0CCB` | `0x0332C` | MWV207REG_AHBXDMA3_DBG0 | 1 |  |
| `0x0CCC` | `0x03330` | MWV207REG_AHBXDMA3_DBG1 | 1 |  |
| `0x0CCD` | `0x03334` | MWV207REG_AHBXDMA3_DBG2 | 1 |  |

### `AHBXDMA4` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0CCE` | `0x03338` | MWV207REG_AHBXDMA4_DBG0 | 1 |  |
| `0x0CCF` | `0x0333C` | MWV207REG_AHBXDMA4_DBG1 | 1 |  |
| `0x0CD0` | `0x03340` | MWV207REG_AHBXDMA4_DBG2 | 1 |  |

### `VARYING` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E08` | `0x03820` | MWV207REG_VARYING_PACKING | 8 |  |
| `0x0E0D` | `0x03834` | MWV207REG_VARYING_PACKING_EX | 8 |  |

### `OCCLUSION` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E09` | `0x03824` | MWV207REG_OCCLUSION_ADDRESS | 2 |  |
| `0x0E0C` | `0x03830` | MWV207REG_OCCLUSION_DATA | 1 |  |
| `0x0E18` | `0x03860` | MWV207REG_OCCLUSION_CONTROL | 1 |  |

### `COMPONENT` — 4 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E0A` | `0x03828` | MWV207REG_COMPONENT_TYPE_LOW | 16 |  |
| `0x0E0B` | `0x0382C` | MWV207REG_COMPONENT_TYPE_HIGH | 16 |  |
| `0x0E0E` | `0x03838` | MWV207REG_COMPONENT_TYPE_EX | 16 |  |
| `0x0E15` | `0x03854` | MWV207REG_COMPONENT_TYPE_EX2 | 16 |  |

### `WINDOW` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E0F` | `0x0383C` | MWV207REG_WINDOW_CONTROL | 3 |  |
| `0x0E10` | `0x03840` | MWV207REG_WINDOW_ORIGIN | 2 |  |
| `0x0E11` | `0x03844` | MWV207REG_WINDOW_SIZE | 2 |  |

### `STORE` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E12` | `0x03848` | MWV207REG_STORE_COUNTER | 17 |  |

### `PC` — 46 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E16` | `0x03858` | MWV207REG_PC_ADDRESS | 1 |  |
| `0x4C00` | `0x13000` | MWV207REG_PC_TIME_STAMP | 1 |  |
| `0x4C01` | `0x13004` | MWV207REG_PC_COMAMD_BYTES_FETCHED | 1 |  |
| `0x4C02` | `0x13008` | MWV207REG_PC_DRAW_COMMANDS | 1 |  |
| `0x4C03` | `0x1300C` | MWV207REG_PC_INDEX_BYTES_FETCHED | 1 |  |
| `0x4C04` | `0x13010` | MWV207REG_PC_INDEX_CACHE_HIT | 1 |  |
| `0x4C05` | `0x13014` | MWV207REG_PC_VERTEX_COUNT | 1 |  |
| `0x4C06` | `0x13018` | MWV207REG_PC_VERTEX_BYTES_FETCHED | 1 |  |
| `0x4C07` | `0x1301C` | MWV207REG_PC_TOTAL_VERTEX | 1 |  |
| `0x4C08` | `0x13020` | MWV207REG_PC_TOTAL_PRIMITIVE | 1 |  |
| `0x4C09` | `0x13024` | MWV207REG_PC_TRIVIAL_REJECT_PRIMITIVE | 1 |  |
| `0x4C0A` | `0x13028` | MWV207REG_PC_CULLED_PRIMITIVE | 1 |  |
| `0x4C0B` | `0x1302C` | MWV207REG_PC_DEPTH_CLIPPED_PRIMITIVE | 1 |  |
| `0x4C0C` | `0x13030` | MWV207REG_PC_FRUSTUM_CLIPPED_PRIMITIVE | 1 |  |
| `0x4C0D` | `0x13034` | MWV207REG_PC_OUTPUT_PRIMITIVE | 1 |  |
| `0x4C0E` | `0x13038` | MWV207REG_PC_CULLED_TRIANGLES | 1 |  |
| `0x4C0F` | `0x1303C` | MWV207REG_PC_CULLED_LINES | 1 |  |
| `0x4C10` | `0x13040` | MWV207REG_PC_CULLED_RECTANGLES | 1 |  |
| `0x4C11` | `0x13044` | MWV207REG_PC_CLIPPED_TRIANGLES | 1 |  |
| `0x4C12` | `0x13048` | MWV207REG_PC_CLIPPED_LINES | 1 |  |
| `0x4C13` | `0x1304C` | MWV207REG_PC_CLIPPED_RECTANGLES | 1 |  |
| `0x4C14` | `0x13050` | MWV207REG_PC_RASTERIZED_PRIMITIVES | 1 |  |
| `0x4C15` | `0x13054` | MWV207REG_PC_TOTAL_QUADS | 1 |  |
| `0x4C16` | `0x13058` | MWV207REG_PC_CULLED_HZ | 1 |  |
| `0x4C17` | `0x1305C` | MWV207REG_PC_CULLED_EEZ | 1 |  |
| `0x4C18` | `0x13060` | MWV207REG_PC_VALID_QUADS | 1 |  |
| `0x4C19` | `0x13064` | MWV207REG_PC_VALID_PIXELS | 1 |  |
| `0x4C20` | `0x13080` | MWV207REG_PC_COLOR_KILLED_PIXELS | 1 |  |
| `0x4C28` | `0x130A0` | MWV207REG_PC_DEPTH_KILLED_PIXELS | 1 |  |
| `0x4C30` | `0x130C0` | MWV207REG_PC_STENCIL_KILLED_PIXELS | 1 |  |
| `0x4C38` | `0x130E0` | MWV207REG_PC_COLOR_RENDERED_PIXELS | 1 |  |
| `0x4C40` | `0x13100` | MWV207REG_PC_DEPTH_RENDERED_PIXELS | 1 |  |
| `0x4C90` | `0x13240` | MWV207REG_PC_TEXEL_REQUESTS | 1 |  |
| `0x4C91` | `0x13244` | MWV207REG_PC_BILINEAR_REQUESTS | 1 |  |
| `0x4C92` | `0x13248` | MWV207REG_PC_TRILINEAR_REQUESTS | 1 |  |
| `0x4C93` | `0x1324C` | MWV207REG_PC_DISCARDED_REQUESTS | 1 |  |
| `0x4C94` | `0x13250` | MWV207REG_PC_VERTEX_INSTRUCTIONS | 1 |  |
| `0x4C95` | `0x13254` | MWV207REG_PC_VERTEX_BRANCHES | 1 |  |
| `0x4C96` | `0x13258` | MWV207REG_PC_VERTEX_TEXTURES | 1 |  |
| `0x4C97` | `0x1325C` | MWV207REG_PC_VERTICES_RENDERED | 1 |  |
| `0x4CB0` | `0x132C0` | MWV207REG_PC_PIXEL_INSTRUCTIONS | 1 |  |
| `0x4CB1` | `0x132C4` | MWV207REG_PC_PIXEL_BRANCHES | 1 |  |
| `0x4CB2` | `0x132C8` | MWV207REG_PC_PIXEL_TEXTURES | 1 |  |
| `0x4CB3` | `0x132CC` | MWV207REG_PC_PIXELS_RENDERED | 1 |  |
| `0x4CB4` | `0x132D0` | MWV207REG_PC_LOAD_INSTRUCTIONS | 1 |  |
| `0x4CB5` | `0x132D4` | MWV207REG_PC_STORE_INSTRUCTIONS | 1 |  |

### `CLOCK` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E19` | `0x03864` | MWV207REG_CLOCK_CONTROL | 3 |  |

### `FENCE` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E1A` | `0x03868` | MWV207REG_FENCE_ADDRESS | 2 |  |
| `0x0E1B` | `0x0386C` | MWV207REG_FENCE_DATA | 1 |  |
| `0x0E26` | `0x03898` | MWV207REG_FENCE_DATA_HIGH | 1 |  |

### `TOGGLE` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E1F` | `0x0387C` | MWV207REG_TOGGLE_SWITCH | 4 |  |

### `USC` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E21` | `0x03884` | MWV207REG_USC_CONTROL | 6 |  |

### `SEMANTIC` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E22` | `0x03888` | MWV207REG_SEMANTIC_LOCATION | 4 |  |

### `COLOR` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E28` | `0x038A0` | MWV207REG_COLOR_SLICE | 1 |  |

### `APP` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E40` | `0x03900` | MWV207REG_APP_CONTEXT_ID_LOW | 1 |  |
| `0x0E41` | `0x03904` | MWV207REG_APP_CONTEXT_ID_HIGH | 1 |  |

### `CLUSTER` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E42` | `0x03908` | MWV207REG_CLUSTER_CHECKER_BOARD | 1 |  |
| `0x0E44` | `0x03910` | MWV207REG_CLUSTER_CONTROL | 1 |  |
| `0x0E60` | `0x03980` | MWV207REG_CLUSTER_CONTROL1 | 1 |  |

### `TRIGGER` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E43` | `0x0390C` | MWV207REG_TRIGGER_NN | 1 |  |
| `0x0E4D` | `0x03934` | MWV207REG_TRIGGER_TP | 1 |  |
| `0x5F9B` | `0x17E6C` | MWV207REG_TRIGGER_SCALER | 6 |  |

### `POWER` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E48` | `0x03920` | MWV207REG_POWER_CLUSTER_CONTROL | 1 |  |
| `0x0E64` | `0x03990` | MWV207REG_POWER_CLUSTER_CONTROL1 | 1 |  |

### `CONFIG` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E4C` | `0x03930` | MWV207REG_CONFIG_NN | 15 |  |
| `0x0E53` | `0x0394C` | MWV207REG_CONFIG_TP | 4 |  |
| `0x0E72` | `0x039C8` | MWV207REG_CONFIG_NN2 | 1 |  |

### `SRAM` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E4E` | `0x03938` | MWV207REG_SRAM_REMAP_START_ADDRESS | 1 |  |
| `0x0E51` | `0x03944` | MWV207REG_SRAM_IMAGE_STREAMING_BASE_ADDRESS | 1 |  |
| `0x0E52` | `0x03948` | MWV207REG_SRAM_IMAGE_STREAMING_BUFFER_SIZE | 1 |  |

### `ON` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E4F` | `0x0393C` | MWV207REG_ON_CHIP_BUFFER_REMAP_START_ADDRESS | 1 |  |
| `0x0E50` | `0x03940` | MWV207REG_ON_CHIP_BUFFER_REMAP_END_ADDRESS | 1 |  |

### `VIP` — 9 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E54` | `0x03950` | MWV207REG_VIP_FLUSH | 4 |  |
| `0x0E5A` | `0x03968` | MWV207REG_VIP_MISC | 8 |  |
| `0x0E6B` | `0x039AC` | MWV207REG_VIP_CLOCK_GATE | 28 |  |
| `0x5FB3` | `0x17ECC` | MWV207REG_VIP_TENSOR_IO | 1 |  |
| `0x5FB4` | `0x17ED0` | MWV207REG_VIP_TENSOR_IO_WR | 1 |  |
| `0x5FCA` | `0x17F28` | MWV207REG_VIP_DEBUG | 1 |  |
| `0x6180` | `0x18600` | MWV207REG_VIP_SRAM_IMAGE_STREAM_BASE_ADDRESS | 1 |  |
| `0x6181` | `0x18604` | MWV207REG_VIP_SRAM_IMAGE_STREAM_BUFFER_SIZE | 1 |  |
| `0x6182` | `0x18608` | MWV207REG_VIP_REMAP_START_ADDRESS | 1 |  |

### `VIP40` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E59` | `0x03964` | MWV207REG_VIP40_BITS_VA | 4 |  |

### `SP` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E5B` | `0x0396C` | MWV207REG_SP_INSTR_MEM_ADDR | 1 |  |
| `0x0E5C` | `0x03970` | MWV207REG_SP_INSTR_MEM_DATA_LOW | 1 |  |
| `0x0E5D` | `0x03974` | MWV207REG_SP_INSTR_MEM_DATA_HIGH | 1 |  |

### `CULL` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E6C` | `0x039B0` | MWV207REG_CULL_DISTANCE_LOCATION0 | 4 |  |
| `0x0E6D` | `0x039B4` | MWV207REG_CULL_DISTANCE_LOCATION1 | 4 |  |

### `CLIP` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E6E` | `0x039B8` | MWV207REG_CLIP_DISTANCE_LOCATION0 | 4 |  |
| `0x0E6F` | `0x039BC` | MWV207REG_CLIP_DISTANCE_LOCATION1 | 4 |  |

### `VIEWPORT` — 7 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x0E70` | `0x039C0` | MWV207REG_VIEWPORT_ARRAY_INDEX_LOCATION | 1 |  |
| `0x7A00` | `0x1E800` | MWV207REG_VIEWPORT_XSCALE_EX | 1 |  |
| `0x7A10` | `0x1E840` | MWV207REG_VIEWPORT_YSCALE_EX | 1 |  |
| `0x7A20` | `0x1E880` | MWV207REG_VIEWPORT_ZSCALE_EX | 1 |  |
| `0x7A30` | `0x1E8C0` | MWV207REG_VIEWPORT_XOFFSET_EX | 1 |  |
| `0x7A40` | `0x1E900` | MWV207REG_VIEWPORT_YOFFSET_EX | 1 |  |
| `0x7A50` | `0x1E940` | MWV207REG_VIEWPORT_ZOFFSET_EX | 1 |  |

### `TX3` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x40C0` | `0x10300` | MWV207REG_TX3_D | 3 |  |

### `PATTERN` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x4BA8` | `0x12EA0` | MWV207REG_PATTERN_FG_COLOR | 4 |  |
| `0x4BB0` | `0x12EC0` | MWV207REG_PATTERN_CONFIG | 1 |  |

### `TPCNV12` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x4BD8` | `0x12F60` | MWV207REG_TPCNV12_SRC_CONFIG | 6 |  |

### `PCHZ` — 4 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x4C1A` | `0x13068` | MWV207REG_PCHZ_CACHE_REQUESTS | 1 |  |
| `0x4C1B` | `0x1306C` | MWV207REG_PCHZ_CACHE_MISSES | 1 |  |
| `0x4C1C` | `0x13070` | MWV207REG_PCHZ_BYTES_FETCHED | 1 |  |
| `0x4C88` | `0x13220` | MWV207REG_PCHZ_BYTES_WRITTEN | 1 |  |

### `PCEZ` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x4C1D` | `0x13074` | MWV207REG_PCEZ_CACHE_REQUESTS | 1 |  |
| `0x4C1E` | `0x13078` | MWV207REG_PCEZ_CACHE_MISSES | 1 |  |
| `0x4C1F` | `0x1307C` | MWV207REG_PCEZ_BYTES_FETCHED | 1 |  |

### `PCC` — 4 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x4C48` | `0x13120` | MWV207REG_PCC_CACHE_REQUESTS | 1 |  |
| `0x4C50` | `0x13140` | MWV207REG_PCC_CACHE_MISSES | 1 |  |
| `0x4C58` | `0x13160` | MWV207REG_PCC_CACHE_BYTES_FETCHED | 1 |  |
| `0x4C60` | `0x13180` | MWV207REG_PCC_CACHE_BYTES_WRITTEN | 1 |  |

### `PCZ` — 4 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x4C68` | `0x131A0` | MWV207REG_PCZ_CACHE_REQUESTS | 1 |  |
| `0x4C70` | `0x131C0` | MWV207REG_PCZ_CACHE_MISSES | 1 |  |
| `0x4C78` | `0x131E0` | MWV207REG_PCZ_CACHE_BYTES_FETCHED | 1 |  |
| `0x4C80` | `0x13200` | MWV207REG_PCZ_CACHE_BYTES_WRITTEN | 1 |  |

### `PCTX` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x4C98` | `0x13260` | MWV207REG_PCTX_CACHE_REQUESTS | 1 |  |
| `0x4CA0` | `0x13280` | MWV207REG_PCTX_CACHE_MISSES | 1 |  |
| `0x4CA8` | `0x132A0` | MWV207REG_PCTX_BYTES_FETCHED | 1 |  |

### `PCL1` — 5 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x4CB6` | `0x132D8` | MWV207REG_PCL1_LOAD_REQUESTS | 1 |  |
| `0x4CB7` | `0x132DC` | MWV207REG_PCL1_STORE_REQUESTS | 1 |  |
| `0x4CB8` | `0x132E0` | MWV207REG_PCL1_MISSES | 1 |  |
| `0x4CB9` | `0x132E4` | MWV207REG_PCL1_BYTES_FETCHED | 1 |  |
| `0x4CBA` | `0x132E8` | MWV207REG_PCL1_BYTES_WRITTEN | 1 |  |

### `BLT` — 90 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5000` | `0x14000` | MWV207REG_BLT_SRC_ADDRESS | 1 |  |
| `0x5002` | `0x14008` | MWV207REG_BLT_SRC_CONFIG | 6 |  |
| `0x5003` | `0x1400C` | MWV207REG_BLT_SRC_CONFIG_EX | 18 |  |
| `0x5004` | `0x14010` | MWV207REG_BLT_SRC_TILE_STATUS_ADDRESS | 1 |  |
| `0x5005` | `0x14014` | MWV207REG_BLT_SRC_ORIGIN | 2 |  |
| `0x5006` | `0x14018` | MWV207REG_BLT_DEST_ADDRESS | 1 |  |
| `0x5008` | `0x14020` | MWV207REG_BLT_DEST_TILE_STATUS_ADDRESS | 1 |  |
| `0x5009` | `0x14024` | MWV207REG_BLT_DEST_CONFIG | 6 |  |
| `0x500A` | `0x14028` | MWV207REG_BLT_DEST_CONFIG_EX | 23 |  |
| `0x500B` | `0x1402C` | MWV207REG_BLT_DEST_ORIGIN | 2 |  |
| `0x500C` | `0x14030` | MWV207REG_BLT_WINDOW_SIZE | 2 |  |
| `0x500D` | `0x14034` | MWV207REG_BLT_SRC_CLEAR_VALUE | 1 |  |
| `0x500E` | `0x14038` | MWV207REG_BLT_SRC_CLEAR_VALUE64 | 1 |  |
| `0x500F` | `0x1403C` | MWV207REG_BLT_DST_CLEAR_VALUE | 1 |  |
| `0x5010` | `0x14040` | MWV207REG_BLT_DST_CLEAR_VALUE64 | 1 |  |
| `0x5011` | `0x14044` | MWV207REG_BLT_NEW_CLEAR_VALUE | 1 |  |
| `0x5012` | `0x14048` | MWV207REG_BLT_NEW_CLEAR_VALUE64 | 1 |  |
| `0x5013` | `0x1404C` | MWV207REG_BLT_CLEAR_MASK | 1 |  |
| `0x5014` | `0x14050` | MWV207REG_BLT_CLEAR_MASK64 | 1 |  |
| `0x5015` | `0x14054` | MWV207REG_BLT_COPY_SIZE | 1 |  |
| `0x5016` | `0x14058` | MWV207REG_BLT_DITHER_LOW | 8 |  |
| `0x5017` | `0x1405C` | MWV207REG_BLT_DITHER_HIGH | 8 |  |
| `0x5018` | `0x14060` | MWV207REG_BLT_EXECUTE | 7 |  |
| `0x5019` | `0x14064` | MWV207REG_BLT_GENERAL_CONFIG | 17 |  |
| `0x501A` | `0x14068` | MWV207REG_BLT_TILE_FILLER_COUNT | 1 |  |
| `0x501B` | `0x1406C` | MWV207REG_BLT_YUV_TILER_CONFIG | 6 |  |
| `0x501C` | `0x14070` | MWV207REG_BLT_YUV_TILER_SIZE | 2 |  |
| `0x501D` | `0x14074` | MWV207REG_BLT_YUV_TILER_YBASE | 1 |  |
| `0x501E` | `0x14078` | MWV207REG_BLT_YUV_TILER_YSTRIDE | 1 |  |
| `0x501F` | `0x1407C` | MWV207REG_BLT_YUV_TILER_UBASE | 1 |  |
| `0x5020` | `0x14080` | MWV207REG_BLT_YUV_TILER_USTRIDE | 1 |  |
| `0x5021` | `0x14084` | MWV207REG_BLT_YUV_TILER_VBASE | 1 |  |
| `0x5022` | `0x14088` | MWV207REG_BLT_YUV_TILER_VSTRIDE | 1 |  |
| `0x5023` | `0x1408C` | MWV207REG_BLT_YUV_TILER_DEST_BASE | 1 |  |
| `0x5024` | `0x14090` | MWV207REG_BLT_YUV_TILER_DEST_STRIDE | 1 |  |
| `0x5025` | `0x14094` | MWV207REG_BLT_SRC_SURFACE_SIZE | 2 |  |
| `0x5026` | `0x14098` | MWV207REG_BLT_SRC_ROT_CONFIG | 6 |  |
| `0x5027` | `0x1409C` | MWV207REG_BLT_BLOCK_SIZE | 2 |  |
| `0x5028` | `0x140A0` | MWV207REG_BLT_TILE_SIZE | 2 |  |
| `0x5029` | `0x140A4` | MWV207REG_BLT_FENCE_ADDRESS | 2 |  |
| `0x502A` | `0x140A8` | MWV207REG_BLT_FENCE_DATA | 1 |  |
| `0x502B` | `0x140AC` | MWV207REG_BLT_CACHE_FLUSH | 2 |  |
| `0x502C` | `0x140B0` | MWV207REG_BLT_LOD_CONFIG | 4 |  |
| `0x502D` | `0x140B4` | MWV207REG_BLT_FENCE_DATA_HIGH | 1 |  |
| `0x502E` | `0x140B8` | MWV207REG_BLT_GENERAL_CONTROL | 1 |  |
| `0x502F` | `0x140BC` | MWV207REG_BLT_SWIZZLE_EX | 8 |  |
| `0x5030` | `0x140C0` | MWV207REG_BLT_DEST_LOD_ADDRESS | 1 |  |
| `0x503D` | `0x140F4` | MWV207REG_BLT_SRC_BUFFER_END_ADDRESS | 1 |  |
| `0x503E` | `0x140F8` | MWV207REG_BLT_SRC_UBUFFER_END_ADDRESS | 1 |  |
| `0x503F` | `0x140FC` | MWV207REG_BLT_SRC_VBUFFER_END_ADDRESS | 1 |  |
| `0x5040` | `0x14100` | MWV207REG_BLT_GAMMA_TABLE | 4 |  |
| `0x5080` | `0x14200` | MWV207REG_BLT_DEGAMMA_TABLE | 4 |  |
| `0x50C0` | `0x14300` | MWV207REG_BLT_DEST_LOD_STRIDE | 1 |  |
| `0x50CD` | `0x14334` | MWV207REG_BLT_DST_BUFFER_END_ADDRESS | 1 |  |
| `0x50CE` | `0x14338` | MWV207REG_BLT_CLUSTER_CONTROL | 1 |  |
| `0x50CF` | `0x1433C` | MWV207REG_BLT_DEST_FLIP_SIZE | 2 |  |
| `0x50D0` | `0x14340` | MWV207REG_BLT_DEST_LOD_END_ADDRESS | 1 |  |
| `0x50DD` | `0x14374` | MWV207REG_BLT_SRC_CLEAR_VALUE96 | 1 |  |
| `0x50DE` | `0x14378` | MWV207REG_BLT_SRC_CLEAR_VALUE128 | 1 |  |
| `0x50DF` | `0x1437C` | MWV207REG_BLT_DST_CLEAR_VALUE96 | 1 |  |
| `0x50E0` | `0x14380` | MWV207REG_BLT_DST_CLEAR_VALUE128 | 1 |  |
| `0x50E1` | `0x14384` | MWV207REG_BLT_NEW_CLEAR_VALUE96 | 1 |  |
| `0x50E2` | `0x14388` | MWV207REG_BLT_NEW_CLEAR_VALUE128 | 1 |  |
| `0x50E3` | `0x1438C` | MWV207REG_BLT_CLEAR_MASK96 | 1 |  |
| `0x50E4` | `0x14390` | MWV207REG_BLT_CLEAR_MASK128 | 1 |  |
| `0x50E5` | `0x14394` | MWV207REG_BLT_DEST_UPLANE_ADDRESS | 1 |  |
| `0x50E6` | `0x14398` | MWV207REG_BLT_DEST_UPLANE_CONFIG | 1 |  |
| `0x50E7` | `0x1439C` | MWV207REG_BLT_DEST_UPLANE_END_ADDRESS | 1 |  |
| `0x50E8` | `0x143A0` | MWV207REG_BLT_DEST_VPLANE_ADDRESS | 1 |  |
| `0x50E9` | `0x143A4` | MWV207REG_BLT_DEST_VPLANE_CONFIG | 1 |  |
| `0x50EA` | `0x143A8` | MWV207REG_BLT_DEST_VPLANE_END_ADDRESS | 1 |  |
| `0x50EB` | `0x143AC` | MWV207REG_BLT_QOS_CONFIG | 6 |  |
| `0x50EC` | `0x143B0` | MWV207REG_BLT_SRC1_ADDRESS | 1 |  |
| `0x50EE` | `0x143B8` | MWV207REG_BLT_SRC1_CONFIG | 6 |  |
| `0x50EF` | `0x143BC` | MWV207REG_BLT_SRC1_CONFIG_EX | 17 |  |
| `0x50F0` | `0x143C0` | MWV207REG_BLT_SRC1_TILE_STATUS_ADDRESS | 1 |  |
| `0x50F1` | `0x143C4` | MWV207REG_BLT_SRC1_ORIGIN | 2 |  |
| `0x50F2` | `0x143C8` | MWV207REG_BLT_SRC1_BUFFER_END_ADDRESS | 1 |  |
| `0x50F3` | `0x143CC` | MWV207REG_BLT_DEST1_TILE_STATUS_ADDRESS | 1 |  |
| `0x50F4` | `0x143D0` | MWV207REG_BLT_DEST1_ADDRESS | 1 |  |
| `0x50F6` | `0x143D8` | MWV207REG_BLT_DEST1_CONFIG | 6 |  |
| `0x50F7` | `0x143DC` | MWV207REG_BLT_DEST1_CONFIG_EX | 23 |  |
| `0x50F8` | `0x143E0` | MWV207REG_BLT_DEST1_ORIGIN | 2 |  |
| `0x50F9` | `0x143E4` | MWV207REG_BLT_DEST1_BUFFER_END_ADDRESS | 1 |  |
| `0x50FA` | `0x143E8` | MWV207REG_BLT_TILE_FILLER_CLUSTER_TS_OFFSET | 1 |  |
| `0x50FB` | `0x143EC` | MWV207REG_BLT_SRC_HIGH_ADDRESS | 1 |  |
| `0x50FC` | `0x143F0` | MWV207REG_BLT_SRC_BUFFER_HIGH_END_ADDRESS | 1 |  |
| `0x50FD` | `0x143F4` | MWV207REG_BLT_DEST_HIGH_ADDRESS | 1 |  |
| `0x50FE` | `0x143F8` | MWV207REG_BLT_DEST_BUFFER_HIGH_END_ADDRESS | 1 |  |
| `0x50FF` | `0x143FC` | MWV207REG_BLT_COPY_HIGH_SIZE | 1 |  |

### `SO` — 8 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5100` | `0x14400` | MWV207REG_SO_CONTROL | 7 |  |
| `0x5101` | `0x14404` | MWV207REG_SO_COLLECT | 2 |  |
| `0x5102` | `0x14408` | MWV207REG_SO_BIN_ORIGIN | 2 |  |
| `0x5103` | `0x1440C` | MWV207REG_SO_BIN_SIZE | 4 |  |
| `0x5104` | `0x14410` | MWV207REG_SO_STREAM_SIZE | 4 |  |
| `0x5110` | `0x14440` | MWV207REG_SO_ADDRESS | 1 |  |
| `0x5120` | `0x14480` | MWV207REG_SO_WINDOW_ORIGIN | 2 |  |
| `0x5130` | `0x144C0` | MWV207REG_SO_WINDOW_SIZE | 2 |  |

### `TCS` — 17 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5280` | `0x14A00` | MWV207REG_TCS_START | 1 |  |
| `0x5281` | `0x14A04` | MWV207REG_TCS_RELATIVE_END | 1 |  |
| `0x5282` | `0x14A08` | MWV207REG_TCS_INSTRUCTION | 1 |  |
| `0x5283` | `0x14A0C` | MWV207REG_TCS_INSTRUCTION_PREFETCH | 1 |  |
| `0x5284` | `0x14A10` | MWV207REG_TCS_INSTRUCTION_PREFETCH_RELATIVE_END | 1 |  |
| `0x5285` | `0x14A14` | MWV207REG_TCS_CONTROL | 4 |  |
| `0x5286` | `0x14A18` | MWV207REG_TCS_THROTTLE | 3 |  |
| `0x5287` | `0x14A1C` | MWV207REG_TCS_ATTRIBUTE | 4 |  |
| `0x5288` | `0x14A20` | MWV207REG_TCS_OUTPUT | 4 |  |
| `0x5290` | `0x14A40` | MWV207REG_TCS_ATTRIBUTE_EX | 1 |  |
| `0x5291` | `0x14A44` | MWV207REG_TCS_UNIFORM_BASE | 1 |  |
| `0x5292` | `0x14A48` | MWV207REG_TCS_UNIFORM_LOCK | 1 |  |
| `0x5293` | `0x14A4C` | MWV207REG_TCS_SAMPLER_BASE | 2 |  |
| `0x5294` | `0x14A50` | MWV207REG_TCS_SAMPLER_LOCK | 2 |  |
| `0x5295` | `0x14A54` | MWV207REG_TCS_ATTRIBUTE_GLOBAL_CONTROL | 3 |  |
| `0x5584` | `0x15610` | MWV207REG_TCS_BUDDY | 1 |  |
| `0x5589` | `0x15624` | MWV207REG_TCS_LS_ALLOC | 1 |  |

### `TPG` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x52C0` | `0x14B00` | MWV207REG_TPG_CONTROL | 6 |  |

### `TES` — 17 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x52C1` | `0x14B04` | MWV207REG_TES_START | 1 |  |
| `0x52C2` | `0x14B08` | MWV207REG_TES_RELATIVE_END | 1 |  |
| `0x52C3` | `0x14B0C` | MWV207REG_TES_INSTRUCTION | 1 |  |
| `0x52C4` | `0x14B10` | MWV207REG_TES_INSTRUCTION_PREFETCH | 1 |  |
| `0x52C5` | `0x14B14` | MWV207REG_TES_INSTRUCTION_PREFETCH_RELATIVE_END | 1 |  |
| `0x52C6` | `0x14B18` | MWV207REG_TES_CONTROL | 3 |  |
| `0x52C7` | `0x14B1C` | MWV207REG_TES_THROTTLE | 3 |  |
| `0x52C8` | `0x14B20` | MWV207REG_TES_ATTRIBUTE | 2 |  |
| `0x52C9` | `0x14B24` | MWV207REG_TES_UNIFORM_BASE | 1 |  |
| `0x52CA` | `0x14B28` | MWV207REG_TES_UNIFORM_LOCK | 1 |  |
| `0x52CB` | `0x14B2C` | MWV207REG_TES_SAMPLER_BASE | 2 |  |
| `0x52CC` | `0x14B30` | MWV207REG_TES_SAMPLER_LOCK | 2 |  |
| `0x52CD` | `0x14B34` | MWV207REG_TES_THROTTLE_EX | 2 |  |
| `0x52CE` | `0x14B38` | MWV207REG_TES_ATTRIBUTE_GLOBAL_CONTROL | 3 |  |
| `0x52D0` | `0x14B40` | MWV207REG_TES_OUTPUT | 4 |  |
| `0x5585` | `0x15614` | MWV207REG_TES_BUDDY | 1 |  |
| `0x558A` | `0x15628` | MWV207REG_TES_LS_ALLOC | 1 |  |

### `TEXTURE` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5315` | `0x14C54` | MWV207REG_TEXTURE_DESCRIPTOR_ADDR_TABLE_ADDR | 1 |  |

### `SAMPLER` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5316` | `0x14C58` | MWV207REG_SAMPLER_ADDR_TABLE_ADDR | 1 |  |
| `0x5317` | `0x14C5C` | MWV207REG_SAMPLER_COMMAND | 3 |  |
| `0x5318` | `0x14C60` | MWV207REG_SAMPLER_INVALIDATE | 1 |  |

### `TPC` — 27 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5320` | `0x14C80` | MWV207REG_TPC_GENERAL_CONTROL | 1 |  |
| `0x5321` | `0x14C84` | MWV207REG_TPC_ENC_HDR_ADDR | 1 |  |
| `0x5322` | `0x14C88` | MWV207REG_TPC_ENC_BODY_ADDR | 1 |  |
| `0x5323` | `0x14C8C` | MWV207REG_TPC_ENC_FORMAT | 5 |  |
| `0x5324` | `0x14C90` | MWV207REG_TPC_ENC_SIZE | 2 |  |
| `0x5325` | `0x14C94` | MWV207REG_TPC_ENC_CONF | 2 |  |
| `0x5326` | `0x14C98` | MWV207REG_TPC_DEC_CONFIG | 3 |  |
| `0x5327` | `0x14C9C` | MWV207REG_TPC_SURFACE_CFG | 9 |  |
| `0x5328` | `0x14CA0` | MWV207REG_TPC_DEC_HOST_INIT | 1 |  |
| `0x5329` | `0x14CA4` | MWV207REG_TPC_SURFACE0_HDR_ADDR | 1 |  |
| `0x532A` | `0x14CA8` | MWV207REG_TPC_SURFACE0_FORMAT | 3 |  |
| `0x532B` | `0x14CAC` | MWV207REG_TPC_SURFACE1_HDR_ADDR | 1 |  |
| `0x532C` | `0x14CB0` | MWV207REG_TPC_SURFACE1_FORMAT | 3 |  |
| `0x532D` | `0x14CB4` | MWV207REG_TPC_SURFACE2_HDR_ADDR | 1 |  |
| `0x532E` | `0x14CB8` | MWV207REG_TPC_SURFACE2_FORMAT | 3 |  |
| `0x532F` | `0x14CBC` | MWV207REG_TPC_SURFACE3_HDR_ADDR | 1 |  |
| `0x5330` | `0x14CC0` | MWV207REG_TPC_SURFACE3_FORMAT | 3 |  |
| `0x5331` | `0x14CC4` | MWV207REG_TPC_SURFACE4_HDR_ADDR | 1 |  |
| `0x5332` | `0x14CC8` | MWV207REG_TPC_SURFACE4_FORMAT | 3 |  |
| `0x5333` | `0x14CCC` | MWV207REG_TPC_SURFACE5_HDR_ADDR | 1 |  |
| `0x5334` | `0x14CD0` | MWV207REG_TPC_SURFACE5_FORMAT | 3 |  |
| `0x5335` | `0x14CD4` | MWV207REG_TPC_SURFACE6_HDR_ADDR | 1 |  |
| `0x5336` | `0x14CD8` | MWV207REG_TPC_SURFACE6_FORMAT | 3 |  |
| `0x5337` | `0x14CDC` | MWV207REG_TPC_SURFACE7_HDR_ADDR | 1 |  |
| `0x5338` | `0x14CE0` | MWV207REG_TPC_SURFACE7_FORMAT | 3 |  |
| `0x5339` | `0x14CE4` | MWV207REG_TPC_SURFACE8_HDR_ADDR | 1 |  |
| `0x533A` | `0x14CE8` | MWV207REG_TPC_SURFACE8_FORMAT | 3 |  |

### `ROI` — 4 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5F90` | `0x17E40` | MWV207REG_ROI_BASE_Y | 1 |  |
| `0x5F91` | `0x17E44` | MWV207REG_ROI_BASE_U | 1 |  |
| `0x5F92` | `0x17E48` | MWV207REG_ROI_BASE_V | 1 |  |
| `0x5FA4` | `0x17E90` | MWV207REG_ROI_TL_COOR | 2 |  |

### `OUT` — 5 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5F94` | `0x17E50` | MWV207REG_OUT_IMAGE_BASE_R | 1 |  |
| `0x5F95` | `0x17E54` | MWV207REG_OUT_IMAGE_BASE_G | 1 |  |
| `0x5F96` | `0x17E58` | MWV207REG_OUT_IMAGE_BASE_B | 1 |  |
| `0x5F97` | `0x17E5C` | MWV207REG_OUT_IMAGE | 2 |  |
| `0x5F9C` | `0x17E70` | MWV207REG_OUT_STANDING_REQUST | 1 |  |

### `SCALE` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5F98` | `0x17E60` | MWV207REG_SCALE_X | 1 |  |
| `0x5F99` | `0x17E64` | MWV207REG_SCALE_Y | 1 |  |

### `IN` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5F9A` | `0x17E68` | MWV207REG_IN_IMAGE_INIT_ERROR | 2 |  |
| `0x5FA3` | `0x17E8C` | MWV207REG_IN_IMAGE | 2 |  |
| `0x5FA5` | `0x17E94` | MWV207REG_IN_IMAGE_INIT_ERROR_INT | 2 |  |

### `YUV2` — 5 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5F9D` | `0x17E74` | MWV207REG_YUV2_RGB_COEF0 | 3 |  |
| `0x5F9E` | `0x17E78` | MWV207REG_YUV2_RGB_COEF1 | 2 |  |
| `0x5F9F` | `0x17E7C` | MWV207REG_YUV2_RGB_COEF2 | 1 |  |
| `0x5FA0` | `0x17E80` | MWV207REG_YUV2_RGB_COEF3 | 1 |  |
| `0x5FA1` | `0x17E84` | MWV207REG_YUV2_RGB_COEF4 | 1 |  |

### `CLAMP` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5FA6` | `0x17E98` | MWV207REG_CLAMP_R | 2 |  |
| `0x5FA7` | `0x17E9C` | MWV207REG_CLAMP_G | 2 |  |
| `0x5FA8` | `0x17EA0` | MWV207REG_CLAMP_B | 2 |  |

### `TTD` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5FB0` | `0x17EC0` | MWV207REG_TTD_MISC | 19 |  |

### `PTN` — 6 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5FB5` | `0x17ED4` | MWV207REG_PTN_GEN_BAYER_IMAGE | 2 |  |
| `0x5FB6` | `0x17ED8` | MWV207REG_PTN_GEN_RED | 2 |  |
| `0x5FB7` | `0x17EDC` | MWV207REG_PTN_GEN_GREENS | 2 |  |
| `0x5FB8` | `0x17EE0` | MWV207REG_PTN_GEN_BLUE | 2 |  |
| `0x5FB9` | `0x17EE4` | MWV207REG_PTN_GEN_INC_Y0 | 2 |  |
| `0x5FBA` | `0x17EE8` | MWV207REG_PTN_GEN_INC_Y1 | 1 |  |

### `CHECKSUM` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5FBB` | `0x17EEC` | MWV207REG_CHECKSUM_EXPECTED_CHECKSUM_RED | 1 |  |
| `0x5FBC` | `0x17EF0` | MWV207REG_CHECKSUM_EXPECTED_CHECKSUM_GREEN | 1 |  |
| `0x5FBD` | `0x17EF4` | MWV207REG_CHECKSUM_EXPECTED_CHECKSUM_BLUE | 1 |  |

### `SW` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5FBF` | `0x17EFC` | MWV207REG_SW_CONTROL_VXSBI0 | 2 |  |

### `DENOISE` — 6 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5FC0` | `0x17F00` | MWV207REG_DENOISE_POST_PROCESS_C0 | 1 |  |
| `0x5FC1` | `0x17F04` | MWV207REG_DENOISE_POST_PROCESS_C1 | 1 |  |
| `0x5FC2` | `0x17F08` | MWV207REG_DENOISE_POST_PROCESS_C2 | 1 |  |
| `0x5FC3` | `0x17F0C` | MWV207REG_DENOISE_POST_PROCESS_C3 | 1 |  |
| `0x5FC4` | `0x17F10` | MWV207REG_DENOISE_POST_PROCESS_NN_OUT_SCALE | 1 |  |
| `0x5FC5` | `0x17F14` | MWV207REG_DENOISE_POST_PROCESS_CLAMP_MIN_MAX | 2 |  |

### `XDEEP` — 3 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5FC6` | `0x17F18` | MWV207REG_XDEEP_SLEEP0 | 9 |  |
| `0x5FC7` | `0x17F1C` | MWV207REG_XDEEP_SLEEP1 | 4 |  |
| `0x5FC8` | `0x17F20` | MWV207REG_XDEEP_SLEEP2 | 1 |  |

### `XVIP` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5FCB` | `0x17F2C` | MWV207REG_XVIP_DEBUG | 1 |  |

### `COMP` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x5FCC` | `0x17F30` | MWV207REG_COMP_DECOMP | 2 |  |

### `DEC` — 24 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x6000` | `0x18000` | MWV207REG_DEC_READ_CONFIG | 10 |  |
| `0x6010` | `0x18040` | MWV207REG_DEC_WRITE_CONFIG | 7 |  |
| `0x6020` | `0x18080` | MWV207REG_DEC_READ_BUFFER_BASE | 1 |  |
| `0x6030` | `0x180C0` | MWV207REG_DEC_READ_CACHE_BASE | 1 |  |
| `0x6040` | `0x18100` | MWV207REG_DEC_WRITE_BUFFER_BASE | 1 |  |
| `0x6050` | `0x18140` | MWV207REG_DEC_WRITE_CACHE_BASE | 1 |  |
| `0x6060` | `0x18180` | MWV207REG_DEC_CONTROL | 17 |  |
| `0x6061` | `0x18184` | MWV207REG_DEC_INTR_ACKNOWLEDGE | 1 |  |
| `0x6062` | `0x18188` | MWV207REG_DEC_INTR_ENBL | 1 |  |
| `0x6063` | `0x1818C` | MWV207REG_DEC_TILE_STATUS_DEBUG | 1 |  |
| `0x6064` | `0x18190` | MWV207REG_DEC_ENCODER_DEBUG | 1 |  |
| `0x6065` | `0x18194` | MWV207REG_DEC_DECODER_DEBUG | 1 |  |
| `0x6076` | `0x181D8` | MWV207REG_DEC_DEBUG0 | 1 |  |
| `0x6077` | `0x181DC` | MWV207REG_DEC_DEBUG1 | 1 |  |
| `0x6078` | `0x181E0` | MWV207REG_DEC_DEBUG2 | 1 |  |
| `0x6079` | `0x181E4` | MWV207REG_DEC_DEBUG3 | 1 |  |
| `0x607A` | `0x181E8` | MWV207REG_DEC_CONTROL_EX | 1 |  |
| `0x6080` | `0x18200` | MWV207REG_DEC_CLEAR | 1 |  |
| `0x6090` | `0x18240` | MWV207REG_DEC_READ_EX_CONFIG | 10 |  |
| `0x60A0` | `0x18280` | MWV207REG_DEC_READ_STRIDE | 1 |  |
| `0x60B0` | `0x182C0` | MWV207REG_DEC_WRITE_EX_CONFIG | 10 |  |
| `0x60C0` | `0x18300` | MWV207REG_DEC_WRITE_STRIDE | 1 |  |
| `0x60D0` | `0x18340` | MWV207REG_DEC_READ_BUFFER_END | 1 |  |
| `0x60E0` | `0x18380` | MWV207REG_DEC_WRITE_BUFFER_END | 1 |  |

### `WD` — 2 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x6100` | `0x18400` | MWV207REG_WD_CONFIG | 4 |  |
| `0x6101` | `0x18404` | MWV207REG_WD_STATIC_CONFIG | 1 |  |

### `VERIF` — 1 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x6170` | `0x185C0` | MWV207REG_VERIF_PRIMITIVE_ID | 1 |  |

### `DEC400` — 80 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x6200` | `0x18800` | MWV207REG_DEC400_CONTROL | 21 |  |
| `0x6201` | `0x18804` | MWV207REG_DEC400_CONTROL_EX | 16 |  |
| `0x6202` | `0x18808` | MWV207REG_DEC400_CONTROL_EX2 | 8 |  |
| `0x6203` | `0x1880C` | MWV207REG_DEC400_INTR_ENBL | 1 |  |
| `0x6204` | `0x18810` | MWV207REG_DEC400_INTR_ENBL_EX | 1 |  |
| `0x6205` | `0x18814` | MWV207REG_DEC400_INTR_ENBL_EX2 | 1 |  |
| `0x6206` | `0x18818` | MWV207REG_DEC400_INTR_ACKNOWLEDGE | 1 |  |
| `0x6207` | `0x1881C` | MWV207REG_DEC400_INTR_ACKNOWLEDGE_EX | 1 |  |
| `0x6208` | `0x18820` | MWV207REG_DEC400_INTR_ACKNOWLEDGE_EX2 | 1 |  |
| `0x6209` | `0x18824` | MWV207REG_DEC400_TILE_STATUS_DEBUG | 1 |  |
| `0x620A` | `0x18828` | MWV207REG_DEC400_ENCODER_DEBUG | 1 |  |
| `0x620B` | `0x1882C` | MWV207REG_DEC400_DECODER_DEBUG | 1 |  |
| `0x620C` | `0x18830` | MWV207REG_DEC400_TOTAL_READS_IN | 1 |  |
| `0x620D` | `0x18834` | MWV207REG_DEC400_TOTAL_WRITES_IN | 1 |  |
| `0x620E` | `0x18838` | MWV207REG_DEC400_TOTAL_READ_BURSTS_IN | 1 |  |
| `0x620F` | `0x1883C` | MWV207REG_DEC400_TOTAL_WRITE_BURSTS_IN | 1 |  |
| `0x6210` | `0x18840` | MWV207REG_DEC400_TOTAL_READS_REQ_IN | 1 |  |
| `0x6211` | `0x18844` | MWV207REG_DEC400_TOTAL_WRITES_REQ_IN | 1 |  |
| `0x6212` | `0x18848` | MWV207REG_DEC400_TOTAL_READ_LASTS_IN | 1 |  |
| `0x6213` | `0x1884C` | MWV207REG_DEC400_TOTAL_WRITE_LASTS_IN | 1 |  |
| `0x6214` | `0x18850` | MWV207REG_DEC400_TOTAL_WRITE_RESPONSE_IN | 1 |  |
| `0x6215` | `0x18854` | MWV207REG_DEC400_TOTAL_READS_OUT | 1 |  |
| `0x6216` | `0x18858` | MWV207REG_DEC400_TOTAL_WRITES_OUT | 1 |  |
| `0x6217` | `0x1885C` | MWV207REG_DEC400_TOTAL_READ_BURSTS_OUT | 1 |  |
| `0x6218` | `0x18860` | MWV207REG_DEC400_TOTAL_WRITE_BURSTS_OUT | 1 |  |
| `0x6219` | `0x18864` | MWV207REG_DEC400_TOTAL_READS_REQ_OUT | 1 |  |
| `0x621A` | `0x18868` | MWV207REG_DEC400_TOTAL_WRITES_REQ_OUT | 1 |  |
| `0x621B` | `0x1886C` | MWV207REG_DEC400_TOTAL_READ_LASTS_OUT | 1 |  |
| `0x621C` | `0x18870` | MWV207REG_DEC400_TOTAL_WRITE_LASTS_OUT | 1 |  |
| `0x621D` | `0x18874` | MWV207REG_DEC400_TOTAL_WRITE_RESPONSE_OUT | 1 |  |
| `0x621E` | `0x18878` | MWV207REG_DEC400_STATUS | 3 |  |
| `0x621F` | `0x1887C` | MWV207REG_DEC400_DEBUG_INFO_SELECT | 2 |  |
| `0x6220` | `0x18880` | MWV207REG_DEC400_READ_CONFIG | 9 |  |
| `0x6240` | `0x18900` | MWV207REG_DEC400_READ_EX_CONFIG | 10 |  |
| `0x6260` | `0x18980` | MWV207REG_DEC400_WRITE_CONFIG | 8 |  |
| `0x6280` | `0x18A00` | MWV207REG_DEC400_WRITE_EX_CONFIG | 10 |  |
| `0x62A0` | `0x18A80` | MWV207REG_DEC400_READ_BUFFER_BASE | 1 |  |
| `0x62C0` | `0x18B00` | MWV207REG_DEC400_READ_BUFFER_BASE_EX | 1 |  |
| `0x62E0` | `0x18B80` | MWV207REG_DEC400_READ_BUFFER_END | 1 |  |
| `0x6300` | `0x18C00` | MWV207REG_DEC400_READ_BUFFER_END_EX | 1 |  |
| `0x6320` | `0x18C80` | MWV207REG_DEC400_READ_FLUSH_CACHE | 1 |  |
| `0x6340` | `0x18D00` | MWV207REG_DEC400_READ_FLUSH_CACHE_EX | 1 |  |
| `0x6360` | `0x18D80` | MWV207REG_DEC400_WRITE_BUFFER_BASE | 1 |  |
| `0x6380` | `0x18E00` | MWV207REG_DEC400_WRITE_BUFFER_BASE_EX | 1 |  |
| `0x63A0` | `0x18E80` | MWV207REG_DEC400_WRITE_BUFFER_END | 1 |  |
| `0x63C0` | `0x18F00` | MWV207REG_DEC400_WRITE_BUFFER_END_EX | 1 |  |
| `0x63E0` | `0x18F80` | MWV207REG_DEC400_WRITE_FLUSH_CACHE | 1 |  |
| `0x6400` | `0x19000` | MWV207REG_DEC400_WRITE_FLUSH_CACHE_EX | 1 |  |
| `0x6420` | `0x19080` | MWV207REG_DEC400_READ_CACHE_BASE | 1 |  |
| `0x6440` | `0x19100` | MWV207REG_DEC400_READ_CACHE_BASE_EX | 1 |  |
| `0x6460` | `0x19180` | MWV207REG_DEC400_WRITE_CACHE_BASE | 1 |  |
| `0x6480` | `0x19200` | MWV207REG_DEC400_WRITE_CACHE_BASE_EX | 1 |  |
| `0x64A0` | `0x19280` | MWV207REG_DEC400_DEBUG_INFO_OUT | 1 |  |
| `0x64A1` | `0x19284` | MWV207REG_DEC400_DEBUG0 | 1 |  |
| `0x64A2` | `0x19288` | MWV207REG_DEC400_DEBUG1 | 1 |  |
| `0x64A3` | `0x1928C` | MWV207REG_DEC400_DEBUG2 | 1 |  |
| `0x64A4` | `0x19290` | MWV207REG_DEC400_DEBUG3 | 1 |  |
| `0x64A5` | `0x19294` | MWV207REG_DEC400_DEBUG4 | 1 |  |
| `0x64A6` | `0x19298` | MWV207REG_DEC400_DEBUG5 | 1 |  |
| `0x64A7` | `0x1929C` | MWV207REG_DEC400_STATE_COMMIT | 1 |  |
| `0x64A8` | `0x192A0` | MWV207REG_DEC400_DEBUG6 | 1 |  |
| `0x64A9` | `0x192A4` | MWV207REG_DEC400_DEBUG7 | 1 |  |
| `0x64AA` | `0x192A8` | MWV207REG_DEC400_CR_DEBUG_TILE128_TYPE0 | 1 |  |
| `0x64AB` | `0x192AC` | MWV207REG_DEC400_CR_DEBUG_TILE128_TYPE1 | 1 |  |
| `0x64AC` | `0x192B0` | MWV207REG_DEC400_CR_DEBUG_TILE128_TYPE2 | 1 |  |
| `0x64AD` | `0x192B4` | MWV207REG_DEC400_CR_DEBUG_TILE128_TYPE3 | 1 |  |
| `0x64AE` | `0x192B8` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE0 | 1 |  |
| `0x64AF` | `0x192BC` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE1 | 1 |  |
| `0x64B0` | `0x192C0` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE2 | 1 |  |
| `0x64B1` | `0x192C4` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE3 | 1 |  |
| `0x64B2` | `0x192C8` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE4 | 1 |  |
| `0x64B3` | `0x192CC` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE5 | 1 |  |
| `0x64B4` | `0x192D0` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE6 | 1 |  |
| `0x64B5` | `0x192D4` | MWV207REG_DEC400_CR_DEBUG_TILE256_TYPE7 | 1 |  |
| `0x64B6` | `0x192D8` | MWV207REG_DEC400_DEBUG_READ_GATE_DOMAIN_CLK_COUNTER | 1 |  |
| `0x64B7` | `0x192DC` | MWV207REG_DEC400_DEBUG_WRITE_GATE_DOMAIN_CLK_COUNTER | 1 |  |
| `0x64B8` | `0x192E0` | MWV207REG_DEC400_DEBUG_OTHER_GATE_DOMAIN_CLK_COUNTER | 1 |  |
| `0x64C0` | `0x19300` | MWV207REG_DEC400_FAST_CLEAR_VALUE | 1 |  |
| `0x64E0` | `0x19380` | MWV207REG_DEC400_FAST_CLEAR_VALUE_EX | 1 |  |
| `0x6500` | `0x19400` | MWV207REG_DEC400_IMAGE_SIZE | 2 |  |

### `TFB` — 11 个（驱动使用 0 个）

| reg_id | 地址 | 寄存器 | 字段 | 驱动 |
|---|---:|---|---:|---|
| `0x7000` | `0x1C000` | MWV207REG_TFB_CONTROL | 2 |  |
| `0x7001` | `0x1C004` | MWV207REG_TFB_COMMAND | 1 |  |
| `0x7002` | `0x1C008` | MWV207REG_TFB_HEADER | 1 |  |
| `0x7003` | `0x1C00C` | MWV207REG_TFB_INVALIDATE | 1 |  |
| `0x7004` | `0x1C010` | MWV207REG_TFB_QUERY_COMMAND | 3 |  |
| `0x7005` | `0x1C014` | MWV207REG_TFB_QUERY_HEADER | 1 |  |
| `0x7010` | `0x1C040` | MWV207REG_TFB_BUFFER_ADDRESS | 1 |  |
| `0x7020` | `0x1C080` | MWV207REG_TFB_BUFFER_SIZE | 1 |  |
| `0x7030` | `0x1C0C0` | MWV207REG_TFB_BUFFER_STRIDE | 1 |  |
| `0x7040` | `0x1C100` | MWV207REG_TFB_DESCRIPTOR_COUNT | 1 |  |
| `0x7200` | `0x1C800` | MWV207REG_TFB_DESCRIPTOR | 5 |  |

