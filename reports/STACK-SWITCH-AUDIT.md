# 开源驱动栈 + 切换脚本 复审报告

审查时间：2026-10-30 22:4x　审查对象：`/home/admin/jm9100-oss`（仓库）、`/usr/local/lib/mwv207-oss`（安装态）、`switch-mwv207-oss.sh`（597 行）

---

## 〇、一句话结论

**安装机制本身是健康的、实测有效的**（glue 副本 SONAME 全部正确、`$ORIGIN` rpath 生效、VA/VDPAU/OpenCL 全部解析到自建库、glamor 硬件加速点亮）。
**但它装的是 2026-10-21 的旧载荷** —— `payload-24` 与本轮源码树（含补丁 0009/0010，GLES 3.2）已经脱节，且脚本没有任何机制能发现这件事。

---

## 一、开源栈组成核对

| 组件 | 安装位置 | 实测 | 结论 |
|---|---|---|---|
| GL/EGL/GLES/GBM | `/usr/local/lib/mwv207-oss/*` | `ldconfig -p` 全部指向该目录；`ldd glxinfo` → `libGL.so.1 => /usr/local/lib/mwv207-oss/libGL.so.1` | ✅ 抢占成功 |
| SONAME 胶水副本 | `libEGL.so.1`/`libGL.so.1`/`libGL.so`/`libgbm.so.1`/`libGLESv2.so.2`/`libGLESv1_CM.so.1` | `objdump -p` 逐个 SONAME 与文件名一致 | ✅ 正确 |
| DRI | `…/dri/mwv207_dri.so` → `$INST_DIR/mwv207_dri.so` | `RUNPATH $ORIGIN`、`NEEDED libglapi_mwv207.so.0` 同目录可解析 | ✅ 正确 |
| VA-API | `/usr/lib/aarch64-linux-gnu/dri/mwv207_drv_video.so` → `$INST_DIR/libmwv207_drv_video.so` | `vainfo` 报 `Mesa Gallium driver 24.1.2 for MWV207`，H264~High10 / HEVC Main10 VLD | ✅ 可用 |
| VDPAU | `/usr/lib/aarch64-linux-gnu/vdpau/libvdpau_mwv207.so.1` → `$INST_DIR/libvdpau_mwv207.so.1.0.0` | `G3DVL 1.0`，8192² surface | ✅ 可用 |
| OpenCL | `/etc/OpenCL/vendors/mwv207.icd`（厂商 `jmgpu.icd` 已改名 `.oss-disabled`） | `clinfo` → rusticl / MWV207 / OpenCL 3.0 | ✅ 可用 |
| 环境变量 | `/etc/environment.d/60-*` + `/etc/profile.d/60-*` | `RUSTICL_ENABLE=mwv207`、`VDPAU_DRIVER=mwv207` | ✅ 一致 |
| KMD | 内核自带 `mwv207.ko` | `intree: Y`、内核签名、本机无源码 | ⚠ 脚本不管、也无法管 |

**完整性的好消息**：`payload-24` 的 10 个实体文件 `sha256sum -c MANIFEST.sha256` **全部通过**；`payload-24` 与 `/var/lib/mwv207-oss/payload-cache` 的 10 个 md5 **完全相同**（当前无漂移）。

---

## 二、开源栈层面的问题（按严重度）

### A1 【阻断·最高】载荷与本轮源码脱节，装回去仍是 GLES 3.1

| | md5 | 版本/能力 |
|---|---|---|
| `payload-24/libmwv207_dri.so`（10-21 14:05） | `28ffca76…` | 24.1.2 原样，**GLES 3.1** |
| 已安装 `/usr/local/lib/mwv207-oss/mwv207_dri.so` | `28ffca76…` | 同上 |
| `build-full/…/libmwv207_dri.so`（10-30 21:42） | `20c4710d…` | 24.1.2 + 0009/0010，**GLES 3.2** |
| `build-full/…/va/libmwv207_drv_video.so`（10-29 11:03） | `75ccdd7a…` | 与载荷 VA（`54bb6591…`）**不同** |

