# VS 里 RGBA32F texelFetch 只回 2 分量 —— 最小回归用例

对应 `reports/FF-RENDER-20261102.md` 第 10 节（火狐"页面图元全不可见"的根因）。

## 现象
顶点着色器里对 RGBA32F 纹理做 `texelFetch`，在 mwv207 上只拿到 `(x, y, 0, 1)`（z/w 丢失）；
同一纹理同一纹素在片元着色器里正确，`isampler2D` 在 VS 里也正确。

## 复现
```sh
cc -O1 -o /tmp/wrreplay tests/vstf-128bit/wrreplay.c -lEGL -lGLESv2
# 我们（构建树或 /tmp/gd2 的插桩 DRI）
DISPLAY=:0 LIBGL_DRIVERS_PATH=/tmp/gd2 /tmp/wrreplay tests/vstf-128bit/vt.vs.glsl tests/vstf-128bit/passthru.fs.glsl vt
# 参考（llvmpipe）
DISPLAY=:0 LD_LIBRARY_PATH=/usr/lib/aarch64-linux-gnu \
  LIBGL_DRIVERS_PATH=/usr/lib/aarch64-linux-gnu/dri \
  __EGL_VENDOR_LIBRARY_FILENAMES=/usr/share/glvnd/egl_vendor.d/50_mesa.json \
  /tmp/wrreplay tests/vstf-128bit/vt.vs.glsl tests/vstf-128bit/passthru.fs.glsl vt
```

## 判据
- **修复前**：mwv `PT(32,32)=(166,204,128,255)`，llvmpipe `(166,204,242,255)`（z 分量为 0）。
- **修复后**：两边逐字节一致（`(166,204,242,255)`，`FB_HASH` 相同）。

## 根因位置（待修）
- `src/mwv207-umd/drivers/mwv207/nir/mwv207_lower.c` → `mwv207_lower_128bit()` 只对 FRAGMENT 跑
  `mwv207_lower_tex_128_impl()`；后者又写死 `tex_record[PIPE_SHADER_FRAGMENT]`。
- `src/mwv207-umd/drivers/mwv207/mwv207_draw.c` → `mwv207_update_shader()` 不给 VS 做
  `mwv207_set_current_shader()`，VS 采样器视图变化后不会重编译。
- `src/mwv207-umd/drivers/mwv207/mwv207_sampler.c` → `set_sampler_views()` 曾把全局
  `ctx->info.tex_128bit` 清零（"最后一个 stage 说了算"）。
