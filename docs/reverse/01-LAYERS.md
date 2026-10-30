# 01 · 闭源驱动分层结构与符号可用性

> 反编译对象：景美 JM9100/MWV207 官方 1.8.0 闭源包（`/tmp/v180`，由
> `src/JM9_1.8.0-driver-uos25-desktop-release-aarch64/` 用 `dpkg-deb -x` 解出）。
> 方法：`nm` / `readelf` / `objdump`（本机无 r2/ghidra/radare2）。
> 每条结论都可重跑 `reverse/tools/` 下的脚本复现。

## 1. 关键前提：闭源 .so 保留了完整静态符号表

此前假设"闭源库已剥离符号"，实测**不成立**。全部 13 个二进制都带 `.symtab`
（函数级符号名），但**没有** `.debug_*` 节（`readelf -S | grep -c debug` = 0），
即：有函数名、无类型信息与行号。

这决定了反编译能做什么、不能做什么：

| 能得到 | 得不到 |
|---|---|
| 函数名、调用关系、函数边界 | 结构体定义、局部变量名 |
| 立即数常量（寄存器地址、掩码、枚举值） | 源码行号、变量名 |
| 调用图（谁调用谁） | 控制流的**语义**（需人工判读） |

## 2. 二进制清单与符号统计

| 文件 | 职责 | 大小 | 动态符号 | 静态符号 |
|---|---|---|---|---|
| `jmgpu_dri.so` | GL / DRI 入口层 | 4,177,136 | 1661 | **8180** |
| `libGLX_mwv207.so.0` | GLX（X11 绑定） | 854,752 | 1334 | **3056** |
| `libEGL_mwv207.so.0` | EGL | 433,760 | 406 | **1074** |
| `libGLESv2_mwv207.so` | GLES 2.0/3.x | 1,798,208 | 823 | **2893** |
| `libGLESv1_CM_mwv207.so` | GLES 1.1 | 451,104 | 509 | **994** |
| `libJMC.so` | 中间层（JMC） | 17,630,432 | 690 | **9332** |
| `libGLSLC.so` | GLSL 编译器前端 | 1,350,920 | 163 | **1600** |
| `libCLC.so` | OpenCL 编译器 | 2,604,672 | 137 | **1941** |
| `libSPIRV_jm.so` | SPIR-V → JMIR | 3,014,056 | 132 | **349** |
| `libdrm_jmgpu.so` | **HAL / 寄存器写入层** | 1,518,576 | 1671 | **2538** |
| `mwv207_drv.so` | Xorg DDX | 1,048,800 | 391 | **635** |
| `libOpenCL.so.1` | OpenCL ICD 装载器 | 86,656 | 161 | **345** |
| `libgbm_jm.so` | GBM | 73,320 | 47 | **90** |

## 3. 分层结论：`libdrm_jmgpu.so` 就是 HAL

判定证据（可复现）：

```
$ nm jmgpu_dri.so | grep -cE "jmo_|jms_"        # 本文件定义
0
$ nm -u jmgpu_dri.so | grep -cE "jmo_|jms_"     # 未定义（需外部提供）
322
$ for f in 全部闭源 .so; do ...; done
  libdrm_jmgpu.so: 1524 个已定义 jmo_/jms_
```

即：`jmgpu_dri.so` 里 322 个 `jmo_`/`jms_` 符号**全部是 UNDEF**，而全集合中
只有 `libdrm_jmgpu.so` 定义了它们（1524 个）。因此：

```
  应用层 / GL 入口        jmgpu_dri.so, libGLESv2_mwv207.so, libGLX_mwv207.so, libEGL_mwv207.so
        │                 （导出 gl* 符号 + __glim_* 派发表 + __glChip* 硬件封装）
        ▼
  中间层                  libJMC.so（9332 符号）
        │
        ▼
  HAL / 寄存器写入层      libdrm_jmgpu.so   ← 1524 个 jmo_/jms_，133 个 jmo_HAL_*
        │                 （jmo_3D_* 141, jmo_2D_* 96, jmo_HARDWARE_* 464, jmo_OS_* 165 …）
        ▼
  内核                    jmgpu.ko（13 个 DRM_IOCTL_JM_*）
```

**反编译的头号目标是 `libdrm_jmgpu.so`** —— 所有寄存器写入都在这一层。

## 4. `libdrm_jmgpu.so` 内部家族（1496 个 `jmo_*` 符号，28 个家族）

