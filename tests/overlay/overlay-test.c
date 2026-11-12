/* overlay-test: 用 DRM 直接把一块缓冲放到 overlay 平面上，验证 WIN0 硬件窗口。
 *
 * 需要 DRM master（X/Wayland 未占用时，或在 VT 上停掉显示管理器后运行）：
 *   sudo systemctl stop lightdm
 *   sudo ./overlay-test /dev/dri/card0            # 全屏纯色 overlay，2 秒后自灭
 *   sudo ./overlay-test /dev/dri/card0 --keep     # 一直留着，Ctrl-C 退出
 *
 * 判据：屏幕上应出现一块（半透明或纯色）矩形，位置/大小由参数控制；
 *       纯色为 ARGB8888 (A=0xff)，半透明用 --alpha N。
 *
 * 编译：cc -O2 -o /tmp/overlay-test tests/overlay/overlay-test.c -I/usr/include/libdrm -ldrm
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>

static int drm_fd;
static unsigned int fb_id, bo_handle;
static uint32_t *map;
static uint32_t pitch;

static int make_buffer(int w, int h, uint32_t color)
{
	struct drm_mode_create_dumb create = { 0 };
	struct drm_mode_map_dumb map_req = { 0 };
	int ret;

	create.width = w;
	create.height = h;
	create.bpp = 32;
	ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create);
	if (ret < 0) {
		perror("CREATE_DUMB");
		return -1;
	}
	bo_handle = create.handle;
	pitch = create.pitch;

	ret = drmModeAddFB(drm_fd, w, h, 24, 32, pitch, bo_handle, &fb_id);
	if (ret < 0) {
		perror("AddFB");
		return -1;
	}

	map_req.handle = bo_handle;
	ret = drmIoctl(drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req);
	if (ret < 0) {
		perror("MAP_DUMB");
		return -1;
	}
	map = mmap(NULL, pitch * h, PROT_READ | PROT_WRITE, MAP_SHARED,
		   drm_fd, map_req.offset);
	if (map == MAP_FAILED) {
		perror("mmap");
		return -1;
	}
	for (uint32_t i = 0; i < pitch * (uint32_t)h / 4; i++)
		map[i] = color;
	return 0;
}

int main(int argc, char **argv)
{
	const char *dev = argc > 1 ? argv[1] : "/dev/dri/card0";
	int keep = 0, w = 0, h = 0, x = 0, y = 0, alpha = 0xff, i;
	drmModeRes *res;
	drmModePlaneRes *pres;
	drmModePlane *plane = NULL;
	drmModeCrtc *crtc;
	uint32_t color;

	for (i = 2; i < argc; i++) {
		if (!strcmp(argv[i], "--keep"))
			keep = 1;
		else if (!strcmp(argv[i], "--alpha") && i + 1 < argc)
			alpha = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--size") && i + 2 < argc) {
			w = atoi(argv[++i]);
			h = atoi(argv[++i]);
		} else if (!strcmp(argv[i], "--pos") && i + 2 < argc) {
			x = atoi(argv[++i]);
			y = atoi(argv[++i]);
		}
	}

	drm_fd = open(dev, O_RDWR);
	if (drm_fd < 0) {
		perror(dev);
		return 1;
	}
	if (drmSetMaster(drm_fd) != 0 && errno == EBUSY) {
		fprintf(stderr, "设备被占用（X/Wayland 正在用），请先停掉显示管理器\n");
		return 2;
	}

	res = drmModeGetResources(drm_fd);
	if (!res) {
		perror("GetResources");
		return 1;
	}
	crtc = drmModeGetCrtc(drm_fd, res->crtcs[0]);
	if (!crtc) {
		perror("GetCrtc");
		return 1;
	}
	if (!w || !h) {
		w = crtc->width / 2;
		h = crtc->height / 2;
		x = crtc->width / 4;
		y = crtc->height / 4;
	}

	drmSetClientCap(drm_fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
	pres = drmModeGetPlaneResources(drm_fd);
	if (!pres) {
		perror("GetPlaneResources");
		return 1;
	}
	printf("共 %u 个平面\n", pres->count_planes);
	for (i = 0; i < (int)pres->count_planes; i++) {
		drmModePlane *p = drmModeGetPlane(drm_fd, pres->planes[i]);
		if (!p)
			continue;
		if (p->plane_id)
			printf("  plane %u: crtc=%08x fb=%u\n", p->plane_id,
			       p->crtc_id, p->fb_id);
		if (!plane && p->crtc_id == 0) {
			drmModeObjectProperties *props =
				drmModeObjectGetProperties(drm_fd, p->plane_id,
							   DRM_MODE_OBJECT_PLANE);
			for (uint32_t k = 0; props && k < props->count_props; k++) {
				drmModePropertyRes *pr =
					drmModeGetProperty(drm_fd, props->props[k]);
				if (pr && !strcmp(pr->name, "type") &&
				    props->prop_values[k] == DRM_PLANE_TYPE_OVERLAY)
					plane = p;
				if (pr)
					drmModeFreeProperty(pr);
			}
			if (props)
				drmModeFreeObjectProperties(props);
		}
	}
	if (!plane) {
		fprintf(stderr, "没找到 overlay 平面（KMD 需带 overlay=1 加载）\n");
		return 3;
	}
	printf("用 overlay plane %u，%dx%d @ (%d,%d)，alpha=0x%02x\n",
	       plane->plane_id, w, h, x, y, alpha);

	color = (alpha << 24) | 0x00ff00ff;	/* 品红，alpha 可选 */
	if (make_buffer(w, h, color))
		return 1;

	if (drmModeSetPlane(drm_fd, plane->plane_id, res->crtcs[0], fb_id, 0,
			    x, y, w, h, 0, 0, w << 16, h << 16) < 0) {
		perror("SetPlane");
		return 4;
	}
	printf("已上屏。%s", keep ? "Ctrl-C 退出\n" : "2 秒后关闭\n");
	if (!keep)
		sleep(2);
	else
		for (;;)
			sleep(1);

	drmModeSetPlane(drm_fd, plane->plane_id, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	drmDropMaster(drm_fd);
	return 0;
}
