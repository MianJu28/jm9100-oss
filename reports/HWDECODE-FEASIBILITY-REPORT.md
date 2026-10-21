# 硬件解码补齐可行性报告（实证版）

- 日期：2026-10-21 凌晨
- 问题：开源驱动（jemoic/mesa）没有视频编解码用户态，"参考闭源驱动补齐硬件解码"是否可行？
- 结论先行：**可行，且不需要逆向闭源驱动——景美已经把解码用户态源码公开在 OpenHarmony 仓库里。本次已实际完成移植并跑通了 H.264 + HEVC 硬件解码。**

---

## 一、决定性发现：解码源码已经公开

`gitee.com/jemoic/jemoic-openharmony-code-source`（OpenHarmony_5.0_release 分支）：

| 内容 | 位置 | 说明 |
|------|------|------|
| **完整视频解码用户态** | `User_source_code/mwv207_mesa.patch` 内 `video-codec/`（56 文件，约 1.06 万行） | H264/HEVC/VP9 解码器、寄存器编程（8170 系列表）、`pipe_video_codec` 标准接口 |
| Mesa 24.1.2 完整源码树 | `User_source_code/mesa-24.1.2/` | 与系统 Mesa 24.3 同世代（ABI 兼容，此前 22.3 与 24.3 混装崩溃的问题就此消解） |
| **Linux 6.6 KMD patch** | `Kernel_source_code/linux_6.6/mwv207_gpu.patch`（18324 行） | 之前分析的 5.10→6.6 移植缺口，官方已有现成答案 |
| 预编译 .so | `so/mwv207_drv_video.so` 等 | 仅供比对，未使用 |

License：video-codec 文件头均为 MIT 风格 permission notice（与 mesa 一致），无 GPL 污染问题（用于发行版打包需再让法务确认一次，但授权文本明确允许 use/copy/modify/sublicense）。

KMD 侧早已确认：`mwv207_pipe_dec.c` 只是寄存器直通（0x01=写寄存器批、0x02=回写），**全部解码知识在用户态**——而这个用户态现在拿到了。

## 二、本次实际完成的工作（可行性 = 已验证的工程事实）

### 移植步骤（全部落盘，见 `patches/0005-*.patch`）

1. 从 `mwv207_mesa.patch` 提取 `video-codec/` 56 文件到 mesa-24.1.2 基线
2. meson 构建接线（video 源文件 + include 路径）；去重 patch 应用时产生的重复注册段
3. `mwv207_context.c` 挂上 `mwv207_context_video_codec_init()`（get_video_param / create_video_codec / create_video_buffer）
4. winsys `mwv207_drm.h` 换用增强版（`MWV207_SUBMIT_ENGINE_TYPE_DEC` 等 ABI 宏；ioctl 号 0x0~0x5 与当前内核完全一致）
5. 版本适配 3 处：`MWV207_DEBUG_VIDEO` 宏、`MWV207_RESOURCE_FLAG_*` 命名、`PIPE_TIMEOUT_INFINITE`→`OS_TIMEOUT_INFINITE`
6. **修复一个真 bug**：`mwv207_video_buffer` 未实现 `get_resources` → `vaGetImage`（帧回读）在 `vlVaGetImage` NULL 调用崩溃；实现 `mwv207_video_buffer_resources()`（对照 `vl_video_buffer.c:479` 标准 buffer 行为）
7. OHOS 残留清理：`dep_hilog`/`dep_omx` 依赖移除（代码本身零 OHOS 依赖）

### 硬件实测结果（本机 JM9100 + in-tree mwv207 KMD，全程 env 非侵入）

```
vainfo（libva 2.22）:
  VAProfileH264ConstrainedBaseline/Main/High/High10 : VAEntrypointVLD
  VAProfileHEVCMain / HEVCMain10                    : VAEntrypointVLD
  VAProfileNone                                     : VAEntrypointVideoProc

ffmpeg -hwaccel vaapi 实测:
  H264 1080p: CPU utime 2.060s(软解) → 0.367s(硬解) = CPU 负载 17.8%
  HEVC  720p: 帧回读像素校验通过（解码 + vaGetImage + PNG 输出全链路）
  帧像素验证: hwdec-frame.png / hevc-frame.png 采样非黑、颜色正确
```

产物：`src/oh-mesa/User_source_code/mesa-24.1.2/build-mwv207va/`
- `targets/dri/libmwv207_dri.so`（GL + 解码同体）
- `targets/va/libmwv207_drv_video.so`（VA 硬解驱动）

