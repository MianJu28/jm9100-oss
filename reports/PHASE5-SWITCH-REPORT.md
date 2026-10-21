# 阶段5终报: JM9100 开源驱动切换脚本重构 + 真机点亮 + 冷启动持久化

> 日期: 2026-10-21 | 环境: Deepin 25 aarch64, 内核 6.6.143-arm64-desktop-hwe,
> GPU JingjiaMicro JM9100 [0731:9100] @ PCI 0000:07:00.0
> 前序: ANALYSIS-REPORT.md (逐行分析), COMPAT-TEST-REPORT.md (兼容性),
> HWDECODE-FEASIBILITY-REPORT.md (VA 硬解移植), FULLFEATURE-REPORT.md (全功能栈)

## 一、任务

用户要求 (m01886): 系统还原后, 逐行分析验证旧切换脚本是否仍生效, 清理不可用
代码, 将本次驱动修复写入切换脚本并提供回滚。

## 二、旧脚本 switch-to-oss.sh (690 行) 逐行分析结论

### 2.1 死代码 (全部 KMD 切换段, 共约 300 行)

系统还原后 modules.alias 中 `mwv207` 排在 `jmgpu` 之前, udev 按首 alias
自动加载 in-tree mwv207.ko。旧脚本的 modprobe.d / modules-load.d /
initramfs / grub / update-initramfs / update-grub 全部段不仅无用, 且写入
initramfs/grub 会污染引导链 —— 属于**危险死代码**。

### 2.2 过时载荷

携带 mesa 22.3.7: 主分支不可编译 (缺 isa.xml.h include), winsys 有未修复
缺陷, 且不含 video-codec (VA 硬解) / rusticl (OpenCL) / VDPAU。

### 2.3 致命缺陷 (三次安装失败的根因)

| # | 缺陷 | 位置 | 后果 |
|---|------|------|------|
| 1 | FAIL_MARKERS 含 "Initialized DRISWRAST" | 77 行 | glamor 点亮的同一引导里 GLX 也初始化 swrast 供应商, 看门狗把已成功加速的 Xorg 误杀回滚 (10:02 实证两行共存) |
| 2 | FAIL 优先于 PASS 检查 | wait_xorg_and_check / cmd_watchdog | 先见 DRISWRAST 即判死, 永不到达 PASS 判定 |
| 3 | xorg_log_epoch 用 head -n1\|sed | 365 行 | Xorg.0.log 首行是 "[时间戳] $" 空行, Time 在 "(==) Log file:" 行内 → epoch 为空 → 误判无新日志 |
| 4 | SONAME_LINKS 含 mwv207_dri.so 软链 | deploy_payload | cp same-file 失败被 \|\| true 吞掉, patchelf 改的是软链本体 |
| 5 | rollback 后一刀 rm MANAGED 路径 | do_rollback | 与快照三态语义矛盾: 快照恢复出的"装前已存在"状态被再次破坏, 只剩 DRI_LINK 悬垂 |

### 2.4 保留并沿用的设计

- 快照备份框架 (present/absent/derived 三态 manifest + store)
- systemd-run 看门狗思路
- 厂商 jmgpu.icd 禁用 (glvnd dlopen 成功即锁定不回退)
- systemd-run 瞬态单元作为可靠 root 后台执行通道

## 三、glvnd 拦截链破案 (为什么 Xorg 点亮 glamor 这么难)

Xorg(root, 零 env) 不直接链 GL: libglamoregl.so NEEDED libgbm.so.1 +
libepoxy; epoxy 先 dlopen "libEGL.so.1" 再 dlopen "libGL.so.1" 探测 GL 版本。

拦截链: 系统 glvnd 按文件名序读 10_mwv207.json (厂商 ICD) → dlopen 命中
厂商 libEGL_mwv207.so.0 → 厂商 EGL 在 mwv207 KMD 下 FATAL → glvnd 锁定
不回退 → glamor 退 llvmpipe。

**公开名 SONAME 方案 (最终解)**: 在 INST_DIR 放我们 EGL/GL/gbm 的
"公开名副本" (patchelf 改 SONAME 为 libEGL.so.1 / libGL.so.1 /
libgbm.so.1 + rpath $ORIGIN) + 00- 前缀 ld.so.conf.d 让 ldconfig 优先解析
公开名 → epoxy 的两个 dlopen 全部命中我们的库 → 完全绕开 glvnd。
libGL.so.1 副本是最后一块拼图: epoxy 探测 GL 版本用, 缺了则
glamor_egl.c:1013 `epoxy_gl_version() < 21` → "Ignoring GL < 2.1"。