即：`install` 装的是不含 GLES 3.2 / 鲁棒访问 / 16 位格式修复的旧二进制；且 VA 驱动也该重取。**必须先重打载荷，再谈 install。**

### A2 【中】`payload-24` 是手工产物，没有生成脚本

10 个实体文件 + `MANIFEST.sha256` + 一堆手写软链，全部靠人工从 `build-full` 拷出。没有 `make-payload` 之类步骤 → 每次重建都要靠记忆，天然会漂移（A1 就是这么来的）。建议补一个 `tools/make-payload.sh`：从 `build-full` 取 DRI + VA、保留已有 OpenCL/VDPAU、重算 MANIFEST、重建软链。

### A3 【中】三套 UMD 并存，回归测的不是在跑的那套

`src/mesa/build-mwv207`（22.3.7，仅 `tests/run-all.sh` 用）/ `/usr/local/lib/mwv207-oss`（24.1.2，实际在跑）/ `build-full`（24.1.2+补丁，未安装）。`tests/run-all.sh:6-8` 把 `LIBGL_DRIVERS_PATH` 钉死在 22.3.7 → 回归结论不能代表线上。

### A4 【低】EGL 存在厂商同名闭源库

`/usr/share/glvnd/egl_vendor.d/10_mwv207.json` 指向 `libEGL_mwv207.so.0` = `/usr/lib/aarch64-linux-gnu/mwv207/libEGL_mwv207.so.1.5.0`（厂商闭源）；我们的叫 `libEGL_mwv207.so.1.0.0`（`.so.1`）。当前 EGL 走 ld.so 里的 `libEGL.so.1`（我们的胶水），所以不冲突；但只要有人显式按 glvnd 名字加载，就会碰到闭源库。属于"知道就好"的风险点。

### A5 【低】`libGL.so` 与 `libGL.so.1` 是两份 3.3 MB 实体副本

功能正确（无后缀名条目是 glmark2 实证必需），但可以做成 `libGL.so → libGL.so.1` 软链省 3.3 MB。无实际影响。

### A6 【中】`PIPE_CAP_VIDEO_MEMORY` 上报错 1024 倍（源码问题，非脚本）

`mwv207_screen.c:175` `return 1024 << 20;`，该 cap 单位是 MB → `glxinfo -B` 显示 `Video memory: 1073741824MB`。建议重打载荷时顺手修掉。

---

## 三、切换脚本审查

### 3.1 值得保留的设计（先说好的）

1. **`$ORIGIN` 实体副本**（`deploy_payload` 尾部）——Xorg 经 dri 软链加载 `mwv207_dri.so`，其 `NEEDED libglapi_mwv207.so.0` 只能靠 rpath 解析。这是实测踩出来的，注释完整。
2. **`libGL.so` 无后缀名胶水条目**（`GLUE_COPIES`，注释引用 2026-10-21 11:44 glmark2 实证）——`dlopen("libGL.so")` 不走 SONAME 缓存，没这条会落 glvnd 开发名 → 系统 Mesa → llvmpipe。
3. **看门狗 PASS 优先**（`FAIL_MARKERS`/`PASS_MARKERS`，注释记录 09:43 假阳性事故）——加速成功后 `Initialized DRISWRAST` 仍会出现，若当失败会误杀真实加速。
4. **`grep -m1 "Time:"` 全文抓取**（`xorg_log_epoch`，注释记录 03:06/03:24 两次假回滚）——`Time:` 在 `(==) Log file:` 行内而非首行。
5. **三态快照/回滚**（`present|absent|derived`）——不无条件 `rm` MANAGED 路径，记录 10:19 实证。
6. **故障自愈**：install 内嵌 `wait_xorg_and_check`，失败自动 `do_rollback` + 重启 lightdm。

### 3.2 问题清单

#### S1 【高】`payload-cache` 优先于仓库载荷，且无内容校验