| 家族 | 符号数 | 说明 |
|---|---|---|
| `jmo_HARDWARE_*` | 464 | 真正的寄存器写入封装（`LoadState*` / `Flush*` / `Set*`） |
| `jmo_OS_*` | 165 | OS 抽象（线程/信号/内存/TLS） |
| `jmo_3D_*` | 141 | 3D 引擎状态（`SetQuery`/`DrawPrimitives`/`SetBlend*` …） |
| `jmo_SURF_*` | 120 | surface / 显存节点 |
| `jmo_2D_*` | 96 | 2D BLT 引擎 |
| `jmo_CL_*` | 87 | OpenCL / 通用计算 |
| `jmo_HAL_*` | 133 | 硬件能力查询与全局控制 |
| `jmo_TEXTURE_*` | 44 | 纹理对象 |
| `jmo_BUFFER_*` | 33 | 命令缓冲 |
| `jmo_BUFOBJ_*` | 26 | buffer object |
| `jmo_STREAM_*` | 26 | 流（顶点/索引） |
| `jmo_INDEX_*` | 22 | 索引缓冲 |
| `jmo_PROFILER_*` | 22 | 性能计数 |
| `jmo_BRUSH_*` | 15 | 2D 画刷 |
| `jmo_HD_*` | 15 | ? |
| `jmo_MATH_*` | 12 | 数学 |
| `jmo_TPHARDWARE_*` | 12 | TPC 压缩 |
| `jmo_DECHARDWARE_*` | 10 | 解码器 |
| `jmo_VERTEXARRAY_*` | 9 | 顶点数组 |
| `jmo_VERTEX_*` / `jmo_SHADER_*` | 8 / 8 | 顶点 / 着色器 |
| `jmo_HEAP_*` / `jmo_QUEUE_*` / `jmo_CAPBUF_*` | 7 / 5 / 11 | 堆 / 队列 / 捕获缓冲 |
| 其它（`DEC400EXHARDWARE`2、`CLHardware`1、`PLATFORM`1） | 4 | |

## 5. 调用链实证（以遮挡查询为例）

```
glBeginQuery                                    (jmgpu_dri.so 导出符号)
  → __glim_BeginQuery                           (jmgpu_dri.so 派发表)
    → __glChipBeginQuery                        (jmgpu_dri.so:0x222ea4)
      → jmo_3D_SetQuery                         (libdrm_jmgpu.so:0x642d0, 薄封装)
        → jmo_HARDWARE_SetQuery                 (libdrm_jmgpu.so:0xdbc00, size 0x960)
          → jmo_HARDWARE_LoadCtrlStateNEW       (0x94190)  ← 真正写寄存器
          → jmo_HARDWARE_LoadState32NEW         (0xa1630)
      → jmo_HAL_QueryCluster / jmo_HAL_Query3DCoreCount
      → jms_SURF_NODE_GetHardwareAddress
```

`__glChipBeginQuery` 的全部调用目标（19 个）：`jmo_3D_SetQuery`×2、
`jmo_HAL_QueryCluster`、`jmo_HAL_Query3DCoreCount`、`jmo_HAL_TimeQuery_SendFence`、
`jms_SURF_NODE_GetHardwareAddress`×2、`jms_SURF_NODE_Construct`、`jmo_SURF_NODE_Cache`、
`jmo_SURF_LockNode`、`jmo_OS_Allocate`×4、`jmo_OS_CreateSignal`×3、`jmo_OS_CreateThread`、
`jmo_OS_CreateMutex`、`jmo_OS_GetTLS`、`jmo_OS_Free`、`jmo_OS_Signal`、
`jmChipTimeElapsedAddObject`、`jmChipSetError`、`memset`×4。

## 6. 数据边界（重要）

- 本文档只记录**从二进制本身读出的事实**：符号名、地址、立即数常量、调用关系。
- 寄存器**名与位域**来自随包发布的公开 KMD 头（`/tmp/v180/usr/src/mwv207-1.8.0.uos/`，
  146 个 `.h`，与 1.6.1 的 1353 个地址集合**完全相同**）。这些头的注释声明
  "confidential to JingJiaMicro… strictly prohibited"，故**只引用地址/字段名/
  位域这类事实，不复制其实现代码或注释文本**。
- 凡无法从上述两类来源确证的内容，一律标注 **未确认**，不做猜测性断言。

