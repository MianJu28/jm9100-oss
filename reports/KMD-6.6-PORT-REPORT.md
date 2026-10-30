# KMD 6.6 移植报告（步骤 A：只构建，不部署）

时间：2026-10-30　工作树：`src/kernel-6.6/`（独立 git 仓，基线提交 `62ba212`，修复提交见 `git log`）

---

## 1. 为什么走这条路

原本想走"K-b：取 deepin 的 6.6.155 内核源码，在它的 mwv207 上打补丁"。**实测此路不通**：

| 探测点 | 结果 |
|---|---|
| 运行内核来自哪个源 | `community-packages.deepin.com/beige`，suite `crimson`，组件 **`commercial`** |
| 源码包名 | **`linux-upstream`** 25.01.00.33（不是 `linux`） |
| `deb-src` 覆盖 | 只配了 `main` |
| `main` 的 Sources | 只有 `linux-deepin-hwe` 6.6.155-1 —— 但它是**元包**，`orig.tar.gz` 仅 443 字节；另有 Debian `linux` 6.6.9（loong64） |
| `commercial` 的 Sources | 存在（HTTP 200），但**全库 8 个源码包**，无 `linux-upstream` |
| `pool/commercial/l/linux-upstream/` | 可列（autoindex），36 项全是 `linux-{image,headers}-{6.6.155,6.18.44,6.18.48}-{amd64,arm64,loong64}` 与 `-dbg`，**无 .dsc / 无 .tar** |

结论：Deepin 未公开发布 6.6.155 内核源码。于是改走 **K-a**，但换了基线。

## 2. 基线选择：社区 6.6 移植

`Icenowy/mwv207-dkms`（`PACKAGE_VERSION=20260308`）= 开源 mwv207 KMD 的现代内核移植，
带 `LINUX_VERSION_CODE` 门控覆盖 6.6 → 6.19。**实测可原样对着运行内核编译**：

```
make -C /lib/modules/6.6.155-arm64-desktop-hwe/build M=$PWD CONFIG_DRM_MWV207=m
→ 全量通过
```

三条旁证说明它和"正在跑的那个 in-tree 模块"同源：

| 项 | 社区 port | 运行中的 in-tree 模块 |
|---|---|---|
| `author` | shanjinkui | shanjinkui |
| `description` | MWV207 Graphics Driver | MWV207 Graphics Driver |
| `depends` | ttm,drm,drm_kms_helper,drm_ttm_helper,gpu-sched,drm_display_helper,i2c-algo-bit | 完全相同 |
| `alias` | `pci:v00000731d00009100…` | 相同 |
| 驱动内 pipe 名 | `mwv207_3d0` | 相同 |

且它保留 **legacy 6 个 ioctl**（`INFO/GEM_CREATE/GEM_MMAP/GEM_WAIT/CTX/SUBMIT`，`JM_*` = 0），
所以开源 UMD 无需任何改动。

## 3. 移植了哪些修复

来源：5.10 树的 `0ec4816`（K1/K3–K8/K10）+ `d94c106`（softdep）。

| 编号 | 文件 | 改动 |
|---|---|---|
| K1 | `mwv207_bo.c` | `if (!jbo->flags & (1<<0))` → `if (!(jbo->flags & (1<<0)))`（原式恒为 0，kmap 权限检查从未生效） |
| K10 | `mwv207_ctx.c` | `scheds[6]` 定长栈数组加 `MWV207_MAX_SCHED_PER_ENGINE` 界检查；`BUG_ON(…)` → `WARN_ON(…) + -EINVAL` |
| K3 | `mwv207_db.c` | `copy_to_user()` 残值不再当 ioctl 返回；失败返回 `-EFAULT`；`bsearch` 前初始化 `rec.val` |
| K4 | `mwv207_db.c` | `sort`/`bsearch` 元素尺寸由硬编码 `8` 改 `sizeof(*db->db)` |
| K5 | `mwv207_db.c` | `mwv207_db_fini()` 后 `jdev->db = NULL` |
| K8 | `mwv207_irq.c` | `irq_create_mapping()` 返回值检查；失败走 `free_mapping_partial` 回收已建映射 |
| K6 | `mwv207_submit.c` | 5 处 `copy_from_user` 残值 → `-EFAULT`（原先部分拷贝会当成成功返回给用户态） |
| K7 | `mwv207_submit.c` | `relocs` 拷贝失败时 `kvfree + NULL` |
| — | `mwv207_submit.c` | `engine_id` 是 `u32`，`>= 0` 恒真 → 删除 |
| patch 0008 | `mwv207_drv.c` | `MODULE_SOFTDEP("pre: governor_performance")` |

合计 **6 文件 / +72 −20**。

## 4. 刻意**没有**移植：K2

K2 = 5.10 里"`ttm_bo_init` 失败时补 `drm_gem_object_release()` + `kfree()`"。

**在 6.6 照搬会 double free。** 依据 v6.6.155 的 `drivers/gpu/drm/ttm/ttm_bo.c`：

```c
int ttm_bo_init_validate(...)
{
	ret = ttm_bo_init_reserved(...);
	if (ret)
		return ret;
	...
}
/* ttm_bo_init_reserved() 的失败路径： */
err_unlock:
	if (!resv)
		dma_resv_unlock(bo->base.resv);
err_put:
	ttm_bo_put(bo);          /* → kref 归零 → bo->destroy(bo) */
	return ret;
```

