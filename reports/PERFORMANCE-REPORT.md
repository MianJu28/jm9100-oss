# glmark2 性能调查报告：开源栈 800 vs 厂商栈 4800

> 调查日期：2026-10-21 · 环境：Deepin 25 aarch64 / 6.6.143-arm64-desktop-hwe /
> JM9100 (0731:9100, subsystem 0x9101) / mwv207 开源全栈 (Mesa 24.1.2 git-6cb20bd)

## 一、结论摘要

| 项 | 结论 |
|---|---|
| 发现并修复的缺陷 | **KMD devfreq 注册在每次启动时即失败**（governor_performance.ko 无人加载），GPU 频率管理从未生效 → 已修复并持久化 |
| 修复后干净环境得分 | **851 分**（稳定复现 852/851），vs 用户观测基线 ~800 |
| 厂商 4800 的差距定性 | **非频率问题**（已在驱动额定上限 1000MHz）；GPU 光栅吞吐硬件上限 ~48-53 Gpx-eq/s 决定重场景天花板；简单场景受窗口系统/合成器路径压制；厂商测量条件不可考证 |
| 本机频率修复的实际增益 | 对本机增益有限（vbios 引导频率≈上限，terrain 类 GPU-bound 场景前后 114→118）；**修复的价值在于正确性与普适性**——vbios 引导低频的机型上该缺陷会让性能塌方数倍 |

## 二、缺陷：devfreq 启动即失败（已修复，patch 0008）

### 现象链

1. `/sys/class/devfreq/` 为空——GPU 从未注册任何调频设备；
2. dmesg 铁证（每次开机必现）：
   ```
   devfreq_mwv207_3d0: devfreq_add_device: Unable to find governor for the device
   devfreq_mwv207_2d0: devfreq_add_device: Unable to find governor for the device
   ```
3. mwv207 KMD 的 devfreq 注册代码（5.10 源 `mwv207_devfreq.c:192`，6.6 in-tree 同源）：
   ```c
   pipe->devfreq = devm_devfreq_add_device(dev, &mwv207_devfreq_profile,
                              DEVFREQ_GOV_PERFORMANCE, NULL);
   ```

### 根因

`devm_devfreq_add_device()` **不会** `request_module` 所需 governor。本机（Deepin 25 桌面）没有任何其他 devfreq 设备先触发 `governor_performance.ko` 自动加载（系统里该模块现成存在），于是 mwv207 probe 时两个 devfreq 注册全部失败，GPU PLL 停留在 vbios 初始化值——频率管理完全缺失。

这是**开源 KMD 适配缺陷**：驱动声明了 governor 依赖却未声明模块依赖。厂商闭源栈自带完整频率管理（或随驱动包加载 governor），不受此影响。

### 修复（双轨）

| 轨道 | 载体 | 适用 |
|---|---|---|
| **内核侧正解** | `patches/0008-KMD-devfreq-governor-softdep.patch`：`mwv207_drv.c` 追加 `MODULE_SOFTDEP("pre: governor_performance")` | 任何用开源 KMD 源码构建的场合 |
| **系统侧热修**（本机在用） | `/etc/modprobe.d/mwv207-devfreq.conf`：`softdep mwv207 pre: governor_performance` | 在跑的 distro in-tree 驱动（不可原地打补丁） |

验证：
- `modprobe --show-depends mwv207` 确认 governor_performance.ko 排在 mwv207.ko **之前**——冷启动 udev 走 modprobe 时必然先拉 governor，无竞态；
- 热重载实测（停 lightdm → rmmod → modprobe → 起 lightdm，`/var/log/mwv207-reload.log`）：
  ```
  devfreq_mwv207_3d0: performance  cur=1000MHz  min=50  max=1000
  devfreq_mwv207_2d0: performance  cur=1200MHz  min=50  max=1200
  ```
- 频率表来自 KMD 自身：subsystem 0x9101 → `pipe_3d_max_freq_get()=1000MHz`、2D=1200MHz（`mwv207_pipe_3d.c:489`）；50MHz 起 50MHz 步进建 OPP 表。

## 三、提频后的"反转"与四因子分离

修复 devfreq 后首跑 Score=519（带后台负载）——**低于**修复前的 ~800。逐因子剥离：

### 因子①：后台负载（最大干扰项）

- Firefox（含 Isolate 进程 75% CPU）+ DDE 合成器同时运行时 519 分；
- 杀掉 Firefox 后 **852 / 复测 851**（稳定）——同一栈同一频率，纯粹负载差 64%；
- 教训：**本机任何性能结论必须在干净环境测量**（`pkill firefox; sleep 2` 后再跑）。

