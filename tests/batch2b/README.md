# 批次 2b 探针（2026-11-03）

三条问题（O5/E8/E3）各自的行为验证装置。编译/运行需要 X (:1) 与 build-full 产物目录。

## E8：多视口诚实性

```sh
cc -O1 -o /tmp/vpquery vpquery.c -lX11 -lGL
DISPLAY=:1 /tmp/vpquery
```

期望（当前驱动）：

```
Mesa: error: GL_INVALID_ENUM in glGetIntegerv(pname=GL_MAX_VIEWPORTS)
GL_MAX_VIEWPORTS=... err=0x500 ext=[no ARB_viewport_array]
```

结论：`PIPE_CAP_MAX_VIEWPORTS=1` 且 `GL_ARB_viewport_array` 未暴露，
应用提交不了多视口，忽略 `num_viewports` 与声明一致 → 非缺陷。

## E3：压缩纹理拷贝

```sh
cc -O1 -o /tmp/etc2copy etc2copy.c -lEGL -lGLESv2
DISPLAY=:1 /tmp/etc2copy
```

期望：ETC2_RGBA8 纹理经 `glCopyImageSubData` 后，源/目标采样结果 `DIFF=0`。
压缩拷贝走 `resource_copy_region` 的 CPU 回退，`copy_layer` 的压缩 blit 分支
（`blit.c:568-584`）不可达。

## O5：提交失败注入

```sh
cc -shared -fPIC -o /tmp/failsubmit.so failsubmit.c -ldl
DISPLAY=:1 LD_PRELOAD=/tmp/failsubmit.so \
  LIBGL_DRIVERS_PATH=<build-full>/src/gallium/targets/dri \
  MESA_LOADER_DRIVER_OVERRIDE=mwv207 timeout 5 glxgears -swapinterval 0
```

期望：持续 `mwv207_drm_cmdbuf_commit: submit failed` 输出，进程被 timeout
终止（124）而不是崩溃/挂死；`mwv207_flush()` 在 `out_fence_fd<0` 时返回
NULL fence，不再造“永远就绪”fence。