```bash
PAYLOAD_CANDIDATES=("${PAYLOAD_SRC:-}" "/var/lib/mwv207-oss/payload-cache" "$WORKDIR/payload-24")   # 56-60
```
cache 排在仓库目录**之前**。当前两者一致（10 个 md5 全同），但一旦在仓库重打载荷而没同步 cache，`install` 会**静默使用旧 cache**。`cmd_check` 只对最终选中的目录做 `sha256sum -c`，无法察觉"有更新的仓库载荷被跳过"。

> 建议：仓库目录存在 `MANIFEST.sha256` 时优先它；或对所选目录打印来源并二次确认。

#### S2 【高】`VERSION` 硬编码 + 幂等判断只比字符串

```bash
VERSION="24.1.2-fullstack-r1"                                    # 51
[ "$cur" = "oss-$VERSION" ] && [ $force = 0 ] && die "当前已是 oss 栈"   # 336
```
重打载荷后若忘记改 `VERSION`：不带 `--force` 直接拒绝（"已安装"），带 `--force` 则**用同一版本号重装不同内容**，`active-stack` 里看不出差别。当前 `active-stack` = `oss-24.1.2-fullstack-r1`（10-21 14:08），而源码树已前进到含 0009/0010 → 这条判断此刻正好是"挡住正确操作、放行错误操作"。

> 建议：`VERSION` 由载荷内 `MANIFEST.sha256` 的摘要派生（或写入 `payload/VERSION` 文件），不要手写。

#### S3 【中】`cmd_verify` 只查"存在/解析"，不查"内容"

`cmd_verify:510-563` 校验的是：4 个 glue 名在 `ldconfig -p` 指向 `$INST_DIR`、几个软链/ICD/ldconf 存在、Xorg 日志有 PASS 标记、`vainfo`/`clinfo`/`egl-smoke` 能跑。**从不校验 `$INST_DIR` 内文件的 sha256。**

实测证据：`$INST_DIR` 里 `libmwv207_dri.so` 与 `mwv207_dri.so` 是 **inode 6815774、nlink=2 的硬链对，mtime `2026-10-28 17:08`**，而 `active-stack` 记录与日志都停在 **10-21 14:08**、且切换日志里没有 10-28 的任何记录 → **安装目录被脚本之外的操作动过**（内容 md5 仍等于载荷，所以无害），但 verify 对此完全无感。

> 建议：verify 增加 `sha256sum -c` 对 `$INST_DIR`（把 MANIFEST 随载荷装到 `$CACHE_DIR`）。

#### S4 【中】没有载荷生成步骤（见 A2）

脚本假定 `payload-24` 已存在且正确，自身无法从 `build-full` 生成它。

#### S5 【中】`set -u` 而无 `-e`，关键步骤无返回码检查

`331-414` 内 `ln -sfn`（4 处，DRI/VA/VDPAU×2）、`ldconfig`、`touch "$STAMP"`、`systemctl restart lightdm` 均未 `|| die`。若某条软链失败（例如 `/usr/local/lib/aarch64-linux-gnu/dri` 目录在 deepin 不可变根下被重置），install 会报"完成"但 DRI 不生效，只在随后的 `cmd_verify` 里体现为 FAIL（而 `cmd_verify` 的 rc 被 `return 0` 忽略 —— 见 S6）。

#### S6 【中】install 成功路径忽略 `cmd_verify` 的返回码

```bash
log "install 完成: glamor 硬件加速已生效"
cmd_verify
return 0            # 410-413
```
`cmd_verify` 的 rc 被丢弃 —— 即便 VA/OpenCL/EGL 自检失败，install 仍以 0 退出。建议 `cmd_verify || die`（或至少把 rc 反映到退出码）。

#### S7 【低】看门狗启动前不清理旧 unit

```bash
systemd-run --collect --unit=mwv207-oss-watchdog … || log "[警告] 看门狗启动失败, 依赖脚本自检"   # 394-397
```
若上一轮看门狗仍在跑，同名 unit 会让 `systemd-run` 失败 → 静默降级为"仅靠 install 内联自检"。建议先 `systemctl stop mwv207-oss-watchdog` 再启动。