mwv207_dri.so 必须**实体 + rpath $ORIGIN** (软链不行: NEEDED
libglapi_mwv207.so.0 要靠同目录 rpath 解析, 软链到载荷目录会让 loader
沿链接走, 实测失败)。

## 四、新脚本 switch-mwv207-oss.sh

```
用法: sudo switch-mwv207-oss.sh {check|install [--force]|rollback
                                   [--no-restart]|verify|status|watchdog}
INST_DIR   = /usr/local/lib/mwv207-oss          (载荷 211MB)
CACHE_DIR  = /var/lib/mwv207-oss                (状态+快照+载荷缓存)
LDCONF     = /etc/ld.so.conf.d/00-mwv207-oss.conf
DRI_LINK   = /usr/local/lib/aarch64-linux-gnu/dri/mwv207_dri.so
VA_LINK    = /usr/lib/aarch64-linux-gnu/dri/mwv207_drv_video.so
VDPAU_LINK = /usr/lib/aarch64-linux-gnu/vdpau/libvdpau_mwv207.so.1
OpenCL     = /etc/OpenCL/vendors/mwv207.icd (+ jmgpu.icd → .oss-disabled)
RUSTICL    = environment.d / profile.d / /etc/environment
```

关键机制:
- **三态快照**: MANAGED 路径 manifest 记 present/absent/derived, rollback
  按语义精确恢复 (装前有→还原, 装前无→移除, 衍生物→清理)。
- **PASS 优先看门狗**: 先查 "glamor X acceleration enabled" 再查 FAIL
  (DRISWRAST 不再是失败标记)。
- **xorg_log_epoch = grep -m1 "Time:"** 全文件抓取。
- **14 项安装自检**: 公开名胶水×4 + DRI/VA/VDPAU/ICD/LDCONF/jmgpu 禁用 +
  Xorg glamor 行 + vainfo + clinfo + egl-smoke, 失败自动回滚。
- **systemd-run 瞬态单元**执行 install (直接 setsid+nohup 会被 sudo 退出
  cgroup 清杀)。

## 五、六次安装实录 (问题→修复→成功)

| # | 时间 | 结果 | 教训 |
|---|------|------|------|
| 1 | 03:06 | ✗ 看门狗 5 秒误判 | Time 行抓取 bug |
| 2 | 03:24 | ✗ 129 秒等不到新日志 | 同上 + DRISWRAST 假阳性 |
| 3 | 09:43 | ✗ T+4s 回滚 | DRISWRAST 假阳性 (Xorg 实际软渲染, 但根因是误杀+cp same-file) |
| — | 10:02 | ✓ 手动部署点亮 | 破案: glamor 与 DRISWRAST 两行共存, glxinfo 铁证加速成功 |
| 4 | 10:16 | ✓ 9 秒 14 项全绿 | PASS 优先修复生效 |
| 5 | 10:27 | ✓ 干净系统起成功 | 回滚缺陷已修, install→rollback→install 双向可靠 |
| 6 | 10:29 | ✓ **终态安装** | 系统正式切换到开源栈 |

## 六、终态验证

### 6.1 热态 (安装后即时)

- Xorg.0.log: "glamor X acceleration enabled on MWV207
  (JingjiaMicro Corporation)" + "glamor initialized" + "[DRI2] DRI
  driver: mwv207" + "[DRI2] VDPAU driver: mwv207"
- glxinfo -B: direct rendering Yes / Accelerated yes / Vendor
  JingjiaMicro (0x731) / Device MWV207 (0x9100) / Version 24.1.2
- vainfo: 6 profile (H264 CB/Main/High/High10 + HEVC Main/Main10,
  全 VLD), X11/DRI2 与纯 DRM 双路径命中 mwv207
- clinfo: rusticl 平台, Device MWV207, OpenCL 3.0
- egl-smoke: EGL 全管线 OK (clear+finish on mwv207 hardware)

### 6.2 冷启动 (整机 reboot, 10:34:32) — "完美适配"终极证明