而 `bo->destroy` 就是本驱动的 `mwv207_ttm_bo_destroy()`，内部**本就**执行
`drm_gem_object_release(&jbo->tbo.base); kfree(jbo);`。所以 6.6 的 TTM 已经把
K2 修的问题处理掉了，再加一次就是二次释放。

## 5. 构建与自检

```
make -C /lib/modules/6.6.155-arm64-desktop-hwe/build M=src/kernel-6.6 CONFIG_DRM_MWV207=m
```

| 自检项 | 结果 |
|---|---|
| 编译 | 通过。唯一警告 `mwv207_drv.c:246 unused variable 'primary'` —— **基线已有**，与本轮改动无关 |
| 产物 | `src/kernel-6.6/mwv207.ko`，5,857,816 B |
| vermagic | `6.6.155-arm64-desktop-hwe SMP preempt mod_unload modversions aarch64` —— 与运行内核**逐字相同** |
| `name` | `mwv207` |
| `softdep` | **`pre: governor_performance`**（基线为无） |
| `alias` | `pci:v00000731d00009100sv*sd*bc*sc*i*` |
| `srcversion` | `EC9AA08FD54F3FF748CAB5C`（基线 `EC6A9C32C47FC266FB28270`，确认代码已变） |

体积 5.6 MB 是因为带调试信息；正式安装时用 `INSTALL_MOD_STRIP=1` 可收敛到 250 KB 量级。

## 6. 部署（**尚未执行**）

`dkms.conf` 已就绪：`PACKAGE_NAME=mwv207`、`BUILT_MODULE_NAME[0]=mwv207`、
`DEST_MODULE_LOCATION=/kernel/drivers/gpu/drm`、`AUTOINSTALL=YES`。

风险与前提：

1. **这是换显示驱动**，模块起不来就是黑屏 —— 必须先备好 SSH 或 TTY 切换，并写好一键回滚（恢复内核内建模块 + `depmod`）。
2. 两个同名 `mwv207.ko`：内核内建 vs DKMS `/updates/dkms`。depmod 顺序上 `/updates` 优先，但需 `modprobe -r mwv207 && modprobe mwv207`（或重启）才生效；内核升级后 DKMS 自动重建。
3. 签名不挡路：`SecureBoot disabled`、lockdown `[none]`。

## 7. 部署后要验的清单

- `DRM_IOCTL_VERSION` → 期望 `mwv207 0.1.0/20220407`
- `glxinfo -B` → renderer MWV207、GL 4.0
- `tests/tri-test`、`tests/fbo-test` → PASS
- `vainfo`、`clinfo` → MWV207 / rusticl 3.0
- KMS：分辨率/刷新率、双屏（若接）、DPMS、S3/S4 唤醒
- `cat /sys/class/devfreq/*/governor` → `performance`（验证 softdep 生效）
- 内存压力：反复开关应用看 slab/BO 是否泄漏（K2 之外的路径）

## 8. 结论

步骤 A 完成：**已产出一个"6.6 兼容 + 保留 legacy ABI + 含我们全部 KMD 修复"的 `mwv207.ko`**，
未安装、未加载。是否部署（步骤 B）待定。
---

## 9. 切换脚本与演练（2026-10-30）

`switch-mwv207-kmd.sh` 已写好并通过演练 —— **全程零副作用，未加载、未安装任何模块**。

命令：`check` / `build` / `status` / `install` / `rollback` / `verify`，
另有 `--root <假根> rehearse`（假根演练）与 `--dry-run install`（只打印命令序列）。

### 演练证明了四件事

| 验证项 | 结果 |
|---|---|
| install 的落点 | 自建 `.ko` 正确落到 `.../updates/dkms/mwv207.ko`，`srcversion EC9AA08F…` |
| rollback 的清除 | 删掉该副本后，假根回到"只有内建模块"的状态 |
| 内建模块不变性 | 演练前后 md5 均为 `6c43aa45…`；备份副本与原件逐字节一致 |
| depmod 优先级 | `depmod -b <假根> -n` 中 `mwv207` 的首个落点是 `updates/dkms/mwv207.ko`，且带 `softdep mwv207 pre: governor_performance` |

**第 4 项是关键**：两个模块同名（`mwv207`），如果 depmod 挑的是 `/kernel/...` 那份，
装了也白装。演练里用 `depmod -n`（只打印不写文件）证明了 `/updates` 优先。

### 另外两条实测

- `build` 复跑通过，vermagic 与目标内核逐字一致。唯一警告 `mwv207_drv.c:246 unused variable 'primary'` 是基线自带的。
- 无 root 时 `install` 正确拒绝：`[FAIL] 需要 root`。

### 只能在真机上验的（脚本留了判据 + 自动回滚）

停 lightdm 后 refcnt 能否归零、`modprobe -r` 是否真成功、新模块 probe 是否成功、Xorg 是否点亮。
任一步失败，脚本自动 `do_rollback`（删副本 → depmod → 重加载内建 → 起 lightdm → 核对内建 md5 未变）。

### 部署前提

本机**没有免密 sudo**，真正部署需要手动执行（输入密码）：
```
sudo ./switch-mwv207-kmd.sh install      # 9 步 + 自动回滚
sudo ./switch-mwv207-kmd.sh rollback     # 随时恢复
```
