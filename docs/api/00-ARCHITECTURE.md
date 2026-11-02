# MWV207 开源用户态驱动（UMD）架构

> 适用源码：`src/mwv207-umd`（适配层）。本文档描述**当前仓库里的实际实现**，所有结论都可在源码中核对；行号引用格式为 `路径:行`。
> 上游参考：`ref/oh-mesa/User_source_code/mesa-24.1.2`（Mesa 24.1.2 树，驱动以符号链接形式挂进 `src/gallium/drivers/mwv207` 与 `src/gallium/winsys/mwv207`）。
> 硬件：景美 JM9100 / MWV207（PCI `0731:9100`）。

---

## 1. 概况

| 项 | 值 |
| --- | --- |
| 适配驱动文件数 | **115**（`drivers/mwv207` 107 + `winsys/mwv207` 8） |
| 适配驱动行数 | **50,204** |
| 其中 `video-codec/` | **26,099**（占一半以上） |
| 其中 `nir/`（编译器） | **10,399** |
| 其中顶层 `.c`（状态/绘制/资源） | **10,300** |
| 上游 Mesa 版本 | 24.1.2（`git` 版本戳随构建变化，如 `24.1.2 (git-9466c83cfc)`） |
| 内核侧 | `mwv207.ko`（DRM/KMS + GPU 调度），6 个 legacy ioctl：`DRM_IOCTL_MWV207_{INFO,CTX,GEM_CREATE,GEM_MMAP,GEM_WAIT,SUBMIT}` |
| 已实现前端 | OpenGL 4.0 / OpenGL ES 3.2、VA-API（H.264/HEVC 硬解）、VDPAU、OpenCL（rusticl）、GBM |
| 构建 | 作为 Mesa 的一棵树参与 meson 构建（`build-full`、`build-glvnd`） |

驱动**只使用 legacy ioctl**，与 1.8.0 闭源包新增的 13 个 `DRM_IOCTL_JM_*` 无关——这也是开源栈能与自建/发行版 KMD 直接对接的原因。

---

## 2. 分层与调用链

```
 应用 (glmark2 / Firefox / mpv / OpenCL …)
   │  GL / GLES / EGL / VA-API / VDPAU / CL
   ▼
 Mesa core  src/mesa（状态跟踪 st_*、GL 入口、扩展判定 st_extensions.c、版本门控 version.c）
   │  pipe_context / pipe_screen 接口
   ▼
 mwv207 UMD  src/mwv207-umd/drivers/mwv207
   ├─ 状态层  mwv207_state.c / blend.c / sampler.c / vertex.c / query.c / fence.c
   ├─ 绘制层  mwv207_draw.c / blit.c / surface.c / resource.c / format.c
   ├─ 编译层  nir/mwv207_compiler.c → lower*.c → ra.c → emit.c → disasm.c
   └─ 视频层  mwv207_video*.c + video-codec/（dec / ppu / 寄存器表）
   │  winsys 接口（bo / 命令缓冲）
   ▼
 mwv207 winsys  src/mwv207-umd/winsys/mwv207/drm
   ├─ mwv207_drm_winsys.c  bo 创建、ring 记账、调试开关
   ├─ mwv207_drm_bo.c      mmap/映射缓存、等待围栏、导入导出同步
   └─ mwv207_drm_cs.c      命令缓冲提交、BO/reloc 列表、提交前排空
   │  ioctl（INFO/CTX/GEM_CREATE/GEM_MMAP/GEM_WAIT/SUBMIT）
   ▼
 KMD  mwv207.ko（DRM/KMS、GPU 调度 drm_sched、TTM 显存、中断、devfreq）
   ▼
 硬件 MWV207 3D / 2D BLT / 视频解码块 / 显示控制器
```

要点：**状态在 `create_*_state()` 时打包成驱动的 C 结构体，真正的寄存器命令在绘制/清屏时发射**；UMD 不直接碰 DRM 之外的任何 ABI。

---

## 3. 文件清单

（自动生成：`include/hw/*` 三个寄存器头是**空壳**，必须结合厂商寄存器库阅读，见 `docs/registers/`。）

