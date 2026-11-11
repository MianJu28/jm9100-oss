/*
 * vadec - 最小的 VAAPI 解码客户端（用于隔离 ffmpeg 的影响）
 *
 * 输入是一个 IVF 文件，输出是逐帧的 NV12/P010 原始数据（去掉 pitch），
 * 便于和 libvpx 软件解码的 -pix_fmt nv12/p010 逐字节比较。
 *
 * 用法: vadec <file.ivf> <out.yuv> [--fourcc nv12|p010] [--frames N]
 *
 * 与 ffmpeg 的差别（这正是我们要控制的变量）：
 *  - surface 由我们自己轮转分配，vaSyncSurface 的时机完全确定；
 *  - 单字节 show_existing 帧也会逐包送进驱动，而不是被上层吞掉。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <va/va.h>
#include <va/va_drm.h>
#include <va/va_dec_vp9.h>
#include <va/va_drmcommon.h>
#include <sys/mman.h>

static const char *st2s(VAStatus s) { return vaErrorStr(s); }

#define CHECK(expr, what) do { \
   VAStatus _s = (expr); \
   if (_s != VA_STATUS_SUCCESS) { \
      fprintf(stderr, "vadec: %s 失败: %s\n", what, st2s(_s)); \
      exit(1); \
   } \
} while (0)

struct ivf_frame { unsigned char *data; unsigned len; };

static int read_ivf(const char *path, struct ivf_frame **out)
{
   FILE *fp = fopen(path, "rb");
   unsigned char hdr[32];
   struct ivf_frame *fr = NULL;
   int n = 0, cap = 0;

   if (!fp) { perror(path); exit(1); }
   if (fread(hdr, 1, sizeof(hdr), fp) != sizeof(hdr)) {
      fprintf(stderr, "vadec: %s 不是 IVF（太短）\n", path);
      exit(1);
   }
   if (memcmp(hdr, "DKIF", 4) != 0) {
      fprintf(stderr, "vadec: %s 不是 IVF（缺 DKIF）\n", path);
      exit(1);
   }
   for (;;) {
      unsigned char fh[12];
      unsigned sz;
      struct ivf_frame *f;
      if (fread(fh, 1, sizeof(fh), fp) != sizeof(fh))
         break;
      sz = fh[0] | (fh[1] << 8) | (fh[2] << 16) | ((unsigned)fh[3] << 24);
      if (n == cap) {
         cap = cap ? cap * 2 : 16;
         fr = realloc(fr, cap * sizeof(*fr));
      }
      f = &fr[n++];
      f->data = malloc(sz ? sz : 1);
      f->len = sz;
      if (fread(f->data, 1, sz, fp) != sz) {
         fprintf(stderr, "vadec: %s 截断\n", path);
         exit(1);
      }
   }
   fclose(fp);
   *out = fr;
   return n;
}

static void write_plane(FILE *fp, unsigned char *base, unsigned pitch,
                        unsigned width, unsigned height, unsigned bytes_per_px)
{
   unsigned y;
   for (y = 0; y < height; y++)
      fwrite(base + (size_t)y * pitch, 1, (size_t)width * bytes_per_px, fp);
}


/* 从 VA surface 读回一帧，写出与 ffmpeg -pix_fmt nv12/p010 相同的平面顺序。
 *
 * 本驱动的 pipe_video_buffer 不支持多平面连续映射（Mesa 的 image.c 会因此让
 * vaDeriveImage 直接返回 OPERATION_FAILED），所以走 vaExportSurfaceHandle：
 * 每个平面导出成一个 DRM object，自己 mmap 再按 pitch 逐行拷贝。 */