- 全新引导 Xorg 于 **+32 秒**自动点亮 glamor (零环境零干预)
- 冷启动后全栈五绿: GLX 直渲 / VA 6 profile (含 DRI2) / OpenCL /
  EGL 全管线 / [DRI2] VDPAU driver: mwv207
- STATE=oss-24.1.2-fullstack-r1 持久

### 6.3 回滚验证 (10:28)

install→rollback→install 双向往返:
- 快照三态恢复完整, 零悬垂残留 (INST_DIR/LDCONF/DRI_LINK/ICD 全按
  装前状态还原)
- jmgpu.icd 恢复原位, /etc/environment 恢复

## 七、交付物

- `switch-mwv207-oss.sh` — 新切换脚本 (check/install/rollback/verify/
  status/watchdog)
- 旧脚本 `switch-to-oss.sh` (690 行) 与 `archive/` (22.3 载荷/早期实验/旧
  测试) 已按用户要求彻底清理 (2026-10-21 12:15); 其缺陷与逐行分析结论仅存
  于本报告 §2, 供后续考古
- `payload-24/` + `/var/lib/mwv207-oss/payload-cache/` — 载荷源与系统缓存
  (24.1.2 全功能栈, MANIFEST 校验)
- 前序报告: ANALYSIS-REPORT.md / COMPAT-TEST-REPORT.md /
  HWDECODE-FEASIBILITY-REPORT.md / FULLFEATURE-REPORT.md
- `patches/0001-0006` — KMD/UMD 缺陷修复补丁集

## 八、遗留事项

1. **VP9 解码**: set_regs 寄存器序列未公开 (H264 有 94 处 set_reg 可作
   模板), 硬件编码用户态完全无公开资料。
2. **irq.c W1C 竞态**: 未修, 需硬件验证 (中断风暴风险)。
3. tests/ 里 fbo-test/tri-test/sdl-test 仅 .c 源码 (需要时编译, 现位于 tests/)。
4. 厂商闭源 OpenCL 在 mwv207 KMD 下 devices=0 (开源 rusticl 反超)。

## 九、追加: glmark2 llvmpipe 破案与修复 (2026-10-21 11:44)

用户报告 glmark2 落 llvmpipe ("failed to load driver: mwv207" +
GL_RENDERER llvmpipe, 系统 Mesa 24.3.0-1deepin9)。

### 9.1 破案

glmark2 通过 `dlopen("libGL.so")` (**无版本后缀开发名**) 取 GL:
- 该名不在 ldconfig 缓存 (缓存只收 SONAME 如 libGL.so.1) → loader
  落默认目录 → 命中 `/lib/aarch64-linux-gnu/libGL.so` (glvnd 开发名
  软链) → libGLdispatch + libGLX + libGLX_mesa (系统 Mesa 24.3) →
  其 dri 目录无 mwv207_dri.so → llvmpipe。
- 已有 libGL.so.1 SONAME 拦截对这条链完全无效。
- LIBGL_DRIVERS_PATH 对 Debian 24.3 dril 加载器无效 (LD_DEBUG 无
  mwv207_dri 尝试痕迹)。
- 把 mwv207_dri.so 放进系统 dri 目录也不行: 系统 24.3 dril 与我们
  24.1.2 驱动分发接口不兼容 (dlopen 成功但初始化失败)。

### 9.2 修复: 无后缀名 SONAME 胶水

`GLUE_COPIES` 新增 `[libGL.so]=libGL_mwv207.so.1.2.0` 条目——造
SONAME=libGL.so 胶水副本入 INST_DIR, ldconfig 缓存出现
`libGL.so => /usr/local/lib/mwv207-oss/libGL.so` (排系统前),
`dlopen("libGL.so")` 直接命中我们 → DRI2 直渲链路。

### 9.3 验证

- glmark2 零 env: GL_RENDERER **MWV207** / Mesa 24.1.2 / GL 4.0
  compat —— 硬件加速点亮; 完整基准 **Score 533** (loop 场景 ~634 FPS)。
- glxinfo 回归无影响 (direct Yes / Accelerated yes / MWV207)。
- 快照语义: INST_DIR 整目录 absent 条目, 新胶水自动被回滚覆盖;
  payload-cache 源完好, 全新安装自动再生。