## 三、"参考闭源驱动"还需要吗？

**不需要逆向。** 闭源 `mwv207_drv_video.so` 的角色已被公开源码取代。剩余差距仅两点：

1. **编码（VAEntrypointEncSlice）**：本次仅解码（VLD）。KMD 有 pipe_enc，OH patch 里未见编码用户态——若需要硬编，只能向景美索取或后续逆向（工作量大，暂不建议）。
2. **VP9/AV1**：源码含 VP9 切片头解析与 bool 解码器，但 meson 构建项和 `video-codecs` 选项未全开（VIDEO_CODEC_VP9DEC=0）。属增量工作，非障碍。

## 四、落地路径（从"env 验证"到"系统可用"）

| 步骤 | 内容 | 状态 |
|------|------|------|
| 1 | 源码获取 + 移植 + 硬解跑通 | ✅ 本次完成 |
| 2 | libva 2.22 对齐（系统被厂商依赖锁 2.20） | 仓库有 2.22；需打包决策（升级或双栈） |
| 3 | KMD 升级 | 当前 in-tree 6.6 版可用（ioctl ABI 已实测一致）；若需修复报告里的 KMD 缺陷，用官方 `linux_6.6/mwv207_gpu.patch` 作基线 |
| 4 | 系统级切换 | ✅ 已由 `switch-mwv207-oss.sh` 完成（私有目录 + 公开名胶水 + 三态快照/看门狗；glamor 点亮 + 冷启动验证）；`mwv207_drv_video.so` 已装入系统 dri 目录、vainfo 零 env 自动命中（无需 LIBVA_DRIVER_NAME） |
| 5 | GStreamer/浏览器 | gstreamer1.0-vaapi 需按 libva 2.22 重编；Firefox/Chromium 走 VA-API 即可（上一报告已验证机制） |

## 五、风险与注意

- **上游发布形态混乱**：OH 仓库是"源码树 + 补丁"双形态，直接 `git apply` 会静默丢内容（new-file hunk 撞已有文件时被跳过）——本次已用脚本化提取规避，后续同步上游时需同样小心。
- patch 里 winsys/driver 的部分"新文件"实为 22.3 旧版（景美只维护了 video-codec 的 24.1.2 适配），**必须**像本次一样"基线树 + 仅取 video-codec"。
- 编码侧缺失：视频会议/录制类应用仍无硬编。
- 帧回读修复（`get_resources`）说明 OH 仓的主线在 OpenHarmony 上用 OMX/显示直通路径，从未走过 VA `vaGetImage`——Linux VA 路径需要本报告的这类补丁。

## 六、结论

| 问题 | 答案 |
|------|------|
| 参考闭源驱动补齐硬解可行吗？ | **可行，且路线比预期更短：无需逆向，源码已公开** |
| 需要什么？ | 拿 OH 仓的 video-codec + 24.1.2 树做基线 + 本次 0005 补丁 |
| 已验证到什么程度？ | H264(1080p, CPU↓82%) + HEVC 帧级像素校验通过；vainfo 全 profile 枚举 |
| 剩余工作 | libva 2.22 打包对齐、系统级切换（既有脚本）、可选的 VP9 开启、硬编（源缺失） |

---

## 七、10-bit HEVC（Main10）黑屏缺陷的发现与修复（2026-10-21 追加）

### 7.1 现象

自制 1080p ~28Mbps 测试流（`hevc_1080p_main10.mp4`）：
- 8-bit Main：`mpv --hwdec=vaapi` 完全正常（`vaapi[nv12]` 零拷贝）；
- 10-bit Main10：解码在 GPU（`hwdec-current=vaapi`，`vaapi[p010]`）但**窗口全黑**，每帧刷 `blit unsupported r16_unorm -> r8_unorm`（8 秒 466 次）；`--hwdec=vaapi-copy` 报 `Could not copy back hardware decoded frame` 后回退软解。

### 7.2 根因（两个独立缺陷叠加）

**缺陷 A：能力谎报（`mwv207_video_codec.c`）**。`mwv207_is_video_format_supported()` 的双 switch 结构性 bug：第一个 switch 按 format 判定（NV12→true），第二个 switch 按 profile（AVC/HEVC 全系→true）**无条件覆盖**返回值——本应 AND 写成了 OR。结果 P010/P016/YV12/UYVY/BGRA 全被谎报支持 → `config.c` 的 VA_RT_FORMAT 含 `YUV420_10BPP` → mpv/ffmpeg 协商出 P010 surface。