### 因子②：合成器

- kwin_x11 合成器挂起（`qdbus org.kde.KWin /Compositor suspend`）：简单场景 build 582→965 FPS（**+66%**）；
- GPU-bound 场景（terrain）持平——合成器开销只影响轻负载场景的窗口系统路径。

### 因子③：GPU 光栅吞吐上限（硬件级，重场景天花板）

自研离屏探针（GBM renderD128 + FBO + 每帧变 uniform 强制真实 GPU 工作 + glFinish 同步）：

| 分辨率 | 64-pass FPS | 等效光栅吞吐 |
|---|---|---|
| 800×600 | 1559 | **47.3 Gpx-eq/s** |
| 1600×1200 | 736* | **53.3 Gpx-eq/s** |

（*按像素数反比线性换算；两分辨率独立实测，线性关系成立 = GPU-bound 铁证。）

对照 glmark2 重场景（干净环境）：refract 98 / terrain 118 / jellyfish 229 FPS——与 ~48-53 Gpx-eq/s 的硬件上限吻合，**这些场景在当前频率下不可能更快**。

### 因子④：CPU 提交往返（非瓶颈）

探针分离测试：flush 模式提交往返 0.087ms/帧（11487 FPS 饱和）——CPU→GPU 提交路径不是限制。

### 附：swap 路径与 vsync

- GBM 离屏 swap 吞吐 18-19 GB/s（800×600=9523FPS，1080p=2312FPS）——内存带宽充裕；
- glmark2 实际走 **DRI3**（`LIBGL_DEBUG=verbose`："Using DRI3 for screen 0"），非 DRI2；
- 显示器 1920×1080@100Hz：glxgears 默认锁 ~100FPS（vsync），`vblank_mode=0` 解锁后 1808FPS——测吞吐务必关 vsync。

## 四、与厂商 4800 的差距定性

1. **不是频率**：devfreq 已锁 1000MHz（本 subsystem 驱动额定上限）。本机修复前后 GPU-bound 场景 114→118，说明 vbios 引导频率本已接近上限——频率修复在本机增益有限，其价值在正确性与低频引导机型的普适防护。
2. **不是提交路径**：0.087ms/帧往返充裕。
3. **硬上限**：~48-53 Gpx-eq/s 光栅吞吐决定 terrain/refract 类场景 ~100-120 FPS 天花板（在 800×600 窗口下）。几何平均后重场景把总分压住。
4. **窗口系统路径**：合成器挂起可给简单场景 +66%——若厂商在无合成器/离屏条件下测量（其 glmark2 构建可能支持 --off-screen，deepin 版无此选项），得分可达数千分。**厂商测量条件（窗口尺寸/合成器/分辨率/glmark2 版本/频率策略）不可考证**，4800 无法在本机复现比对。
5. 本机可复现的**公平数字**：干净环境、800×600 窗口、vsync 关、DRI3、合成器在跑 = **851**；合成器挂起 ≈ 1200+（按 +66% 场景增益折算）。

## 五、交付物

| 文件 | 内容 |
|---|---|
| `patches/0008-KMD-devfreq-governor-softdep.patch` | KMD 源码正解（MODULE_SOFTDEP） |
| `/etc/modprobe.d/mwv207-devfreq.conf`（已部署） | 本机 in-tree 驱动的系统级修复，冷启动持久 |
| `/tmp/gpu-load6h.c`、`/tmp/gpu-load7.c`、`/tmp/swap-thrash.c` | GPU 吞吐 / CPU 提交 / swap 路径分离探针 |
| `/tmp/glmark2-clean{,2}.log` | 干净环境 852/851 完整场景留档 |
| `/var/log/mwv207-reload.log` | 热重载验证日志 |

## 六、遗留与建议

- 厂商 4800 的差距在无法获得厂商测量条件前**不可归因**；若用户提供厂商侧 glmark2 完整输出（场景 FPS 列表），可逐场景对账定位；
- devfreq 目前锁 `performance` governor（与厂商性能形态一致）；如需功耗平衡，可换 `simple_ondemand`（KMD 的 utilization_show 已为其准备 reset 逻辑）——需加载 governor_simple_ondemand 模块；
- 测量方法学：本机跑分必须干净环境 + `vblank_mode=0`；单场景 `-b <name>` 可快速复测。
