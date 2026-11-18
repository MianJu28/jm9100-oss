# mwv207 OSS DDX —— 从 xserver modesetting 派生，目标是接入 mwv207 的
# overlay 平面(WIN0) 直扫路径，消除窗口态 Present 的整帧拷贝。
mwv207ddx_srcs = [
    'dri2.c',
    'driver.c',
    'drmmode_display.c',
    'dumb_bo.c',
    'pageflip.c',
    'present.c',
    'vblank.c',
]

shared_module(
    'mwv207oss_drv',
    mwv207ddx_srcs,
    name_prefix: '',

    include_directories: [inc, xorg_inc],
    c_args: xorg_c_args,
    dependencies: [
        common_dep,
        udev_dep,
        libdrm_dep,
        gbm_dep,
    ],

    install: true,
    install_dir: join_paths(module_dir, 'drivers'),
)