#### S8 【低】`cmd_watchdog` 超时"放行"

`486-508`：240 s 内既无 PASS 也无 FAIL → 记"未检测到失败标记，放行"并 `exit 0`。install 场景下 `wait_xorg_and_check` 会先因"无新日志"失败并回滚（安全）；但**独立运行**的看门狗在 X 彻底起不来时会放行。

#### S9 【低】厂商 ICD 用 `mv` 改名，非幂等

```bash
[ -f "$OPENCL_DIR/jmgpu.icd" ] && mv -f "$OPENCL_DIR/jmgpu.icd" "$OPENCL_ICD_DISABLED"   # 375
```
`apt upgrade mwv207-dev` 会把 `jmgpu.icd` 装回来 → 两个 OpenCL 平台（厂商的在 mwv207 KMD 下枚举 0 设备）。当前状态正常（`jmgpu.icd.oss-disabled` + `mwv207.icd`），但建议 `cmd_verify` 断言"`jmgpu.icd` 不存在"。

#### S10 【低】`/etc/environment` 只在变量缺失时追加

```bash
if ! grep -q '^RUSTICL_ENABLE=' "$SYS_ENVIRONMENT"; then … fi   # 386-388
```
若已存在但值不对（例如 `RUSTICL_ENABLE=llvmpipe`），不会纠正。

#### S11 【低】自安装副本与仓库脚本不同步

`/usr/local/sbin/mwv207-oss-switch`（10-21 18:25，md5 `73977339…`）≠ 仓库脚本（10-22 10:41，md5 `94ea77dc…`）。`watchdog`/`rollback` 由**自安装副本**执行 → 长期不 install 就等于用旧版回滚逻辑。建议 `cmd_status` 里对比两者 md5。

#### S12 【信息】脚本不管理 KMD，这是设计选择但缺一句风险提示

头注释说明 `mwv207` 已是 `modules.alias` 对 0731:9100 的默认。实测确认：内核自带 in-tree `mwv207.ko`（无本机源码）。含义是 **开源栈的 KMD 完全由发行版内核决定**；内核升级后若 in-tree mwv207 变化/缺失，脚本无任何保护或提示。建议在 `cmd_check` 里至少断言 `kernel_driver == mwv207`（已做）并在 `cmd_status` 显示内核版本与模块签名状态。

#### S13 【信息】`xorg_log_is_new` 的边界

`426-431` 用 `≥ 基准` 判定"新日志"。同一秒内的重复 install 会看到同一 epoch → 视为"新"（可接受）。`STAMP` 放 `/run` 是正确选择（重启即失效，避免旧 stamp 造成的假通过）。

---

## 四、建议的处置顺序

1. **修 `PIPE_CAP_VIDEO_MEMORY` 一行**（A6），并顺手把 O2/O3 两个一行修复一起打进去（见 `docs/api/11-FIX-FEASIBILITY.md` 批次 1）。
2. **写 `tools/make-payload.sh`**（A2/S4）：从 `build-full` 取 `libmwv207_dri.so`+`libmwv207_drv_video.so`，其余沿用，重算 MANIFEST，产出 `payload-25/`。
3. **`VERSION` 改为从载荷派生**（S2），并把 `payload-cache` 的优先级/校验修正（S1/S3）。
4. **重装 + 全量验证**：`install --force` → `cmd_verify` → 修 `tests/run-all.sh` 让它覆盖三套（A3）。
5. 低优先：S5/S6/S7/S9/S10/S11 做成一轮小改。

---

## 五、审查边界

- 未执行任何 `install`/`rollback`（会重启 lightdm、终止图形会话），全部为只读核对。
- `shellcheck` 未安装，静态检查仅 `bash -n`（通过）+ 人工通读 597 行。
- 未验证 `--force` 重装路径的实际行为（同上原因）。
- 10-28 17:08 那次对 `$INST_DIR` 的改动来源未能确定（无日志、无 shell 历史证据）；内容仍与载荷一致，故按"无害漂移"记录。