static int read_surface(VADisplay dpy, VASurfaceID surf, FILE *out,
                        unsigned width, unsigned height, int p010)
{
   VADRMPRIMESurfaceDescriptor desc;
   VAStatus st;
   void *maps[4] = { NULL, NULL, NULL, NULL };
   int i, ret = 0;

   memset(&desc, 0, sizeof(desc));
   st = vaExportSurfaceHandle(dpy, surf,
                              VA_SURFACE_ATTRIB_MEM_TYPE_DRM_PRIME_2,
                              VA_EXPORT_SURFACE_READ_ONLY |
                              VA_EXPORT_SURFACE_SEPARATE_LAYERS,
                              &desc);
   if (st != VA_STATUS_SUCCESS) {
      fprintf(stderr, "vadec: vaExportSurfaceHandle 失败: %s\n", st2s(st));
      return -1;
   }

   fprintf(stderr, "vadec: export fourcc=0x%x %ux%u objects=%u layers=%u\n",
           desc.fourcc, desc.width, desc.height, desc.num_objects, desc.num_layers);
   for (i = 0; i < (int)desc.num_objects; i++)
      fprintf(stderr, "vadec:   obj%d fd=%d size=%u modifier=0x%llx\n", i,
              desc.objects[i].fd, desc.objects[i].size,
              (unsigned long long)desc.objects[i].drm_format_modifier);
   for (i = 0; i < (int)desc.num_layers; i++)
      fprintf(stderr, "vadec:   layer%d fmt=0x%x planes=%u obj=%u off=%u pitch=%u\n",
              i, desc.layers[i].drm_format, desc.layers[i].num_planes,
              desc.layers[i].object_index[0], desc.layers[i].offset[0],
              desc.layers[i].pitch[0]);

   /* 本驱动的 export 不给 size（全 0），自己按 pitch*行数 估一个上界 */
   {
      unsigned need[4] = { 0, 0, 0, 0 };
      for (i = 0; i < (int)desc.num_layers; i++) {
         unsigned obj = desc.layers[i].object_index[0];
         unsigned rows = i == 0 ? height : (height + 1) / 2;
         unsigned sz = desc.layers[i].offset[0] +
                       desc.layers[i].pitch[0] * rows;
         if (need[obj] < sz)
            need[obj] = sz;
      }
      for (i = 0; i < (int)desc.num_objects; i++)
         if (desc.objects[i].size < need[i])
            desc.objects[i].size = need[i];
   }

   for (i = 0; i < (int)desc.num_objects; i++) {
      maps[i] = mmap(NULL, desc.objects[i].size, PROT_READ, MAP_SHARED,
                     desc.objects[i].fd, 0);
      if (maps[i] == MAP_FAILED) {
         perror("mmap");
         maps[i] = NULL;
         ret = -1;
         goto out;
      }
   }

   for (i = 0; i < (int)desc.num_layers; i++) {
      unsigned rowbytes, y;
      unsigned char *base;
      unsigned pitch = desc.layers[i].pitch[0];
      unsigned off = desc.layers[i].offset[0];
      unsigned obj = desc.layers[i].object_index[0];

      switch (desc.layers[i].drm_format) {
      case 0x20203852 /* GR88 */: rowbytes = width; break;
      case 0x20363152 /* R16 */:  rowbytes = width * 2; break;
      case 0x34324752 /* GR32 */: rowbytes = width * 2; break;
      default:                    rowbytes = width; break;
      }
      if (p010 && rowbytes < width * 2)
         rowbytes = width * 2;

      base = (unsigned char *)maps[obj] + off;
      for (y = 0; y < (i == 0 ? height : (height + 1) / 2); y++)
         fwrite(base + (size_t)y * pitch, 1, rowbytes, out);
   }

out:
   for (i = 0; i < (int)desc.num_objects; i++) {
      if (maps[i])
         munmap(maps[i], desc.objects[i].size);
      if (desc.objects[i].fd >= 0)
         close(desc.objects[i].fd);
   }
   return ret;
}