| 文件 | 行数 | 职责 |
| --- | --- | --- |
| `drivers/mwv207/include/hw/mwv207reg_3d.h` | 30 | 寄存器头——**空壳**（只有 include guard），寄存器号在驱动里以裸十六进制书写 |
| `drivers/mwv207/include/hw/mwv207reg_common.h` | 33 | 寄存器头——**空壳**（只有 include guard），寄存器号在驱动里以裸十六进制书写 |
| `drivers/mwv207/include/hw/mwv207reg_shader.h` | 30 | 寄存器头——**空壳**（只有 include guard），寄存器号在驱动里以裸十六进制书写 |
| `drivers/mwv207/include/hw/mwv207reg_texture.h` | 30 | 寄存器头——**空壳**（只有 include guard），寄存器号在驱动里以裸十六进制书写 |
| `drivers/mwv207/include/mwv207_blit.h` | 98 | 驱动公共头 |
| `drivers/mwv207/include/mwv207_cmd.h` | 223 | 命令字编码宏（MWV207_EMIT/EMITF/EMITS/EMITFS/STALL/SNAP_TO_PAGE、BLIT_LOCK）——全驱动唯一的命令发射接口 |
| `drivers/mwv207/include/mwv207_context.h` | 198 | 驱动公共头 |
| `drivers/mwv207/include/mwv207_format.h` | 182 | 驱动公共头 |
| `drivers/mwv207/include/mwv207_query.h` | 60 | 驱动公共头 |
| `drivers/mwv207/include/mwv207_resource.h` | 153 | 驱动公共头 |
| `drivers/mwv207/include/mwv207_screen.h` | 56 | 驱动公共头 |
| `drivers/mwv207/include/mwv207_shader.h` | 105 | 驱动公共头 |
| `drivers/mwv207/include/mwv207_state.h` | 145 | 驱动公共头 |
| `drivers/mwv207/include/mwv207_surface.h` | 125 | 驱动公共头 |
| `drivers/mwv207/mwv207_blend.c` | 369 | 混合状态：alpha_config/blend_config(ex)/color_mask 与 advanced 方程映射 |
| `drivers/mwv207/mwv207_blit.c` | 1249 | blitInfos[] 格式表与 2D BLT 路径（表面复制、解析、CPU 回退） |
| `drivers/mwv207/mwv207_cmd.c` | 372 | 命令缓冲辅助命令：blit 锁、tile status 开关、flush/同步等零散寄存器写入 |
| `drivers/mwv207/mwv207_compute.c` | 319 | compute（CS）路径：工作组、共享内存、SSBO/图像绑定 |
| `drivers/mwv207/mwv207_context.c` | 232 | pipe_context 分配、状态对象绑定、flush/destroy、compute 入口挂接 |
| `drivers/mwv207/mwv207_draw.c` | 1302 | draw_vbo/clear/blit 主路径：发射 3D 命令、绑定目标与深度、调用顶点/着色器装配 |
| `drivers/mwv207/mwv207_format.c` | 963 | texture_infos[]/pixel_infos[] 格式表：PIPE_FORMAT→硬件编码、tiling 判定 |
| `drivers/mwv207/mwv207_query.c` | 415 | 查询（遮挡/时间戳/流水线统计）寄存器编程与结果读回 |
| `drivers/mwv207/mwv207_resource.c` | 661 | 资源（纹理/缓冲）创建与布局、tiling、stride、dmabuf 导入导出 |
| `drivers/mwv207/mwv207_sampler.c` | 1083 | 采样器状态与采样器视图：texture_type[]、tex_fetch[]、ctrl 位域、矩形/数组语义 |
| `drivers/mwv207/mwv207_screen.c` | 708 | pipe_screen：能力表 mwv207_get_param()、资源/采样器视图/shader 变体创建、内存预算 |
| `drivers/mwv207/mwv207_shader.c` | 1198 | shader 状态与变体管理（VS/FS/CS 编译入口、pshader→NIR、变体缓存） |
| `drivers/mwv207/mwv207_state.c` | 499 | 混合/深度模板/光栅化/裁剪/视口/采样器状态的 pipe→驱动结构转换 |
| `drivers/mwv207/mwv207_surface.c` | 363 | surface 描述（framebuffer 附件 → 硬件目标描述） |
| `drivers/mwv207/mwv207_vertex.c` | 567 | 顶点元素/属性与 uniform/立即数上传（get_texture_size、get_texrect_scale、unifrom[]） |
| `drivers/mwv207/nir/mwv207_cmdline.c` | 170 | NIR 调试命令行/环境开关处理 |
| `drivers/mwv207/nir/mwv207_common.c` | 326 | NIR 侧公共工具函数 |
| `drivers/mwv207/nir/mwv207_common.h` | 298 | 驱动内部枚举与共享定义（mwv207_immediate_contents 等） |
| `drivers/mwv207/nir/mwv207_compiler.c` | 666 | NIR 编译流水线入口：lowering 调度、特供 uniform 解析（TEXSCALE/TEXSIZE）、变体回写 |
| `drivers/mwv207/nir/mwv207_compiler.h` | 194 | 编译流水线对外声明（mwv207_compile_shader 等） |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1317 | ISA 反汇编与调试输出 |
| `drivers/mwv207/nir/mwv207_disasm.h` | 328 | 反汇编接口声明 |
| `drivers/mwv207/nir/mwv207_emit.c` | 2029 | ISA 发射：算术语义、控制流（if/loop/switch）、纹理取样、SSBO/图像指令 |
| `drivers/mwv207/nir/mwv207_emit.h` | 115 | 发射层公共结构体与宏（CONST_VAL/TEXSCALE/TEXSIZE、SRC_CONST、rgroup 判定） |
| `drivers/mwv207/nir/mwv207_liveness.c` | 257 | 活跃区间分析（供寄存器分配使用） |
| `drivers/mwv207/nir/mwv207_lower.c` | 1695 | NIR lowering 主集（矩形纹理坐标、纹理查询、间接 uniform 等） |
| `drivers/mwv207/nir/mwv207_lower.h` | 49 | 各 lowering pass 的声明集 |
| `drivers/mwv207/nir/mwv207_lower_64bit.c` | 786 | 64 位整数/浮点运算的 NIR 降级 |
| `drivers/mwv207/nir/mwv207_lower_dual.c` | 269 | 双源/对偶发射相关降级 |
| `drivers/mwv207/nir/mwv207_lower_shared_atomic.c` | 194 | 共享内存与原子操作的 NIR 降级 |
| `drivers/mwv207/nir/mwv207_lower_tg4.c` | 111 | textureGather(tg4) 的 NIR 降级 |
| `drivers/mwv207/nir/mwv207_nir_opt.h` | 39 | NIR 优化 pass 声明 |
| `drivers/mwv207/nir/mwv207_nir_opt_peephole_fdiv.c` | 127 | 窥孔优化：除法（fdiv） |
| `drivers/mwv207/nir/mwv207_nir_opt_peephole_ffma.c` | 234 | 窥孔优化：FFMA 融合 |
| `drivers/mwv207/nir/mwv207_nir_opt_peephole_set.c` | 188 | 窥孔优化：比较/set 指令 |
| `drivers/mwv207/nir/mwv207_opcode.c` | 154 | 操作码定义与映射表（含厂商扩展操作码） |
| `drivers/mwv207/nir/mwv207_ra.c` | 856 | 寄存器分配与调度（含立即数常量识别） |
| `drivers/mwv207/video-codec/common/mwv207_codec_time.c` | 68 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_codec_time.h` | 63 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 460 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_decoder_h264_nal_unit.c` | 149 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_decoder_h264_nal_unit.h` | 96 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_decoder_h264_slice_header.c` | 887 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_decoder_h264_slice_header.h` | 197 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_exp_golomb.c` | 181 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_exp_golomb.h` | 58 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_slice_header.c` | 457 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_slice_header.h` | 107 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_stream.c` | 338 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_stream.h` | 54 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.c` | 102 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 121 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_decoder_vp9_slice_header.c` | 1532 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_decoder_vp9_slice_header.h` | 768 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_log.c` | 58 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_log.h` | 62 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_utils.h` | 39 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_video_cs.c` | 136 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_video_cs.h` | 51 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_video_enums.h` | 102 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_video_info.h` | 277 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_video_reg_buf.c` | 131 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_video_reg_buf.h` | 86 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 765 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_vp9hwd_bool.c` | 364 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/common/mwv207_vp9hwd_bool.h` | 75 | 通用解码框架（切片头/参数集解析、参考帧与 DPB 管理） |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 569 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_h264.c` | 2378 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_h264.h` | 39 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_hevc.c` | 1332 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_hevc.h` | 34 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.c` | 2088 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.h` | 323 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_register_array_7020.h` | 889 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec.c` | 242 | H.264/HEVC/VP9 解码实现（寄存器编程、切片解析、参考帧管理） |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec.h` | 165 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_8170enum.h` | 830 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_8170g1table.h` | 1344 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_8170g2table.h` | 1344 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_8170table.h` | 1871 | 解码寄存器表（8170 系列） |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_cfg.h` | 321 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_cs.h` | 65 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_reg.c` | 175 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_reg.h` | 135 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_vpu_features_list.h` | 2148 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_vpufeature.c` | 62 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_vpufeature.h` | 191 | MWV207 解码实现/寄存器表（硬件专属） |
| `drivers/mwv207/video-codec/mwv207_video_buffer.c` | 611 | 视频表面/缓冲的分配与包装 |
| `drivers/mwv207/video-codec/mwv207_video_buffer.h` | 219 | 视频缓冲接口声明 |
| `drivers/mwv207/video-codec/mwv207_video_codec.c` | 154 | 编解码器注册与公共入口 |
| `drivers/mwv207/video-codec/mwv207_video_codec.h` | 43 | 编解码器公共声明 |
| `drivers/mwv207/video-codec/mwv207_video_decoder.c` | 650 | 解码器基类：上下文、参数集、参考帧管理接口 |
| `drivers/mwv207/video-codec/mwv207_video_decoder.h` | 93 | 解码器基类声明 |
| `winsys/mwv207/drm/mwv207_drm.h` | 215 | winsys 内部结构定义（bo / 命令缓冲 / 屏幕状态） |
| `winsys/mwv207/drm/mwv207_drm_bo.c` | 582 | winsys bo 实现：mmap/映射缓存、等待围栏、导入/导出同步、EVICT 处理 |
| `winsys/mwv207/drm/mwv207_drm_bo.h` | 33 | bo 结构与操作声明 |
| `winsys/mwv207/drm/mwv207_drm_cs.c` | 470 | 命令缓冲：提交、command-list/BO 列表、提交前排空与 reloc 组装 |
| `winsys/mwv207/drm/mwv207_drm_cs.h` | 37 | 命令缓冲结构与操作声明 |
| `winsys/mwv207/drm/mwv207_drm_public.h` | 37 | winsys 对外声明（屏幕创建入口） |
| `winsys/mwv207/drm/mwv207_drm_winsys.c` | 276 | DRM winsys：屏幕/bo 创建、ring（buffet）记账与回收、调试开关解析 |
| `winsys/mwv207/drm/mwv207_drm_winsys.h` | 285 | winsys 屏幕方法声明 |

---

## 4. 模块职责（按功能分组）

- **屏幕与能力**：`mwv207_screen.c` 是能力表的唯一出处（`mwv207_get_param()`）。每加一个 `PIPE_CAP_*` 都要注意 gallium 的默认值来自 `src/gallium/auxiliary/util/u_screen.c`——**默认非 0 而驱动没实现的功能会变成超额声明**（历史实例：`PIPE_CAP_BUFFER_MAP_PERSISTENT_COHERENT` 默认 1 → 广告出 `GL_ARB_buffer_storage`，而驱动没实现持久映射）。
- **上下文与状态**：`mwv207_context.c` 负责 `pipe_context` 的分配与所有 `create_*_state` 绑定；状态结构体定义在 `include/mwv207_state.h`。混合状态里的 advanced-blend 映射（`mwv207_blend.c` 的 `alpha_mode_ext[]`/`adv_ext[]`）只覆盖硬件真正实现的方程子集。
- **绘制**：`mwv207_draw.c` 是 3D 主路径（`mwv207_draw_vbo()`、两处 clear、条件渲染早退），它把当前状态翻译成一串 `MWV207_EMIT` 命令；`mwv207_blit.c` 负责表面复制/解析，含 CPU 回退。
- **资源与格式**：`mwv207_format.c` 里有**三张互不相同编码空间**的表（`texture_infos[]` TX、`blitInfos[]` BLT、`pixel_infos[]`），新增格式必须三处都改；`mwv207_resource.c` 决定 tiling/stride，必要时可强制 `LINEAR` 规避 BLT 破坏数据。
- **采样**：`mwv207_sampler.c` 既管采样器状态（`sampler_ctrl0/1` 位域），也管采样器视图（`tex_desc->tex_fetch[]`）。矩形纹理（`PIPE_TEXTURE_RECT`）**没有独立分支**——`mwv207_texture_type[]` 把 RECT 当作 2D，坐标语义靠"硬件未归一化模式"（`sampler->state.unnormalized_coords`）+ 坐标原样透传实现。
- **顶点与 uniform**：`mwv207_vertex.c` 收集顶点元素并上传 uniform；**驱动自用常量**（`MWV207_IMMEDIATE_*` 枚举）通过"扫描着色器立即数 → `imm_contents[]` → 每 4 个一组写入 `unifrom[]`"这条链路上传，`get_texture_size()`/`get_texrect_scale()` 是取值函数。
- **NIR 编译**：`nir/mwv207_compiler.c` 是流水线入口（`mwv207_compile_shader()`），依次调用 `mwv207_lower*` 各降级 pass、`mwv207_ra.c` 寄存器分配、`mwv207_emit.c` ISA 发射；`mwv207_disasm.c` 提供反汇编（配合 `mwv207_debug` 使用）。
- **视频**：`mwv207_video*.c` 是前端，`video-codec/` 是解码实现与**大量寄存器表**（H.264/HEVC 完整，VP9 部分）。解码寄存器在该目录里有厂商命名（与 3D 部分不同）。
- **winsys**：`mwv207_drm_cs.c` 的提交路径决定"什么时候排空"，`mwv207_drm_bo.c` 的映射/等待决定"CPU 视图何时能看到 GPU 写入"——这两处是导入/导出（GBM、EGL dmabuf）正确性的关键。

---

## 5. 四条关键数据流

1. **状态设置流**：`st_context` 状态变化 → `pipe_context::create_*_state()` → 驱动结构体（`include/mwv207_state.h`）→ 绘制时被 `mwv207_draw.c` 翻译成寄存器写入。
2. **绘制流**：`st_draw_gallium*()` → `cso_draw_vbo()` → `mwv207_draw_vbo()` → 顶点/uniform 装配（`mwv207_vertex.c`）→ 命令发射（`mwv207_draw.c` + `nir/mwv207_emit.c` 生成的着色器）→ `mwv207_drm_cs.c` 提交 → KMD `SUBMIT`。
3. **着色器流**：GLSL/SPIR-V → Mesa NIR → `mwv207_compile_shader()`（clone → `mwv207_lower()` → `mwv207_ra()` → `mwv207_emit()`）→ 指令内存 + 常量内存；变体由 `mwv207_shader.c` 管理。
4. **视频流**：VA-API/VDPAU/FFmpeg → `pipe_video_codec`（`st_vdpau`/`vl`）→ `mwv207_video_codec.c` → `mwv207_video_decoder.c` → `mwv207_dec_{h264,hevc}.c` 的 `set_regs` 寄存器序列 → 提交 → DEC 硬件 → `mwv207_dec_ppu.c` 后处理 → 输出 surface（可经 dmabuf 导出）。细节见 `docs/api/05-VIDEO-CODEC.md`。

**纹理上传（`glTexImage2D`）路径与已知坑**：

```
glTexImage2D → st_TexImage/st_TexSubImage
  → pipe_context::texture_subdata = u_default_texture_subdata
  → pipe_context::texture_map   = mwv207_transfer_map()
        非 LINEAR 资源：先建 staging 资源（PIPE_USAGE_STAGING|PIPE_BIND_LINEAR）→ winsys bo_map → CPU 可写指针
  → memcpy（CPU，字节精确）
  → pipe_context::texture_unmap = mwv207_transfer_unmap()
        用 2D BLT（mwv207_blit_resource_box）把 staging 拷进真实资源