**缺陷 B：输出错位（`mwv207_video_dec.c`）**。`mwv207_dec_pp_params_init()` 在 `bit_depth > 8` 时**无条件** `out_p010 = 1`，不看 outsurface 实际是什么 buffer。而 mwv207 纹理格式表（199 条）**没有 R16_UNORM/R16G16_UNORM**（厂商两代代码都没实现，码值无权威来源、无法盲补）→ P010 双平面 sampler view 建不出 → mpv 的 VA-VPP `p010→nv12` 转换走 `mwv207_blit.c:947` 被拒 → NV12 目标面从未被写 → 黑屏。`resource_create()` 不查格式表（`bo_create` 按字节数分配），所以 P010 surface 建得成、解码也正常——正是"解码 OK 但渲染黑屏"的完整机制链。

### 7.3 修复（commit 6cb20bd，2 文件 +44 −16）

1. **`mwv207_is_video_format_supported()` 改 AND 语义**：`ret = format_switch_ret && profile_switch_ret`，并豁免 `PIPE_VIDEO_PROFILE_UNKNOWN`（VPP/image 路径不受影响）。修复后 `VA_RT_FORMAT` 只剩 `VA_RT_FORMAT_YUV420`，ffmpeg 按标准协商机制自动选 `Picked nv12 as best match for yuv420p10le`。
2. **`mwv207_dec_pp_params_init()` 按 target 分流**：P010/P016 target → 维持 `out_p010=1`（硬件 `fmt_p010_support=1`）；8-bit NV12 target + `bit_depth>8` → `out_cut_8bits=1` 走 **PPU 硬件降位输出**（`PP_OUT_FMT_YUV420_8BIT`，pixel_width=8，NV12 半平面）——解码核心仍按 10bit 输出到 staging，仅输出级降到 8bit。h264.c:1416 与 hevc.c:387 两个调用点都传 target，均受益。

### 7.4 验证（重装 20261021-140819 全量一致构建后）

| 项 | 结果 |
|---|---|
| vainfo HEVCMain10/H264High10 | 仍枚举 VLD（profile 不再广播 10BPP RTFormat） |
| ffmpeg 硬解 10bit 30 帧 | `EXIT=0` 零错误，`Picked nv12 as best match` |
| mpv `--hwdec=vaapi`（10-bit） | `Using hardware decoding (vaapi)` + `1920x1080 vaapi[nv12]`——**P010 黑屏链路彻底消失**：无 `blit unsupported`、无 `scale_vaapi (p010->nv12)` 滤镜、无软解回退 |
| **像素级校验**（10 帧回读 vs 纯软解） | `mean_abs_diff=0.027, max=1`（31,104,000 字节，差异全部 ≤1 LSB=10bit→8bit 取整固有差）；`hw mean=126.24` 铁证非黑屏 |
| 8-bit Main 回归 | `EXIT=0` 无回归 |
| glamor/GLX | `glamor X acceleration enabled on MWV207` + glxinfo 直渲加速（全量重装后 14:08） |

### 7.5 事故教训（热替换）

13:49 曾只替换 INST_DIR 的 dri+VA 两个 .so：旧 Xorg 死于运行中文件替换（缺页读到半新半旧），新 Xorg 被 **Mesa megadriver 版本校验拒绝**——`DRI driver not from this Mesa build ('24.1.2 (git-6cb20bd761)' vs '24.1.2 (git-3cb2d35af1)')`：Mesa 加载器比对 libEGL 与 dri 驱动内 bake 的 git 版本串。**mwv207 载荷 10 个文件必须同一构建（commit id 一致）整体替换**，不可只换 dri/VA；正式路径永远是 `switch-mwv207-oss.sh install --force`（14:08 全量重装已验证）。

### 7.6 精度说明

PPU `out_cut_8bits` 降位会丢弃 2 个 LSB（10bit→8bit）。对 8-bit 显示链路（当前全部显示路径）无可感知差异；像素校验 `max=1` 的差异即来源于此（软解侧是 dither）。若未来需要全 10bit 显示输出，需补 R16_UNORM 纹理格式码值（无权威来源）+ 10bit 扫描 out（KMD 侧另需核查）——属硬件文档依赖项，当前不具可行性。