int main(int argc, char **argv)
{
   const char *in, *outp;
   int want_p010 = 0, max_frames = 0;
   struct ivf_frame *frames;
   int nframes, i, s;
   int fd;
   VADisplay dpy;
   VAStatus st;
   int major, minor;
   VAConfigID config;
   VAContextID ctx;
   VASurfaceID surf[8];
   VAConfigAttrib attr;
   VAImage img;
   int nsurf = 8, surf_i = 0;
   unsigned width = 0, height = 0, depth = 8;
   unsigned hdr_w, hdr_h;
   unsigned char *hdrp;
   FILE *out;

   if (argc < 3) {
      fprintf(stderr, "用法: %s <in.ivf> <out.yuv> [--fourcc nv12|p010] [--frames N]\n",
              argv[0]);
      return 1;
   }
   in = argv[1];
   outp = argv[2];
   for (i = 3; i < argc; i++) {
      if (!strcmp(argv[i], "--p010")) want_p010 = 1;
      else if (!strcmp(argv[i], "--frames") && i + 1 < argc) max_frames = atoi(argv[++i]);
      else if (!strcmp(argv[i], "--help")) { fprintf(stderr, "见文件头注释\n"); return 0; }
      else { fprintf(stderr, "vadec: 未知参数 %s\n", argv[i]); return 1; }
   }

   nframes = read_ivf(in, &frames);
   if (!nframes) { fprintf(stderr, "vadec: 没有帧\n"); return 1; }

   /* 从 IVF 头拿宽高：字节 12/14 是 little-endian uint16 */
   {
      FILE *fp = fopen(in, "rb");
      unsigned char h[32];
      fread(h, 1, sizeof(h), fp);
      fclose(fp);
      hdr_w = h[12] | (h[13] << 8);
      hdr_h = h[14] | (h[15] << 8);
   }
   width = hdr_w;
   height = hdr_h;
   if (!width || !height) { fprintf(stderr, "vadec: IVF 头宽高为 0\n"); return 1; }
   if (want_p010) depth = 10;

   fd = open("/dev/dri/renderD128", O_RDWR);
   if (fd < 0) { perror("open renderD128"); return 1; }
   dpy = vaGetDisplayDRM(fd);
   if (!dpy) { fprintf(stderr, "vadec: vaGetDisplayDRM 失败\n"); return 1; }
   CHECK(vaInitialize(dpy, &major, &minor), "vaInitialize");
   fprintf(stderr, "vadec: VA %d.%d, %s\n", major, minor, vaQueryVendorString(dpy));

   {
      VAProfile profile = want_p010 ? VAProfileVP9Profile2 : VAProfileVP9Profile0;
      VAEntrypoint eps[8];
      int neps = 8, k, found = 0;
      st = vaQueryConfigEntrypoints(dpy, profile, eps, &neps);
      if (st == VA_STATUS_SUCCESS)
         for (k = 0; k < neps; k++)
            if (eps[k] == VAEntrypointVLD) found = 1;
      if (!found) {
         fprintf(stderr, "vadec: 驱动不支持 %s 的 VLD\n",
                 want_p010 ? "VP9Profile2" : "VP9Profile0");
         return 1;
      }
   }

   attr.type = VAConfigAttribRTFormat;
   attr.value = want_p010 ? VA_RT_FORMAT_YUV420_10 : VA_RT_FORMAT_YUV420;
   CHECK(vaCreateConfig(dpy, want_p010 ? VAProfileVP9Profile2 : VAProfileVP9Profile0,
                        VAEntrypointVLD, &attr, 1, &config), "vaCreateConfig");
   CHECK(vaCreateSurfaces(dpy, want_p010 ? VA_RT_FORMAT_YUV420_10 : VA_RT_FORMAT_YUV420,
                          width, height, surf, nsurf, NULL, 0), "vaCreateSurfaces");
   CHECK(vaCreateContext(dpy, config, (int)width, (int)height, VA_PROGRESSIVE,
                         NULL, 0, &ctx), "vaCreateContext");
   fprintf(stderr, "vadec: config=%u ctx=%u\n", (unsigned)config, (unsigned)ctx);

   out = fopen(outp, "wb");
   if (!out) { perror(outp); return 1; }

   for (i = 0; i < nframes; i++) {
      VABufferID buf, ppbuf;
      VADecPictureParameterBufferVP9 pp;
      void *p;

      if (max_frames && i >= max_frames)
         break;
      if (!frames[i].len) continue;

      CHECK(vaBeginPicture(dpy, ctx, surf[surf_i]), "vaBeginPicture");

      /* Mesa 的 va 前端在收到 VAPictureParameterBufferType（VP9）时才创建
       * 解码器实例，因此即使解的是纯 intra 帧也必须先送这个缓冲。 */
      memset(&pp, 0, sizeof(pp));
      pp.frame_width = (uint16_t)width;
      pp.frame_height = (uint16_t)height;
      for (s = 0; s < 8; s++)
         pp.reference_frames[s] = VA_INVALID_SURFACE;
      /* Mesa 前端会用这个长度去解析未压缩头，必须给一个非零上界 */
      pp.frame_header_length_in_bytes =
         (uint8_t)(frames[i].len > 255 ? 255 : frames[i].len);
      pp.pic_fields.bits.show_frame = 1;
      pp.pic_fields.bits.frame_type = 0;
      pp.pic_fields.bits.intra_only = 1;
      CHECK(vaCreateBuffer(dpy, ctx, VAPictureParameterBufferType,
                           sizeof(pp), 1, &pp, &ppbuf), "vaCreateBuffer(pp)");

      CHECK(vaCreateBuffer(dpy, ctx, VASliceDataBufferType,
                           frames[i].len, 1, NULL, &buf), "vaCreateBuffer");
      CHECK(vaMapBuffer(dpy, buf, &p), "vaMapBuffer");
      memcpy(p, frames[i].data, frames[i].len);
      CHECK(vaUnmapBuffer(dpy, buf), "vaUnmapBuffer");
      {
         VABufferID bufs[2] = { ppbuf, buf };
         CHECK(vaRenderPicture(dpy, ctx, bufs, 2), "vaRenderPicture");
      }
      CHECK(vaEndPicture(dpy, ctx), "vaEndPicture");

      st = vaSyncSurface(dpy, surf[surf_i]);
      if (st != VA_STATUS_SUCCESS) {
         fprintf(stderr, "vadec: frame %d vaSyncSurface: %s（0x%x）\n",
                 i, st2s(st), st);
         vaDestroyBuffer(dpy, buf);
         fclose(out);
         return 2;
      }

      if (read_surface(dpy, surf[surf_i], out, width, height, want_p010) < 0) {
         vaDestroyBuffer(dpy, buf);
         vaDestroyBuffer(dpy, ppbuf);
         fclose(out);
         return 3;
      }

      CHECK(vaDestroyBuffer(dpy, buf), "vaDestroyBuffer");
      CHECK(vaDestroyBuffer(dpy, ppbuf), "vaDestroyBuffer(pp)");
      surf_i = (surf_i + 1) % nsurf;
   }

   fprintf(stderr, "vadec: 解码 %d 帧，宽高 %ux%u，%s\n",
           i, width, height, want_p010 ? "P010" : "NV12");
   fclose(out);
   vaDestroyContext(dpy, ctx);
   vaDestroySurfaces(dpy, surf, nsurf);
   vaDestroyConfig(dpy, config);
   vaTerminate(dpy);
   (void)hdrp;
   return 0;
}