```

⚠ **坑**：最后那一步 BLT 对 16 位多通道布局**不能保持字节精确**，这些格式必须走 `LINEAR`（见 `mwv207_tile_supported_format()` 与 `patches/0010-mwv207-16bit-formats-linear-layout.patch`）。

---

## 6. 命令流与寄存器机制

命令字由 `include/mwv207_cmd.h` 的宏生成：

| 宏 | 高 16 位 | 说明 |
| --- | --- | --- |
| `MWV207_EMIT(cmd, reg, value)` | `0x0801` | 写一个寄存器：`0x08010000 + reg`，随后跟一个 32 位值 |
| `MWV207_EMITF` | `0x0C01` | 同上但带 fence/刷新语义 |
| `MWV207_EMITS(cmd, reg, count, value)` | `0x0800 + (count<<16)` | 连续写 `count` 个字；`count` 为偶数时补 `0xdeadbeef` |
| `MWV207_EMITFS` | `0x0C00 + (count<<16)` | 连续写的带 fence 版本 |
| `MWV207_STALL(cmd, value)` | `0x9<<27` | 等待/同步 |
| `MWV207_SNAP_TO_PAGE(cmd)` | — | 发 `0x9800001f, 0x00000000` |

字段布局：**bits[31:27] = opcode，bits[26:16] = count，bits[15:0] = 寄存器号**。

寄存器号的约定：**驱动里的 `reg` = 厂商头文件中 `MWV207REG_<X>_Address >> 2`**（已用 5 组语义精确配对 + 全库统计 170/219 ≈ 78% 验证；统计工具 `docs/tools/map_regs.py`）。厂商寄存器库（1.6.1 与 1.8.0 同一集合）共 1392 个带 `_Address` 的寄存器、3954 字段、6514 命名值，明细见 `docs/registers/`。

⚠ 驱动自带的 `include/hw/mwv207reg_*.h` 是**空壳**，所以源码里到处是裸十六进制常量；阅读时请对照 `docs/registers/REGISTERS-FIELDS.md`。

---

## 7. 构建、安装与加载

**目录约定**：驱动源码在 `src/mwv207-umd/`，上游 Mesa 树在 `ref/oh-mesa/User_source_code/mesa-24.1.2/`（只读参考 + 构建宿主），后者的 `src/gallium/{drivers,winsys}/mwv207` 是指向 `src/mwv207-umd` 的符号链接。

```bash
# 1) 配置（首次；这一套选项是"能装进 switch-mwv207.sh 布局"的唯一组合）
meson setup ref/oh-mesa/User_source_code/mesa-24.1.2/build-full \
  ref/oh-mesa/User_source_code/mesa-24.1.2 \
  -Dgallium-drivers=mwv207 -Dvulkan-drivers= \
  -Dplatforms=x11,wayland -Dglx=dri -Ddri3=enabled -Dgbm=enabled -Degl=enabled \
  -Dglvnd=disabled -Dgles1=enabled -Dgles2=enabled -Dshared-glapi=enabled \
  -Dtools=[] -Dvideo-codecs=all -Dshader-cache=enabled \
  -Dgallium-va=enabled -Dgallium-vdpau=enabled -Dgallium-rusticl=true \
  -Dgallium-xa=disabled -Dgallium-opencl=disabled -Dgallium-omx=disabled \
  -Dllvm=enabled -Ddraw-use-llvm=true -Dshared-llvm=enabled \
  -Dzstd=disabled -Dvalgrind=disabled -Dlibunwind=disabled -Dlmsensors=disabled \
  --buildtype=debugoptimized

