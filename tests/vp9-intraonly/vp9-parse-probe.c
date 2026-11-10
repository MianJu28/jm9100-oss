/* 用驱动自带的 VP9 切片头解析器读出关键字段，用于构造 intra-only 样片。
 *
 * 编译见 build-probe.sh。用法：vp9-parse-probe <packet.bin>
 * 输出一行 key=value，其中 tag 是未压缩头 + 压缩头占用的字节数
 * （slice_header->frame_tag_size），parts 是 first_partition_size。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mwv207_decoder_vp9_slice_header.h"

static uint8_t *
read_file(const char *path, size_t *size)
{
   FILE *f = fopen(path, "rb");
   if (!f) {
      perror(path);
      exit(1);
   }
   fseek(f, 0, SEEK_END);
   long len = ftell(f);
   fseek(f, 0, SEEK_SET);
   uint8_t *buf = malloc(len ? len : 1);
   if (fread(buf, 1, len, f) != (size_t)len) {
      fprintf(stderr, "%s: 读取失败\n", path);
      exit(1);
   }
   fclose(f);
   *size = len;
   return buf;
}

int
main(int argc, char **argv)
{
   if (argc < 2) {
      fprintf(stderr, "用法: %s <packet.bin> [fallback_width fallback_height]\n",
              argv[0]);
      return 2;
   }

   size_t size;
   uint8_t *data = read_file(argv[1], &size);

   struct mwv207_vp9_slice_header sh;
   memset(&sh, 0, sizeof(sh));
   uint32_t fbw = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 0) : 0;
   uint32_t fbh = argc > 3 ? (uint32_t)strtoul(argv[3], NULL, 0) : 0;
   uint32_t used = mwv207_vp9_parse_frame(&sh, data, (uint32_t)size,
                                          fbw, fbh, NULL, 0, 0);

   printf("used=%u tag=%u parts=%u key=%u intra=%u show=%u showex=%u err=%u "
          "prof=%u w=%u h=%u refresh=0x%02x txmode=%u fpar=%u lf=%u qyac=%u\n",
          used, sh.frame_tag_size, sh.offset_to_dct_parts, sh.key_frame,
          sh.intra_only, sh.show_frame, sh.show_existing_frame,
          sh.error_resilient, sh.vp_profile, sh.width, sh.height,
          sh.refresh_frame_flags, sh.transform_mode,
          sh.frame_parallel_decoding, sh.loop_filter_level, sh.qp_yac);
   fprintf(stderr, "active_ref_idx=%u,%u,%u ref_map=%d,%d,%d sign_bias=%u,%u,%u\n",
          sh.active_ref_idx[0], sh.active_ref_idx[1], sh.active_ref_idx[2],
          sh.ref_frame_map[0], sh.ref_frame_map[1], sh.ref_frame_map[2],
          sh.ref_frame_sign_bias[1], sh.ref_frame_sign_bias[2],
          sh.ref_frame_sign_bias[3]);

   free(data);
   return used ? 1 : 0;
}