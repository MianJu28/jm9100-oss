# N14 reset 通路探针（实测记录）

N14（`22145e9`）改的是 KMD 的异常路径：3D/2D/DEC/ENC 的 `reset`/`destroy` 不再
`dma_fence_put` 掉在飞 fence 而不 signal，而是统一 `dma_fence_set_error(-EIO)` +
`dma_fence_signal`。这个路径平时不会走到，必须**故意制造一次 GPU 超时**才能实测。

本目录的探针就是干这个的：提交一个耗时 > 2 s 的 3D 作业，让调度器超时看门狗
（`mwv207_sched_timedout_job()`，超时 2 s、`hang_limit` 5）连续复位 3D 管线 6 轮，
同时用 kprobe 记录：

- `n14_tdr: mwv207_sched_timedout_job` —— 超时通路确实被走到；
- `n14_fc: pipe_3d_force_complete` —— N14 新增的“完成在飞 fence”函数确实被调用；
- 用户态 `glClientWaitSync` 最终返回（而不是永久阻塞），复位后 GPU 仍可用。

## 用法

```sh
# 需要 root（tracefs / dmesg）；DISPLAY 默认 :0，XAUTHORITY 默认 admin 的
cd tests/n14-reset
sudo DISPLAY=:0 XAUTHORITY=/home/admin/.Xauthority ./run-n14-reset.sh
# 可调迭代数（默认 2e9；1e9 也足够）
ITERS=1000000000 sudo ... ./run-n14-reset.sh
```

退出码：0 PASS / 2 判定失败 / 3 环境或 kprobe 问题 / 4 探针看门狗。

## 风险（必读）

脚本会**故意挂死 3D 引擎 6 次**（每次 2 s 超时后硬复位）。实测中桌面只是卡顿约
12 s，复位后立即恢复；但如果硬件复位本身失败，可能整机无响应。运行前先保存工作，
并确保有 SSH（本机 22 端口已开）或电源键兜底。

## 实测结果（2026-11-10，内核 6.6.155-arm64-desktop-hwe，模块 `039F972EFE3279A155EE225`）

```
PROBE-RESULT: PASS（reset 通路被走到，fence 被完成，复位后可用）
probe exit code: 0
mwv207_sched_timedout_job : 6
pipe_3d_force_complete    : 6
...
[  874.885993] mwv207: job from 'hangprobe' to 'mwv207_3d0', timed out!
[  876.933942] mwv207: job from 'hangprobe' to 'mwv207_3d0', timed out!
[  878.981895] mwv207: job from 'hangprobe' to 'mwv207_3d0', timed out!
[  881.029823] mwv207: job from 'hangprobe' to 'mwv207_3d0', timed out!
[  883.077771] mwv207: job from 'hangprobe' to 'mwv207_3d0', timed out!
[  885.125713] mwv207: job from 'hangprobe' to 'mwv207_3d0', timed out!
RESULT: PASS（TDR 6 次，force_complete 6 次，探针未阻塞）
```

- 6 次超时、6 次 `pipe_3d_force_complete()`，一一对应：每轮 reset 都有 1 个在飞
  fence 被 N14 的代码完成，没有漏、没有重复。
- 用户 fence 在 12.3 s 后完成（= 6 × 2.05 s 超时周期）；随后 64 次迭代的短作业
  立刻完成，`glxinfo -B` 仍是 `MWV207 / GL 4.0 / GLES 3.2 / 2048MB`，
  `glxgears` 100 FPS（vsync 上限），dmesg 无 WARN/BUG/Oops/hung task。
- 校准数据（同一探针，未触发 TDR 时）：64 次迭代 0.0 s、1e6 → 0.1 s、
  5e6 → 0.6 s、2e7 → 12.3 s（已跨过 2 s 超时线）。

## 这条探针证明了什么、没证明什么

证明：

1. N14 的代码路径在真实硬件上被执行（kprobe 命中 6/6），函数不崩、不死锁；
2. 超时 → `drm_sched_stop` → `pipe_3d_reset`（含 force_complete）→ `resubmit` →
   `start(full_recovery)` 整条链路可反复跑 6 轮，硬件复位后 GPU 可继续工作；
3. 用户态可见的 fence 最终完成，没有永久阻塞。

没证明（如实记录）：

- **“reset 时用户态永久等待”这个旧故障场景本身**：读 6.6 `drm_sched` 源码可知，
  `drm_sched_stop()` 会把坏 job 的 `parent` 回调摘掉并置 NULL，
  `drm_sched_start(full_recovery=true)` 对 parent 为 NULL 的 job 直接
  `drm_sched_job_done(job, -ECANCELED)`，用户 fence 因此仍会完成。也就是说超时
  路径上 N14 的 `-EIO` signal 没有“用户态可见的等待者”，我们无法用“修复前会卡死”
  来做对照实验。N14 的实际收益在于：fence 状态正确（带错误信号，而不是被静默
  丢弃）、内部等待者（TTM/devfreq/驱动内 `dma_fence_wait`）不会被永久挂住、
  卸载/销毁路径不留下永远 unsignaled 的 fence。
- 2D / DEC / ENC 三条管线的 force_complete 没有被本探针覆盖（构造它们的超时要
  真的让 2D 或解码引擎挂死，风险高于 3D）；destroy 路径同理（需要驱动卸载时仍有
  在飞 fence，正常使用时 refcnt 不允许卸载）。