# glvnd 版（产出 EGL ICD libEGL_jm.so.0.0.0，由 /usr/share/glvnd/egl_vendor.d/10_mwv207.json 引用）
meson setup ref/oh-mesa/User_source_code/mesa-24.1.2/build-glvnd \
  ref/oh-mesa/User_source_code/mesa-24.1.2 -Dgallium-drivers=mwv207 \
  -Dglvnd=enabled -Dglvnd-vendor-name=jm -Dgles1=enabled -Dgles2=enabled ...

# 2) 构建（先保证符号链接在，再编）
./tools/build-umd.sh build

# 3) 免安装试跑：装配"模拟安装目录"，所有库戳自洽
./tools/build-umd.sh sim          # → /tmp/mwv207-sim
DISPLAY=:0 LD_LIBRARY_PATH=/tmp/mwv207-sim LIBGL_DRIVERS_PATH=/tmp/mwv207-sim \
  MESA_LOADER_DRIVER_OVERRIDE=mwv207 glxinfo -B

# 4) 出载荷并正式安装
./tools/make-payload.sh -v 24.1.2-fullstack-rN   # → payload-N/
./switch-mwv207.sh umd check                     # 只读预检
sudo ./switch-mwv207.sh umd install              # 会重启 lightdm
```

**两个必须显式指定的选项**（否则产物名/行为随环境漂移，装不进脚本布局）：

- `-Dglvnd=disabled`（`build-full`）：留给 `auto` 时重配一次就可能变成 glvnd 模式，产物从 `libGL_mwv207.so.1.2.0`/`libGLESv2_mwv207.so.2.0.0` 变成 `libGLX_mwv207.so.0` 且不再生成 GLES，`switch-mwv207.sh` 期待的实体文件名全部对不上（踩过一次：`meson setup --wipe` 后整个 GL 栈装不上）。
- `-Dgles1=enabled -Dgles2=enabled`：默认 `auto` 在 glvnd 模式下不产 GLES。

**版本戳陷阱**：Mesa 的 `24.1.2 (git-<sha>)` 取自**上游树构建时的 git HEAD**。加载器逐字比对 DRI 驱动与 `libGL`/`libEGL`/ICD 的戳，不一致就 `DRI driver not from this Mesa build` → 回退软件渲染 → `X Error BadValue (X_GLXCreateNewContext)`。因此：只改 `src/mwv207-umd/` 并重建 DRI 时戳不变（仍是上游树 HEAD），可直接用；一旦上游树有新提交，重建后戳就变了 → 必须 `./tools/build-umd.sh sim` 或整体出载荷，让所有库用同一个戳。

产物路径：`build-full/src/gallium/targets/dri/libmwv207_dri.so`，并有一份 `mwv207_dri.so` 符号链接（DRI 加载器按 `<driver>_dri.so` 查找）。

**部署布局**：`/usr/local/lib/mwv207-oss/`（经 `/etc/ld.so.conf.d/00-mwv207-oss.conf` 抢占）+ `/usr/local/lib/aarch64-linux-gnu/dri/mwv207_dri.so`；glvnd 侧把 `/usr/share/glvnd/egl_vendor.d/10_mwv207.json` **原地改写**（`library_path` → 我们的 `libEGL_jm.so.0`，原件备份为 `.vendor-orig`）。

> ⚠ **不要把 `10_mwv207.json` 改名或删除**：`/etc/cdi/jmgpu.yaml`（厂商 1.8.0 的 CDI 规格，38 个挂载源）把它当作 `rbind` 的 `hostPath`。源文件不存在时 `ll-box` 建容器直接失败（`child process error: No such file or directory` → `container process exited before reaching expected stage exec_ready`），**所有玲珑/linyaps 应用（微信、日历、看图…）都打不开**。这正是 r6 时期"微信/日历/看图打不开"的根因。同理不要再另建 `20_jm.json`：glvnd 会读目录下所有 `*.json`，两条指向同一 ICD 会重复枚举 vendor/device。

## 8. 版本与 API 边界

- 上游 25.x 移除/改写了大量接口（`pipe_shader_type`、`nir_def::parent_instr`、`pipe_surface`、`pipe_caps` 等），本实现锚定 **24.1.2**；升级需先做大范围 API 适配。
- 与 Mesa 24.3（发行版）**不能混跑**：版本戳与内部 ABI 都不同，混用会出现 `DRI driver not from this Mesa build` 或渲染错乱。
- 与厂商闭源栈**不共享**任何内部 ABI：闭源的 HAL 在 `libdrm_jmgpu.so`（`jmo_*`），KMD 侧用 13 个 `DRM_IOCTL_JM_*`；反编译结论见 `ref/reverse-source/docs/`。

向上游移植时的已知断点（基线 24.1.2）：

| 版本 | 变化 | 影响 |
| --- | --- | --- |
| 25.3.0 | 删除 `enum pipe_shader_type` 与 `PIPE_SHADER_*`（改用 `mesa_shader_stage`） | 驱动内 289 处引用需改写 |
| 25.3.0 | 删除 VDPAU / OMX 前端 | 丢失 VDPAU 输出通路 |
| 26.0.0 | `PIPE_CAP_*` 枚举改为 `struct pipe_caps` | `get_param()` 129 处 + `get_shader_param()` 28 处需重写 |
| 26.0.0 | 删除 `nir_def::parent_instr`（改 accessor） | 63 处引用需改写 |
| 26.0.0 | 删除 `pipe_context::create_surface` / `surface_destroy` | 驱动表面层需重写 |
| 26.0.0 | 删除 `vl/vl_decoder.h` | 视频层需自带 `vl_level_supported` 实现 |

---

## 9. 已知限制与当前状态

（详细清单与复现见 `docs/UNRESOLVED.md`、`docs/api/11-FIX-FEASIBILITY.md`。）

- 高级混合只实现 **8/15** 个方程（MULTIPLY/SCREEN/OVERLAY/DARKEN/LIGHTEN/HARDLIGHT/DIFFERENCE/EXCLUSION），其余 7 个属超额声明（`GL_KHR_blend_equation_advanced` 需要它才能拿到 GLES 3.2）。
- **R16/RG16 作颜色附件时 FBO 不完整**（`0x8cdd`），且 16 位归一化格式的采样路径历史上存在恒 0 问题。
- 遮挡查询的实现已修（按槽内前 `cluster_count` 个 u32 求和），条件渲染已实现（`mwv207_render_condition()` + `check_render_cond()`）。
- 矩形纹理（`sampler2DRect`）取样曾恒读纹素 (0,0)，根因是**对未归一化坐标额外做了一次 `1/size` 缩放**，现已改为坐标原样透传。
- `PIPE_CAP_BUFFER_MAP_PERSISTENT_COHERENT` 已显式返回 0（不再超额声明 `GL_ARB_buffer_storage`）。
- 视频：H.264/HEVC 解码可用，VP9 缺 `set_regs` 路径，编码未实现；4:2:2/4:4:4 未完成。
- 导入/导出（GBM、EGLImage）需要驱动侧正确排空与映射同步；相关修复在 winsys。

---

## 10. 建议阅读顺序

1. `include/mwv207_cmd.h`（命令字怎么发）→ 2. `mwv207_screen.c`（能力面）→ 3. `winsys/mwv207/drm/mwv207_drm_{winsys,bo,cs}.c`（对象与提交）→ 4. `mwv207_context.c` + `mwv207_state.c`（状态）→ 5. `mwv207_draw.c`（主路径）→ 6. `mwv207_format.c` / `mwv207_resource.c`（资源与格式）→ 7. `mwv207_sampler.c` / `mwv207_vertex.c`（采样与顶点）→ 8. `nir/` 全目录（编译器）→ 9. `video-codec/`（视频，可用时再读）。

配套材料：`docs/api/01-WINSYS-KMD.md`、`02-CORE-OBJECTS.md`、`03-DRAW-PIPELINE.md`、`04-NIR-COMPILER.md`、`05-VIDEO-CODEC.md`、`06-KNOWN-ISSUES.md`、`07-WINSYS-AUDIT.md`、`08-KMD.md`、`09-SYMBOL-INDEX.md`、`10-HARDWARE-BLOCKS.md`、`11-FIX-FEASIBILITY.md`；寄存器：`docs/registers/`；闭源对照：`ref/reverse-source/docs/`。
