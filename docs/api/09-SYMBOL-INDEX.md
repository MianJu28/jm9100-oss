# 09 · 驱动符号索引

本文件由 `docs/tools/generate_symbol_index.py` 自动生成，便于按符号名检索。

- 函数定义：**666** 个
- 结构体/联合体/枚举定义：**194** 个
- 函数宏：**182** 个

各符号的语义请查对应模块文档（见 [../README.md](../README.md) 索引）；
本索引只提供"符号在哪"与签名。

## 函数定义（按文件）

### `drivers/mwv207/include/mwv207_cmd.h`  (2 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 138 | `static inline void` | **mwv207_wait_blit** | `mwv207_wait_blit(uint32_t **cmd_buf, uint32_t cluster_mask)` |
| 161 | `static inline void` | **mwv207_excute_blit** | `mwv207_excute_blit(uint32_t **cmd_buf, uint32_t op)` |

### `drivers/mwv207/include/mwv207_context.h`  (1 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 171 | `static inline struct mwv207_context *` | **mwv207_context** | `mwv207_context(struct pipe_context *context)` |

### `drivers/mwv207/include/mwv207_resource.h`  (1 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 127 | `static inline struct mwv207_resource *` | **mwv207_resource** | `mwv207_resource(struct pipe_resource *r)` |

### `drivers/mwv207/include/mwv207_screen.h`  (1 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 48 | `static inline struct mwv207_screen *` | **mwv207_screen** | `mwv207_screen(struct pipe_screen *pipe)` |

### `drivers/mwv207/include/mwv207_shader.h`  (2 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 60 | `static inline struct mwv207_shader_state *` | **mwv207_shader_state** | `mwv207_shader_state(void *state)` |
| 66 | `static inline void` | **mwv207_config_arrays** | `mwv207_config_arrays(uint32_t *arrays, uint32_t bit_size, uint32_t index, uint32_t value)` |

### `drivers/mwv207/include/mwv207_state.h`  (1 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 119 | `static inline struct mwv207_stream_output_target *` | **mwv207_stream_output_target** | `mwv207_stream_output_target(struct pipe_stream_output_target *r)` |

### `drivers/mwv207/include/mwv207_surface.h`  (2 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 99 | `static inline struct mwv207_sampler_view *` | **mwv207_sampler_view** | `mwv207_sampler_view(struct pipe_sampler_view *pview)` |
| 105 | `static inline struct mwv207_surface *` | **mwv207_surface** | `mwv207_surface(struct pipe_surface *pipe)` |

### `drivers/mwv207/mwv207_blend.c`  (5 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 136 | `static void *` | **mwv207_create_blend_state** | `mwv207_create_blend_state(struct pipe_context *pctx, const struct pipe_blend_state *state)` |
| 322 | `static void` | **mwv207_delete_blend_state** | `mwv207_delete_blend_state(struct pipe_context *pctx, void *blend_state)` |
| 328 | `static void` | **mwv207_bind_blend_state** | `mwv207_bind_blend_state(struct pipe_context *pctx, void *state)` |
| 352 | `static void` | **mwv207_set_blend_color** | `mwv207_set_blend_color(struct pipe_context *pctx, const struct pipe_blend_color *color)` |
| 363 | `void` | **mwv207_context_blend_init** | `mwv207_context_blend_init(struct pipe_context *pctx)` |

### `drivers/mwv207/mwv207_blit.c`  (24 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 37 | `static bool` | **mwv207_can_blit** | `mwv207_can_blit(const struct pipe_blit_info *info)` |
| 90 | `static void` | **mwv207_set_image_task** | `mwv207_set_image_task(struct mwv207_context *ctx, uint32_t **commandBuffer, struct mwv207_task_info info)` |
| 157 | `static void` | **mwv207_set_buffer_task** | `mwv207_set_buffer_task(struct mwv207_context *ctx, uint32_t **commandBuffer, struct mwv207_region_info info)` |
| 196 | `enum pipe_error` | **mwv207_fill_buffer** | `mwv207_fill_buffer(struct mwv207_context *ctx, struct mwv207_bo *mem, uint32_t offset, uint32_t size, uint32_t data)` |
| 307 | `enum pipe_error` | **mwv207_copy_buffer** | `mwv207_copy_buffer(struct mwv207_context *ctx, struct mwv207_region_info info)` |
| 330 | `enum pipe_error` | **mwv207_update_buffer** | `mwv207_update_buffer(struct mwv207_context *ctx, struct mwv207_region_info info)` |
| 353 | `static enum pipe_error` | **mwv207_clear_image** | `mwv207_clear_image(struct mwv207_context *ctx, struct mwv207_clear_info info)` |
| 447 | `static enum pipe_error` | **mwv207_copy_layer** | `mwv207_copy_layer(struct mwv207_context *ctx, struct mwv207_region_info info)` |
| 606 | `static enum pipe_error` | **mwv207_mipmap** | `mwv207_mipmap(struct mwv207_context *ctx, struct mwv207_region_info info)` |
| 698 | `enum pipe_error` | **mwv207_copy_image** | `mwv207_copy_image(struct mwv207_context *ctx, struct mwv207_region_info info)` |
| 710 | `static enum pipe_error` | **mwv207_blit_self_image** | `mwv207_blit_self_image(struct mwv207_context *ctx, struct mwv207_resource *res)` |
| 751 | `static uint32_t` | **mwv207_get_filler_bpp** | `mwv207_get_filler_bpp(enum pipe_format format)` |
| 771 | `static void` | **mwv207_till_fill** | `mwv207_till_fill(struct mwv207_context *ctx, struct mwv207_resource *src)` |
| 839 | `void` | **mwv207_disable_tile** | `mwv207_disable_tile(struct mwv207_context *ctx, struct mwv207_resource *res)` |
| 854 | `static void` | **mwv207_fix_box** | `mwv207_fix_box(struct pipe_box *box)` |
| 863 | `static bool` | **mwv207_blit_image** | `mwv207_blit_image(struct mwv207_context *ctx, const struct pipe_blit_info *info)` |
| 914 | `static bool` | **mwv207_try_blit128_or_blit96_copy_region** | `mwv207_try_blit128_or_blit96_copy_region(struct pipe_context *ctx, const struct pipe_blit_info *blit)` |
| 932 | `static void` | **mwv207_blit** | `mwv207_blit(struct pipe_context *pctx, const struct pipe_blit_info *info)` |
| 978 | `static void` | **mwv207_clear_render_target** | `mwv207_clear_render_target(struct pipe_context *pctx, struct pipe_surface *dst, const union pipe_color_union *color, unsigned dstx, unsigned dsty, unsigned width, unsigned height, bool render_condition_enabled)` |
| 1015 | `static void` | **mwv207_clear_depth_stencil** | `mwv207_clear_depth_stencil(struct pipe_context *pctx, struct pipe_surface *dst, unsigned clear_flags, double depth, unsigned stencil, unsigned dstx, unsigned dsty, unsigned width, unsigned height, bool render_condition_enabled)` |
| 1047 | `static void` | **mwv207_clear** | `mwv207_clear(struct pipe_context *pctx, unsigned buffers, const struct pipe_scissor_state *scissor_state, const union pipe_color_union *pcolor, double depth, unsigned stencil)` |
| 1096 | `static bool` | **mwv207_gen_mipmap** | `mwv207_gen_mipmap(struct pipe_context *pctx, struct pipe_resource *resource, enum pipe_format format, uint base_level, uint last_level, uint first_layer, uint last_layer)` |
| 1143 | `void` | **mwv207_blit_resource_box** | `mwv207_blit_resource_box(struct pipe_context *pctx, struct pipe_resource *dst, struct pipe_resource *src, int level, const struct pipe_box *box)` |
| 1164 | `static void` | **mwv207_resource_copy_region** | `mwv207_resource_copy_region(struct pipe_context *pctx, struct pipe_resource *pdst, unsigned dst_level, unsigned dstx, unsigned dsty, unsigned dstz, struct pipe_resource *psrc, unsigned src_level, const struct pipe_box *src_box)` |

### `drivers/mwv207/mwv207_cmd.c`  (14 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 32 | `static void` | **mwv207_start_cmd** | `mwv207_start_cmd(struct mwv207_context *ctx)` |
| 61 | `static void` | **mwv207_end_cmd** | `mwv207_end_cmd(struct mwv207_context *ctx)` |
| 75 | `void` | **mwv207_pipeline_barrier** | `mwv207_pipeline_barrier(struct mwv207_context *ctx)` |
| 98 | `enum pipe_error` | **mwv207_flush_cache** | `mwv207_flush_cache(uint32_t **pbuf, enum cache_mask mask)` |
| 178 | `void` | **mwv207_wait_idle** | `mwv207_wait_idle(uint32_t **pbuf, uint32_t target)` |
| 188 | `enum pipe_error` | **mwv207_dat_add_reloc** | `mwv207_dat_add_reloc(struct cmd_dat *dat, uint32_t **ptr, struct mwv207_bo *bo, uint32_t offset, uint32_t flags)` |
| 214 | `enum pipe_error` | **mwv207_add_relocs** | `mwv207_add_relocs(struct mwv207_context *ctx, uint32_t **ptr, uint32_t reg, struct mwv207_bo *bo, uint32_t offset, uint32_t flags)` |
| 240 | `enum pipe_error` | **mwv207_add_dat** | `mwv207_add_dat(struct mwv207_context *ctx, uint32_t **ptr, uint32_t reg, struct cmd_dat *dat)` |
| 258 | `enum pipe_error` | **mwv207_dma** | `mwv207_dma(struct mwv207_context *ctx, struct pipe_resource *psrc, struct pipe_resource *pdst, unsigned level, const struct pipe_box *box)` |
| 314 | `void` | **mwv207_create_cmd** | `mwv207_create_cmd(struct mwv207_context *ctx)` |
| 326 | `void` | **mwv207_begin_record** | `mwv207_begin_record(struct mwv207_cmd *cmd, uint32_t **start)` |
| 334 | `void` | **mwv207_end_record** | `mwv207_end_record(struct mwv207_cmd *cmd, uint32_t **end)` |
| 341 | `static bool` | **check_cmd** | `check_cmd(struct mwv207_cmd *cmd)` |
| 355 | `void` | **mwv207_submit** | `mwv207_submit(struct mwv207_context *ctx, int *out_fence_fd, bool force)` |

### `drivers/mwv207/mwv207_compute.c`  (8 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 37 | `static void` | **mwv207_set_hw_atomic_buffers** | `mwv207_set_hw_atomic_buffers(struct pipe_context *pctx, unsigned start_slot, unsigned count, const struct pipe_shader_buffer *buffers)` |
| 48 | `static void` | **mwv207_set_shader_buffers** | `mwv207_set_shader_buffers(struct pipe_context *pctx, enum pipe_shader_type shader, unsigned start_slot, unsigned count, const struct pipe_shader_buffer *buffers, unsigned writable_bitmask)` |
| 61 | `static void` | **mwv207_set_shader_images** | `mwv207_set_shader_images(struct pipe_context *pctx, enum pipe_shader_type shader, unsigned start_slot, unsigned count, unsigned unbind_num_trailing_slots, const struct pipe_image_view *images)` |
| 78 | `static void` | **mwv207_set_compute_resources** | `mwv207_set_compute_resources(struct pipe_context *pctx, unsigned start, unsigned count, struct pipe_surface **resources)` |
| 86 | `static void` | **mwv207_set_global_binding** | `mwv207_set_global_binding(struct pipe_context *pctx, unsigned first, unsigned count, struct pipe_resource **resources, uint32_t **handles)` |
| 125 | `static void` | **mwv207_launch_grid** | `mwv207_launch_grid(struct pipe_context *pctx, const struct pipe_grid_info *info)` |
| 293 | `static void` | **mwv207_create_local_memory_buffer** | `mwv207_create_local_memory_buffer(struct mwv207_context *ctx)` |
| 310 | `void` | **mwv207_context_compute_init** | `mwv207_context_compute_init(struct pipe_context *pctx)` |

### `drivers/mwv207/mwv207_context.c`  (7 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 44 | `static bool` | **mwv207_context_helper_init** | `mwv207_context_helper_init(struct mwv207_context *ctx)` |
| 68 | `static void` | **mwv207_flush** | `mwv207_flush(struct pipe_context *pctx, struct pipe_fence_handle **pfence, enum pipe_flush_flags flags)` |
| 98 | `static void` | **mwv207_memory_barrier** | `mwv207_memory_barrier(struct pipe_context *pctx, uint32_t flags)` |
| 106 | `static void` | **mwv207_context_destroy** | `mwv207_context_destroy(struct pipe_context *pctx)` |
| 154 | `static void` | **mwv207_create_fence_fd** | `mwv207_create_fence_fd(struct pipe_context *pctx, struct pipe_fence_handle **pfence, int fd, enum pipe_fd_type type)` |
| 167 | `static void` | **mwv207_fence_server_sync** | `mwv207_fence_server_sync(struct pipe_context *pctx, struct pipe_fence_handle *fence)` |
| 175 | `struct pipe_context *` | **mwv207_context_create** | `mwv207_context_create(struct pipe_screen *pscreen, void *priv, unsigned flags)` |

### `drivers/mwv207/mwv207_draw.c`  (17 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 76 | `static void` | **mwv207_config_vertex** | `mwv207_config_vertex(struct mwv207_context *ctx, const struct pipe_draw_info *dinfo, const struct pipe_draw_start_count_bias *draws)` |
| 184 | `static void` | **mwv207_config_viewport** | `mwv207_config_viewport(struct mwv207_context *ctx)` |
| 209 | `static void` | **mwv207_config_scissor** | `mwv207_config_scissor(struct mwv207_context *ctx)` |
| 253 | `static void` | **mwv207_config_framebuffer** | `mwv207_config_framebuffer(struct mwv207_context *ctx)` |
| 500 | `static void` | **mwv207_config_rasterizer** | `mwv207_config_rasterizer(struct mwv207_context *ctx)` |
| 544 | `static void` | **mwv207_config_multisamples** | `mwv207_config_multisamples(struct mwv207_context *ctx)` |
| 615 | `static void` | **mwv207_config_sample_mask** | `mwv207_config_sample_mask(struct mwv207_context *ctx)` |
| 639 | `static void` | **mwv207_config_zsa** | `mwv207_config_zsa(struct mwv207_context *ctx)` |
| 666 | `static void` | **mwv207_config_blend** | `mwv207_config_blend(struct mwv207_context *ctx)` |
| 734 | `static void` | **mwv207_config_blendColor** | `mwv207_config_blendColor(struct mwv207_context *ctx)` |
| 785 | `static void` | **mwv207_config_stream_out** | `mwv207_config_stream_out(struct mwv207_context *ctx, bool suspend)` |
| 881 | `static void` | **mwv207_config_draw** | `mwv207_config_draw(struct mwv207_context *ctx, const struct pipe_draw_info *dinfo, unsigned drawid_offset, const struct pipe_draw_indirect_info *dindirect, const struct pipe_draw_start_count_bias *draws, unsigned num_draws)` |
| 941 | `static void` | **mwv207_config_other** | `mwv207_config_other(struct mwv207_context *ctx)` |
| 1043 | `static bool` | **mwv207_update_shader** | `mwv207_update_shader(struct mwv207_context *ctx)` |
| 1115 | `static void` | **mwv207_config_dual_blend** | `mwv207_config_dual_blend(struct pipe_context *pctx, const struct pipe_draw_info *dinfo)` |
| 1174 | `static void` | **mwv207_config_render_piepline** | `mwv207_config_render_piepline(struct mwv207_context *ctx, const struct pipe_draw_info *dinfo, unsigned drawid_offset, const struct pipe_draw_indirect_info *dindirect, const struct pipe_draw_start_count_bias *draws, unsigned num_draws)` |
| 1261 | `void` | **mwv207_draw_vbo** | `mwv207_draw_vbo(struct pipe_context *pctx, const struct pipe_draw_info *dinfo, unsigned drawid_offset, const struct pipe_draw_indirect_info *indirect, const struct pipe_draw_start_count_bias *draws, unsigned num_draws)` |

### `drivers/mwv207/mwv207_format.c`  (13 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 34 | `static uint32_t` | **mwv207_get_endian** | `mwv207_get_endian(enum pipe_format format)` |
| 180 | `enum pipe_error` | **mwv207_get_texture_desc** | `mwv207_get_texture_desc(enum pipe_format format, struct mwv207_texture_desc *texture_desc)` |
| 324 | `enum pipe_error` | **mwv207_get_pixel_desc** | `mwv207_get_pixel_desc(enum pipe_format format, struct mwv207_pixel_desc *pixel_desc)` |
| 471 | `enum pipe_error` | **mwv207_get_blit_desc** | `mwv207_get_blit_desc(enum pipe_format format, struct mwv207_blit_desc *blt_desc)` |
| 535 | `enum pipe_error` | **mwv207_get_element_desc** | `mwv207_get_element_desc(struct mwv207_element *element)` |
| 652 | `uint32_t` | **mwv207_get_format_desc** | `mwv207_get_format_desc(enum pipe_format format)` |
| 685 | `static void` | **mwv207_get_tile_mode** | `mwv207_get_tile_mode( struct mwv207_resource *res, uint32_t *tiling, uint32_t *super_tile )` |
| 710 | `enum pipe_error` | **mwv207_get_image_info** | `mwv207_get_image_info(struct mwv207_resource *res)` |
| 791 | `enum pipe_error` | **mwv207_get_clearval** | `mwv207_get_clearval(uint32_t format, struct mwv207_clear_info info, uint32_t *clear_val, uint32_t *clear_mask)` |
| 875 | `uint32_t` | **mwv207_get_compressed_format** | `mwv207_get_compressed_format(enum pipe_format format)` |
| 901 | `uint32_t` | **mwv207_update_colormask** | `mwv207_update_colormask(struct mwv207_context *ctx, int num)` |
| 921 | `bool` | **mwv207_get_sample_info** | `mwv207_get_sample_info(int num_samples, uint8_t *xscale_out, uint8_t *yscale_out, uint8_t *nr_samples_out)` |
| 956 | `uint32_t` | **mwv207_get_blit_msaa_mode** | `mwv207_get_blit_msaa_mode(uint8_t x_scale, uint8_t y_scale)` |

### `drivers/mwv207/mwv207_query.c`  (10 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 37 | `static struct pipe_query *` | **mwv207_create_query** | `mwv207_create_query(struct pipe_context *pctx, unsigned query_type, unsigned index)` |
| 61 | `static void` | **mwv207_destroy_query** | `mwv207_destroy_query(struct pipe_context *pctx, struct pipe_query *q)` |
| 72 | `static bool` | **mwv207_begin_query** | `mwv207_begin_query(struct pipe_context *pctx, struct pipe_query *q)` |
| 177 | `static bool` | **mwv207_end_query** | `mwv207_end_query(struct pipe_context *pctx, struct pipe_query *q)` |
| 236 | `static bool` | **mwv207_get_query_result** | `mwv207_get_query_result(struct pipe_context *pctx, struct pipe_query *q, bool wait, union pipe_query_result *result)` |
| 292 | `static void` | **mwv207_suspend_queries** | `mwv207_suspend_queries(struct pipe_context *pctx)` |
| 301 | `static void` | **mwv207_resume_queries** | `mwv207_resume_queries(struct pipe_context *pctx)` |
| 310 | `static void` | **mwv207_set_active_query_state** | `mwv207_set_active_query_state(struct pipe_context *pctx, bool enable)` |
| 325 | `void` | **mwv207_context_query_init** | `mwv207_context_query_init(struct pipe_context *pctx)` |
| 343 | `void` | **mwv207_context_query_destory** | `mwv207_context_query_destory(struct pipe_context *pctx)` |

### `drivers/mwv207/mwv207_resource.c`  (18 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 70 | `static enum pipe_error` | **mwv207_can_tile** | `mwv207_can_tile(struct mwv207_resource *res)` |
| 95 | `static enum pipe_error` | **mwv207_get_tile_info** | `mwv207_get_tile_info(struct mwv207_screen *screen, struct mwv207_resource *res, uint32_t size)` |
| 132 | `static bool` | **mwv207_screen_can_create_resource** | `mwv207_screen_can_create_resource(struct pipe_screen *pscreen, const struct pipe_resource *templ)` |
| 142 | `static void` | **mwv207_set_miptree** | `mwv207_set_miptree(struct mwv207_screen *screen, struct mwv207_resource *res, struct winsys_handle *whandle)` |
| 237 | `static void` | **mwv207_resource_destroy** | `mwv207_resource_destroy(struct pipe_screen *pscreen, struct pipe_resource *pres)` |
| 264 | `static struct pipe_resource *` | **resource_create** | `resource_create(struct pipe_screen *pscreen, const struct pipe_resource *templ, struct winsys_handle *whandle, unsigned external_usage)` |
| 309 | `static struct pipe_resource *` | **mwv207_resource_create** | `mwv207_resource_create(struct pipe_screen *pscreen, const struct pipe_resource *templ)` |
| 315 | `static struct pipe_resource *` | **mwv207_resource_from_handle** | `mwv207_resource_from_handle(struct pipe_screen *pscreen, const struct pipe_resource *templ, struct winsys_handle *whandle, unsigned usage)` |
| 324 | `static bool` | **mwv207_resource_get_handle** | `mwv207_resource_get_handle(struct pipe_screen *pscreen, struct pipe_context *context, struct pipe_resource *tex, struct winsys_handle *whandle, unsigned usage)` |
| 356 | `static void` | **mwv207_flush_resource** | `mwv207_flush_resource(struct pipe_context *pctx, struct pipe_resource *resource)` |
| 399 | `void` | **mwv207_wait_resource** | `mwv207_wait_resource(struct pipe_context *pctx, struct pipe_resource *resource)` |
| 409 | `static void *` | **mwv207_transfer_map** | `mwv207_transfer_map(struct pipe_context *pctx, struct pipe_resource *pres, unsigned level, unsigned usage, const struct pipe_box *box, struct pipe_transfer **transfer)` |
| 524 | `static void` | **mwv207_transfer_unmap** | `mwv207_transfer_unmap(struct pipe_context *pctx, struct pipe_transfer *ptrans)` |
| 558 | `static void` | **mwv207_flush_region** | `mwv207_flush_region(struct pipe_context *pctx, struct pipe_transfer *ptrans, const struct pipe_box *box)` |
| 572 | `void` | **mwv207_context_resource_init** | `mwv207_context_resource_init(struct pipe_context *pctx)` |
| 588 | `static void` | **mwv207_screen_query_dmabuf_modifiers** | `mwv207_screen_query_dmabuf_modifiers(struct pipe_screen *pscreen, enum pipe_format format, int max, uint64_t *modifiers, unsigned int *external_only, int *count)` |
| 620 | `static struct pipe_resource *` | **mwv207_resource_create_modifiers** | `mwv207_resource_create_modifiers(struct pipe_screen *pscreen, const struct pipe_resource *templat, const uint64_t *modifiers, int count)` |
| 649 | `void` | **mwv207_screen_resource_init** | `mwv207_screen_resource_init(struct pipe_screen *pscreen)` |

### `drivers/mwv207/mwv207_sampler.c`  (22 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 95 | `static uint32_t` | **mwv207_log2** | `mwv207_log2(uint32_t val)` |
| 130 | `static int32_t` | **mwv207_float_to_fixed** | `mwv207_float_to_fixed(float x, int8_t trunc_bits, int8_t frac_bits)` |
| 162 | `static void` | **mwv207_print_samper** | `mwv207_print_samper(struct mwv207_sampler_view *sampler_view, uint32_t start_slot)` |
| 193 | `static bool` | **mwv207_is_128bit_sampler** | `mwv207_is_128bit_sampler(struct mwv207_resource *res, struct mwv207_sampler_view *sampler_view)` |
| 205 | `enum pipe_format` | **mwv207_transfer_128bit** | `mwv207_transfer_128bit(enum pipe_format input)` |
| 218 | `void` | **mwv207_split_128bit** | `mwv207_split_128bit(struct pipe_context *pctx, struct pipe_resource *pres, enum pipe_format format)` |
| 285 | `void` | **mwv207_merge_128bit** | `mwv207_merge_128bit(struct pipe_context *pctx, struct pipe_resource *pres)` |
| 350 | `static uint32_t` | **mwv207_transfer_border** | `mwv207_transfer_border(uint32_t swizzle, uint32_t current)` |
| 367 | `static union pipe_color_union` | **mwv207_clamp_color** | `mwv207_clamp_color(uint32_t format, struct mwv207_sampler *sampler)` |
| 420 | `static void` | **mwv207_reconfig_texture** | `mwv207_reconfig_texture(struct mwv207_sampler_view *sampler_view, struct mwv207_sampler *sampler, bool use_ba)` |
| 473 | `static uint32_t` | **mwv207_get_stage_offset** | `mwv207_get_stage_offset(enum pipe_shader_type stage)` |
| 494 | `void` | **mwv207_config_sampler** | `mwv207_config_sampler(struct mwv207_context *ctx, enum pipe_shader_type stage)` |
| 647 | `static uint32_t` | **mwv207_get_texture_swizzle** | `mwv207_get_texture_swizzle(uint32_t swizzle, uint32_t current_set, const uint32_t *texture_swizzles)` |
| 673 | `static enum pipe_error` | **mwv207_get_texture_info** | `mwv207_get_texture_info(struct mwv207_context *ctx, struct mwv207_sampler_view *sampler_view)` |
| 857 | `static void *` | **mwv207_create_sampler_state** | `mwv207_create_sampler_state(struct pipe_context *pctx, const struct pipe_sampler_state *state)` |
| 914 | `static void` | **mwv207_bind_sampler_states** | `mwv207_bind_sampler_states(struct pipe_context *pctx, enum pipe_shader_type stage, unsigned start_slot, unsigned num_samplers, void **samplers)` |
| 938 | `static void` | **mwv207_delete_sampler_state** | `mwv207_delete_sampler_state(struct pipe_context *pctx, void *sampler_state)` |
| 945 | `static struct pipe_sampler_view *` | **mwv207_create_sampler_view** | `mwv207_create_sampler_view(struct pipe_context *pctx, struct pipe_resource *pres, const struct pipe_sampler_view *state)` |
| 991 | `static void` | **mwv207_sampler_view_destroy** | `mwv207_sampler_view_destroy(struct pipe_context *pctx, struct pipe_sampler_view *pview)` |
| 1002 | `static void` | **mwv207_set_sampler_views** | `mwv207_set_sampler_views(struct pipe_context *pctx, enum pipe_shader_type stage, unsigned start_slot, unsigned num_views, unsigned unbind_num, bool take_ownership, struct pipe_sampler_view **views)` |
| 1050 | `void *` | **mwv207_get_sampler_state** | `mwv207_get_sampler_state(struct mwv207_context *ctx)` |
| 1062 | `void` | **mwv207_context_sampler_init** | `mwv207_context_sampler_init(struct pipe_context *pctx)` |

### `drivers/mwv207/mwv207_screen.c`  (13 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 42 | `static const char *` | **mwv207_get_vendor** | `mwv207_get_vendor(struct pipe_screen *pscreen)` |
| 48 | `static const char *` | **mwv207_get_device_vendor** | `mwv207_get_device_vendor(struct pipe_screen *pscreen)` |
| 54 | `static const char *` | **mwv207_get_name** | `mwv207_get_name(struct pipe_screen *pscreen)` |
| 60 | `static int` | **mwv207_get_param** | `mwv207_get_param(struct pipe_screen *pscreen, enum pipe_cap param)` |
| 334 | `static float` | **mwv207_get_paramf** | `mwv207_get_paramf(struct pipe_screen *pscreen, enum pipe_capf param)` |
| 366 | `static int` | **mwv207_get_shader_param** | `mwv207_get_shader_param(struct pipe_screen *pscreen, enum pipe_shader_type shader, enum pipe_shader_cap param)` |
| 473 | `static int` | **mwv207_get_compute_param** | `mwv207_get_compute_param(struct pipe_screen *pscreen, enum pipe_shader_ir ir_type, enum pipe_compute_cap param, void *ret)` |
| 522 | `static bool` | **mwv207_sample_count_flags** | `mwv207_sample_count_flags(uint32_t sample_count)` |
| 539 | `static bool` | **mwv207_is_format_supported** | `mwv207_is_format_supported(struct pipe_screen *pscreen, enum pipe_format format, enum pipe_texture_target target, unsigned sample_count, unsigned storage_sample_count, unsigned bind)` |
| 595 | `static uint64_t` | **mwv207_get_timestamp** | `mwv207_get_timestamp(struct pipe_screen *screen)` |
| 610 | `static bool` | **mwv207_fence_finish** | `mwv207_fence_finish(struct pipe_screen *screen, struct pipe_context *ctx, struct pipe_fence_handle *fence, uint64_t timeout)` |
| 629 | `static void` | **mwv207_destroy_screen** | `mwv207_destroy_screen(struct pipe_screen *pscreen)` |
| 642 | `struct pipe_screen *` | **mwv207_create_screen** | `mwv207_create_screen(struct mwv207_winsys *winsys)` |

### `drivers/mwv207/mwv207_shader.c`  (25 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 59 | `static int` | **mwv207_get_gs_vertices** | `mwv207_get_gs_vertices(struct mwv207_context *ctx)` |
| 118 | `static void` | **mwv207_config_usc** | `mwv207_config_usc(struct mwv207_context *ctx)` |
| 226 | `void` | **mwv207_config_vs** | `mwv207_config_vs(struct mwv207_context *ctx)` |
| 371 | `void` | **mwv207_config_tcs** | `mwv207_config_tcs(struct mwv207_context *ctx)` |
| 434 | `void` | **mwv207_config_tes** | `mwv207_config_tes(struct mwv207_context *ctx)` |
| 560 | `void` | **mwv207_config_gs** | `mwv207_config_gs(struct mwv207_context *ctx)` |
| 636 | `void` | **mwv207_config_fs** | `mwv207_config_fs(struct mwv207_context *ctx)` |
| 686 | `enum pipe_error` | **mwv207_config_link** | `mwv207_config_link(struct mwv207_context *ctx)` |
| 856 | `const void *` | **mwv207_get_compiler_options** | `mwv207_get_compiler_options(struct pipe_screen *screen, enum pipe_shader_ir ir, enum pipe_shader_type shader)` |
| 869 | `static struct nir_shader *` | **mwv207_tgsi_to_nir** | `mwv207_tgsi_to_nir(struct pipe_screen *pscreen, const struct tgsi_token *tokens)` |
| 883 | `static struct mwv207_shader *` | **mwv207_create_shader** | `mwv207_create_shader(struct mwv207_shader_state *state, struct mwv207_shader_key key)` |
| 920 | `void static` | **mwv207_destroy_shader** | `mwv207_destroy_shader(struct mwv207_shader *shader)` |
| 934 | `bool` | **mwv207_set_current_shader** | `mwv207_set_current_shader(struct mwv207_context *ctx, struct mwv207_shader_state *state)` |
| 1019 | `static void *` | **mwv207_create_shader_state** | `mwv207_create_shader_state(struct pipe_context *pctx, const struct pipe_shader_state *pshader)` |
| 1041 | `static void` | **mwv207_bind_vs_state** | `mwv207_bind_vs_state(struct pipe_context *pctx, void *state)` |
| 1051 | `static void` | **mwv207_bind_fs_state** | `mwv207_bind_fs_state(struct pipe_context *pctx, void *state)` |
| 1061 | `static void` | **mwv207_bind_gs_state** | `mwv207_bind_gs_state(struct pipe_context *pctx, void *state)` |
| 1071 | `static void` | **mwv207_bind_tcs_state** | `mwv207_bind_tcs_state(struct pipe_context *pctx, void *state)` |
| 1081 | `static void` | **mwv207_bind_tes_state** | `mwv207_bind_tes_state(struct pipe_context *pctx, void *state)` |
| 1091 | `static void` | **mwv207_delete_shader_state** | `mwv207_delete_shader_state(struct pipe_context *pctx, void *pshader)` |
| 1113 | `static void *` | **mwv207_create_cs_state** | `mwv207_create_cs_state(struct pipe_context *pctx, const struct pipe_compute_state *pshader)` |
| 1141 | `static void` | **mwv207_get_compute_state_info** | `mwv207_get_compute_state_info(struct pipe_context *ctx, void *state, struct pipe_compute_state_object_info *info)` |
| 1152 | `static uint32_t` | **mwv207_get_compute_state_subgroup_size** | `mwv207_get_compute_state_subgroup_size(struct pipe_context *ctx, void *state, const uint32_t block[3])` |
| 1160 | `static void` | **mwv207_bind_cs_state** | `mwv207_bind_cs_state(struct pipe_context *pctx, void *state)` |
| 1170 | `void` | **mwv207_context_shader_init** | `mwv207_context_shader_init(struct pipe_context *pctx)` |

### `drivers/mwv207/mwv207_state.c`  (20 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 61 | `static uint32_t` | **line_scale** | `line_scale(float width, float granularity, const float min, const float max)` |
| 75 | `static uint32_t` | **fill_mode** | `fill_mode(uint32_t fill_mode)` |
| 93 | `static void *` | **mwv207_create_rasterizer_state** | `mwv207_create_rasterizer_state(struct pipe_context *pctx, const struct pipe_rasterizer_state *state)` |
| 159 | `static void` | **mwv207_bind_rasterizer_state** | `mwv207_bind_rasterizer_state(struct pipe_context *pctx, void *state)` |
| 167 | `static void` | **mwv207_delete_rasterizer_state** | `mwv207_delete_rasterizer_state(struct pipe_context *pctx, void *state)` |
| 173 | `static void *` | **mwv207_create_depth_stencil_alpha_state** | `mwv207_create_depth_stencil_alpha_state(struct pipe_context *pctx, const struct pipe_depth_stencil_alpha_state *state)` |
| 302 | `static void` | **mwv207_delete_depth_stencil_alpha_state** | `mwv207_delete_depth_stencil_alpha_state(struct pipe_context *pctx, void *state)` |
| 308 | `static void` | **mwv207_bind_depth_stencil_alpha_state** | `mwv207_bind_depth_stencil_alpha_state(struct pipe_context *pctx, void *state)` |
| 316 | `static void` | **mwv207_set_stencil_ref** | `mwv207_set_stencil_ref(struct pipe_context *pctx, const struct pipe_stencil_ref ref)` |
| 336 | `static void` | **mwv207_set_viewport_states** | `mwv207_set_viewport_states(struct pipe_context *pctx, unsigned start_slot, unsigned num_viewports, const struct pipe_viewport_state *state)` |
| 347 | `static void` | **mwv207_set_scissor_states** | `mwv207_set_scissor_states(struct pipe_context *pctx, unsigned start_slot, unsigned num_scissors, const struct pipe_scissor_state *states)` |
| 357 | `static void` | **mwv207_set_polygon_stipple** | `mwv207_set_polygon_stipple(struct pipe_context *pctx, const struct pipe_poly_stipple *ps)` |
| 364 | `static void` | **mwv207_set_clip_state** | `mwv207_set_clip_state(struct pipe_context *pctx, const struct pipe_clip_state *pcs)` |
| 371 | `static void` | **mwv207_set_stream_output_targets** | `mwv207_set_stream_output_targets(struct pipe_context *pctx, unsigned num_targets, struct pipe_stream_output_target **targets, const unsigned *offsets)` |
| 403 | `static struct pipe_stream_output_target *` | **mwv207_create_stream_output_target** | `mwv207_create_stream_output_target(struct pipe_context *ctx, struct pipe_resource *res, unsigned buffer_offset, unsigned buffer_size)` |
| 424 | `static void` | **mwv207_stream_output_target_destroy** | `mwv207_stream_output_target_destroy(struct pipe_context *pctx, struct pipe_stream_output_target *t)` |
| 438 | `static void` | **mwv207_set_tess_state** | `mwv207_set_tess_state(struct pipe_context *pctx, const float default_outer_level[4], const float default_inner_level[2])` |
| 448 | `static void` | **MWV207_set_patch_vertices** | `MWV207_set_patch_vertices(struct pipe_context *pctx, uint8_t patch_vertices)` |
| 455 | `static void` | **mwv207_set_sample_mask** | `mwv207_set_sample_mask(struct pipe_context *pctx, unsigned sample_mask)` |
| 464 | `void` | **mwv207_context_state_init** | `mwv207_context_state_init(struct pipe_context *pctx)` |

### `drivers/mwv207/mwv207_surface.c`  (8 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 37 | `void` | **mwv207_update_renderInfo** | `mwv207_update_renderInfo(struct mwv207_surface *surface)` |
| 52 | `static void` | **mwv207_get_renderInfo** | `mwv207_get_renderInfo(struct mwv207_surface *surface)` |
| 127 | `static void` | **mwv207_get_depthInfo** | `mwv207_get_depthInfo(struct mwv207_surface *surface)` |
| 188 | `static struct pipe_surface *` | **mwv207_create_surface** | `mwv207_create_surface(struct pipe_context *pctx, struct pipe_resource *pres, const struct pipe_surface *templ)` |
| 255 | `static void` | **mwv207_surface_destroy** | `mwv207_surface_destroy(struct pipe_context *pctx, struct pipe_surface *pSurface)` |
| 267 | `static int` | **mwv207_swzzile_state_equal** | `mwv207_swzzile_state_equal(uint32_t *cur_swzzi, uint32_t *pre_swzzi)` |
| 278 | `void` | **mwv207_set_framebuffer_state** | `mwv207_set_framebuffer_state(struct pipe_context *pctx, const struct pipe_framebuffer_state *state)` |
| 358 | `void` | **mwv207_context_surface_init** | `mwv207_context_surface_init(struct pipe_context *context)` |

### `drivers/mwv207/mwv207_vertex.c`  (18 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 40 | `static void *` | **mwv207_create_elements** | `mwv207_create_elements(struct pipe_context *pctx, unsigned num_elements, const struct pipe_vertex_element *elements)` |
| 68 | `static void` | **mwv207_bind_elements** | `mwv207_bind_elements(struct pipe_context *pctx, void *state)` |
| 78 | `static void` | **mwv207_delete_elements** | `mwv207_delete_elements(struct pipe_context *pctx, void *state)` |
| 84 | `static void` | **mwv207_set_vertex_buffers** | `mwv207_set_vertex_buffers(struct pipe_context *pctx, unsigned num_buffers, const struct pipe_vertex_buffer *buffers)` |
| 98 | `static uint32_t` | **get_texture_size** | `get_texture_size(struct mwv207_context *ctx, struct mwv207_shader *vs, enum mwv207_immediate_contents contents, uint32_t index)` |
| 113 | `static uint32_t` | **get_texrect_scale** | `get_texrect_scale(struct mwv207_context *ctx, enum pipe_shader_type stage, enum mwv207_immediate_contents contents, uint32_t index)` |
| 130 | `static uint32_t` | **get_ubo_offset** | `get_ubo_offset(struct mwv207_context *ctx, enum pipe_shader_type stage, uint32_t idx, uint32_t *cmd_buf, uint32_t base)` |
| 159 | `static uint32_t` | **get_ssbo_offset** | `get_ssbo_offset(struct mwv207_context *ctx, enum pipe_shader_type stage, uint32_t idx, uint32_t *cmd_buf, uint32_t base)` |
| 184 | `static uint32_t` | **get_global_offset** | `get_global_offset(struct mwv207_context *ctx, uint32_t idx, uint32_t *cmd_buf, uint32_t base)` |
| 199 | `static uint32_t` | **get_local_offset** | `get_local_offset(struct mwv207_context *ctx, uint32_t *cmd_buf, uint32_t base)` |
| 216 | `static uint32_t` | **get_ssimage_info** | `get_ssimage_info(struct mwv207_context *ctx, enum pipe_shader_type stage, uint32_t idx, uint32_t *cmd_buf, uint32_t type)` |
| 287 | `static uint32_t` | **get_atomic_offset** | `get_atomic_offset(struct mwv207_context *ctx, enum pipe_shader_type stage, uint32_t idx, uint32_t *cmd_buf, uint32_t base)` |
| 306 | `void` | **mwv207_config_uniform** | `mwv207_config_uniform(struct mwv207_context *ctx, enum pipe_shader_type stage)` |
| 451 | `static void` | **mwv207_set_constant_buffer** | `mwv207_set_constant_buffer(struct pipe_context *pctx, enum pipe_shader_type shader, uint index, bool take_ownership, const struct pipe_constant_buffer *cb)` |
| 468 | `void` | **mwv207_context_vertex_init** | `mwv207_context_vertex_init(struct pipe_context *pctx)` |
| 483 | `static uint32_t` | **bit_countn** | `bit_countn(uint64_t *bitfield, uint32_t n)` |
| 502 | `static uint32_t` | **bit_scann** | `bit_scann(uint32_t val, uint64_t *bitfield)` |
| 516 | `void` | **mwv207_emit_kernel_params** | `mwv207_emit_kernel_params(struct mwv207_context *ctx, const struct pipe_grid_info *info)` |

### `drivers/mwv207/nir/mwv207_cmdline.c`  (2 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 94 | `static void` | **compile_shader** | `compile_shader(int file_num, char **argv)` |
| 155 | `int` | **main** | `main(int argc, char **argv)` |

### `drivers/mwv207/nir/mwv207_common.c`  (11 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 30 | `uint32_t` | **inst_swiz_compose** | `inst_swiz_compose(uint32_t swz1, uint32_t swz2)` |
| 39 | `uint32_t` | **inst_write_mask_compose** | `inst_write_mask_compose(uint32_t wm1, uint32_t wm2)` |
| 56 | `struct mwv207_inst_src` | **src_swizzle** | `src_swizzle(struct mwv207_inst_src src, uint32_t swizzle)` |
| 65 | `bool` | **is_sysval** | `is_sysval(nir_instr *instr)` |
| 76 | `uint32_t` | **src_index** | `src_index(nir_function_impl *impl, nir_src *src)` |
| 93 | `uint32_t` | **dest_index** | `dest_index(nir_function_impl *impl, nir_def *def)` |
| 111 | `static void` | **update_swiz_mask** | `update_swiz_mask(nir_alu_instr *alu, nir_def *dest, uint32_t *swiz, uint32_t *mask)` |
| 136 | `` | **real_dest** | `real_dest(nir_def *dest, uint32_t *swiz, uint32_t *mask)` |
| 211 | `` | **dest_for_instr** | `dest_for_instr(nir_instr *instr)` |
| 251 | `int` | **const_add** | `const_add(uint64_t *c, uint64_t value)` |
| 264 | `int` | **mwv207_assemble** | `mwv207_assemble(uint32_t *out, const struct mwv207_inst *inst)` |

### `drivers/mwv207/nir/mwv207_compiler.c`  (13 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 35 | `static uint32_t` | **mwv207_get_interpolation** | `mwv207_get_interpolation(uint32_t interpolation)` |
| 48 | `static void` | **mwv207_record_input** | `mwv207_record_input(struct mwv207_compile *c)` |
| 139 | `static void` | **mwv207_reset_output_index** | `mwv207_reset_output_index(struct mwv207_compile *c)` |
| 235 | `static void` | **copy_uniform_state_to_shader** | `copy_uniform_state_to_shader(struct mwv207_shader *shader, uint64_t *consts, uint32_t count)` |
| 254 | `static void` | **mwv207_record_uniform** | `mwv207_record_uniform(struct mwv207_compile *c)` |
| 351 | `static void` | **mwv207_record_code** | `mwv207_record_code(struct mwv207_context *ctx, struct mwv207_compile *c, struct mwv207_shader_state *state, bool offline)` |
| 411 | `bool` | **mwv207_compile_shader** | `mwv207_compile_shader(struct mwv207_context *ctx, struct mwv207_shader_state *state, bool offline)` |
| 481 | `static const struct mwv207_shader_inout *` | **mwv207_stream_out_lookup** | `mwv207_stream_out_lookup(const struct mwv207_shader *shader, int output_index)` |
| 494 | `static int` | **mwv207_link_lookup** | `mwv207_link_lookup(const struct mwv207_shader_link_info *info, const struct mwv207_shader_inout *output)` |
| 505 | `static void` | **mwv207_fix_stream_out** | `mwv207_fix_stream_out(struct mwv207_shader_link_info *info, const struct mwv207_shader *shader)` |
| 545 | `static struct mwv207_shader_inout *` | **mwv207_output_lookup** | `mwv207_output_lookup(struct mwv207_shader *shader, struct mwv207_shader_inout *input)` |
| 564 | `static struct mwv207_shader *` | **mwv207_get_next_shader** | `mwv207_get_next_shader(struct mwv207_context *ctx, enum pipe_shader_type stage)` |
| 589 | `bool` | **mwv207_link_shader** | `mwv207_link_shader(struct mwv207_context *ctx, struct mwv207_shader *shader, struct mwv207_shader_link_info *info)` |

### `drivers/mwv207/nir/mwv207_compiler.h`  (1 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 58 | `static inline bool` | **mwv207_shader_key_equal** | `mwv207_shader_key_equal(struct mwv207_shader_key *a, struct mwv207_shader_key *b)` |

### `drivers/mwv207/nir/mwv207_disasm.c`  (28 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 65 | `static void` | **print_with_update_length** | `print_with_update_length(uint32_t *print_length, char *format, ...)` |
| 77 | `static void` | **print_align** | `print_align(uint32_t *print_length, uint32_t max_length)` |
| 93 | `static void` | **print_inst_crontrol** | `print_inst_crontrol(struct mwv207_inst_ctrl *inst_ctrl, struct mwv207_inst *operands, uint32_t *print_length)` |
| 248 | `static void` | **print_inst_dst** | `print_inst_dst(struct mwv207_inst_dst *dst, enum dst_addr_reg_type addr_reg_type, bool is_need_comma, uint32_t *print_length)` |
| 307 | `static uint64_t` | **get_bits** | `get_bits(uint64_t data, uint32_t start, uint32_t end)` |
| 319 | `static inline void` | **print_swiz_comp** | `print_swiz_comp(uint8_t swiz_comp, uint32_t *print_length)` |
| 345 | `static void` | **print_swiz** | `print_swiz(uint8_t swiz, uint32_t *print_length)` |
| 364 | `static void` | **print_each_src** | `print_each_src(struct mwv207_inst_src *src, bool is_need_comma, uint32_t index, enum kinds_of_inst kind_of_inst, uint32_t *print_length)` |
| 494 | `static void` | **print_inst_srcs** | `print_inst_srcs(struct mwv207_inst *operands, uint32_t *print_length)` |
| 510 | `static void` | **print_inst** | `print_inst(struct mwv207_inst *operands, struct instr *inst, uint32_t *print_length)` |
| 525 | `static void` | **decode_imm** | `decode_imm(uint32_t index, struct instr *inst, struct mwv207_inst_src *src)` |
| 552 | `static void` | **decode_each_src** | `decode_each_src(uint32_t index, struct mwv207_inst_src *src, struct instr *inst)` |
| 596 | `static void` | **decode_check_each_src** | `decode_check_each_src(uint32_t index, struct mwv207_inst *operands, struct instr *inst, uint32_t expected_src_idx_mask)` |
| 629 | `static void` | **decode_srcs** | `decode_srcs(struct mwv207_inst *operands, struct instr *inst, uint32_t expected_src_idx_mask)` |
| 638 | `static uint32_t` | **decode_inst_type** | `decode_inst_type(enum kinds_of_inst kind_of_inst, struct instr *inst)` |
| 653 | `static void` | **decode_inst_ctrl_alu_inst** | `decode_inst_ctrl_alu_inst(struct mwv207_inst *operands, struct instr *inst)` |
| 682 | `static void` | **decode_inst_ctrl_load_store_inst** | `decode_inst_ctrl_load_store_inst(struct mwv207_inst *operands, struct instr *inst, bool is_for_img, bool is_img_atom)` |
| 723 | `static void` | **decode_inst_ctrl** | `decode_inst_ctrl(struct mwv207_inst *operands, struct instr *inst)` |
| 814 | `static uint32_t` | **map_sample_hw_opcode_to_aux_opcode** | `map_sample_hw_opcode_to_aux_opcode(uint32_t base_opcode, uint32_t src_idx_1_and_2_mask)` |
| 849 | `static uint32_t` | **map_ld_st_hw_opcode_to_aux_opcode** | `map_ld_st_hw_opcode_to_aux_opcode(uint32_t base_opcode, bool is_dst_enabled)` |
| 868 | `static void` | **decode_direct_branch_target_src** | `decode_direct_branch_target_src(struct mwv207_inst_src *src, struct instr *inst, enum kinds_of_inst kind_of_inst)` |
| 895 | `static void` | **convert_inst** | `convert_inst(struct mwv207_inst *operands, struct instr *inst)` |
| 980 | `static void` | **convert_branch** | `convert_branch(struct mwv207_inst *operands, struct instr *inst)` |
| 996 | `static void` | **convert_call** | `convert_call(struct mwv207_inst *operands, struct instr *inst)` |
| 1005 | `static void` | **convert_clamp0_max** | `convert_clamp0_max(struct mwv207_inst *operands, struct instr *inst)` |
| 1193 | `static void` | **convert_and_print_inst** | `convert_and_print_inst(uint32_t *dwords, int n)` |
| 1245 | `void` | **mwv207_disasm** | `mwv207_disasm(uint32_t *dwords, int sizedwords)` |
| 1257 | `void` | **mwv207_dump_shader_nir** | `mwv207_dump_shader_nir(const struct mwv207_shader *shader)` |

### `drivers/mwv207/nir/mwv207_emit.c`  (48 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 33 | `void` | **emit_inst** | `emit_inst(struct mwv207_compile *c, struct mwv207_inst *inst)` |
| 40 | `static uint32_t` | **mwv207_get_store_attri_client** | `mwv207_get_store_attri_client(struct mwv207_compile *c)` |
| 62 | `static uint32_t` | **mwv207_get_load_attri_client** | `mwv207_get_load_attri_client(struct mwv207_compile *c)` |
| 84 | `static uint32_t` | **mwv207_get_LS_attr_client** | `mwv207_get_LS_attr_client(struct mwv207_compile *c)` |
| 114 | `static uint32_t` | **gen_coorddata** | `gen_coorddata(uint32_t dataType, uint32_t mode)` |
| 129 | `` | **get_deref_var_type** | `get_deref_var_type(nir_deref_instr *deref)` |
| 138 | `static uint32_t` | **get_deref_driver_location** | `get_deref_driver_location(nir_deref_instr *deref_instr)` |
| 147 | `static bool` | **is_deref_var_compact** | `is_deref_var_compact(nir_deref_instr *deref)` |
| 156 | `static uint32_t` | **get_deref_index** | `get_deref_index(nir_deref_instr *deref_instr)` |
| 180 | `static uint32_t` | **get_deref_index_perVertex** | `get_deref_index_perVertex(nir_deref_instr *deref_instr)` |
| 206 | `static uint32_t` | **get_alu_inst_type** | `get_alu_inst_type(nir_alu_type dest_type, uint32_t bitsize)` |
| 235 | `static bool` | **same_swiz** | `same_swiz(unsigned swiz_src, unsigned dst_write_mask)` |
| 283 | `static void` | **mwv207_emit_alu** | `mwv207_emit_alu(struct mwv207_compile *c, nir_op op, struct mwv207_inst_dst dst, struct mwv207_inst_src src[3], uint32_t src_bitsize, bool saturate)` |
| 428 | `static void` | **mwv207_emit_tex** | `mwv207_emit_tex(struct mwv207_compile *c, nir_texop op, uint32_t texid, struct mwv207_inst_dst dst, uint32_t dst_swiz, struct mwv207_inst_src coord, struct mwv207_inst_src lod_bias, struct mwv207_inst_src compare, struct mwv207_inst_src ms_index, struct mwv207_inst_src tex_offset, uint32_t component)` |
| 488 | `static void` | **mwv207_emit_jump_with_nz** | `mwv207_emit_jump_with_nz(struct mwv207_compile *c, unsigned block, struct mwv207_inst_src condition)` |
| 509 | `static void` | **mwv207_emit_jump** | `mwv207_emit_jump(struct mwv207_compile *c, uint32_t block, struct mwv207_inst_src condition)` |
| 530 | `static void` | **mwv207_emit_discard** | `mwv207_emit_discard(struct mwv207_compile *c, struct mwv207_inst_src condition)` |
| 550 | `static void` | **mwv207_emit_load_patch** | `mwv207_emit_load_patch(struct mwv207_compile *c, nir_intrinsic_instr *intr)` |
| 577 | `static void` | **mwv207_emit_load_gs** | `mwv207_emit_load_gs(struct mwv207_compile *c, nir_intrinsic_instr *intr)` |
| 634 | `static void` | **mwv207_emit_load_perVertex** | `mwv207_emit_load_perVertex(struct mwv207_compile *c, nir_intrinsic_instr *intr)` |
| 796 | `static void` | **mwv207_emit_load_deref** | `mwv207_emit_load_deref(struct mwv207_compile *c, nir_intrinsic_instr *intr)` |
| 811 | `static void` | **mwv207_emit_store_gs** | `mwv207_emit_store_gs(struct mwv207_compile *c, nir_variable *var, nir_intrinsic_instr *intr, struct mwv207_inst_src src)` |
| 832 | `static void` | **mwv207_emit_store_tess** | `mwv207_emit_store_tess(struct mwv207_compile *c, nir_variable *var, nir_intrinsic_instr *intr, struct mwv207_inst_src src)` |
| 907 | `static void` | **mwv207_emit_load_shared** | `mwv207_emit_load_shared(struct mwv207_compile *c, nir_intrinsic_instr *intr)` |
| 932 | `static void` | **mwv207_emit_load_buffer** | `mwv207_emit_load_buffer(struct mwv207_compile *c, nir_intrinsic_instr *intr, bool is_local)` |
| 954 | `static uint32_t` | **get_atomic_op** | `get_atomic_op(nir_intrinsic_instr *intr)` |
| 986 | `static struct mwv207_inst_src` | **get_image_info** | `get_image_info(struct mwv207_compile *c, uint32_t ssimage_index)` |
| 999 | `static void` | **mwv207_emit_image_addr** | `mwv207_emit_image_addr(struct mwv207_compile *c, struct mwv207_inst_src src, int ssimage_index, int opcode)` |
| 1022 | `static void` | **mwv207_emit_atom** | `mwv207_emit_atom(struct mwv207_compile *c, nir_intrinsic_instr *intr, bool is_local, bool is_image)` |
| 1126 | `static void` | **mwv207_emit_store_shared** | `mwv207_emit_store_shared(struct mwv207_compile *c, nir_intrinsic_instr *intr)` |
| 1159 | `static void` | **mwv207_emit_store_buffer** | `mwv207_emit_store_buffer(struct mwv207_compile *c, nir_intrinsic_instr *intr, bool is_local)` |
| 1188 | `static void` | **mwv207_emit_load_image** | `mwv207_emit_load_image(struct mwv207_compile *c, nir_intrinsic_instr *intr)` |
| 1233 | `static void` | **mwv207_emit_store_image** | `mwv207_emit_store_image(struct mwv207_compile *c, nir_intrinsic_instr *intr)` |
| 1284 | `static void` | **mwv207_emit_output** | `mwv207_emit_output(struct mwv207_compile *c, nir_intrinsic_instr *intr, struct mwv207_inst_src src)` |
| 1388 | `static void` | **mwv207_emit_block_start** | `mwv207_emit_block_start(struct mwv207_compile *c, uint32_t block)` |
| 1395 | `static void` | **emit_alu** | `emit_alu(struct mwv207_compile *c, nir_alu_instr *alu)` |
| 1443 | `static void` | **emit_tex** | `emit_tex(struct mwv207_compile *c, nir_tex_instr *tex)` |
| 1501 | `static void` | **emit_intrinsic** | `emit_intrinsic(struct mwv207_compile *c, nir_intrinsic_instr *intr)` |
| 1814 | `static void` | **emit_instr** | `emit_instr(struct mwv207_compile *c, nir_instr *instr)` |
| 1845 | `static void` | **emit_block** | `emit_block(struct mwv207_compile *c, nir_block *block)` |
| 1862 | `static bool` | **has_only_one_jump_instr** | `has_only_one_jump_instr(nir_block *block, nir_jump_instr **jump)` |
| 1881 | `static bool` | **is_block_empty** | `is_block_empty(nir_block *block)` |
| 1888 | `static bool` | **is_then_bail_else_empty_if** | `is_then_bail_else_empty_if(nir_if *nif)` |
| 1917 | `static void` | **emit_if** | `emit_if(struct mwv207_compile *c, nir_if *nif)` |
| 1936 | `static void` | **emit_cf_list** | `emit_cf_list(struct mwv207_compile *c, struct exec_list *list)` |
| 1961 | `static void` | **mwv207_fix_start** | `mwv207_fix_start(struct mwv207_compile *c)` |
| 1993 | `static void` | **mwv207_fix_end** | `mwv207_fix_end(struct mwv207_compile *c)` |
| 2014 | `void` | **mwv207_emit_shader** | `mwv207_emit_shader(struct mwv207_context *ctx, struct mwv207_compile *c)` |

### `drivers/mwv207/nir/mwv207_emit.h`  (5 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 47 | `static inline bool` | **mwv207_rgroup_is_uniform** | `mwv207_rgroup_is_uniform(uint32_t rgroup)` |
| 54 | `static inline struct mwv207_inst_src` | **mwv207_immediate_src** | `mwv207_immediate_src(uint32_t type, uint32_t bits)` |
| 65 | `static inline struct mwv207_inst_src` | **mwv207_immediate_float** | `mwv207_immediate_float(float x)` |
| 73 | `static inline struct mwv207_inst_src` | **mwv207_immediate_int** | `mwv207_immediate_int(int x)` |
| 80 | `static inline struct mwv207_inst_src` | **mwv207_immediate_uint** | `mwv207_immediate_uint(int x)` |

### `drivers/mwv207/nir/mwv207_liveness.c`  (7 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 31 | `static void` | **range_include** | `range_include(struct live_def *def, uint32_t index)` |
| 41 | `static bool` | **init_liveness_block** | `init_liveness_block(nir_block *block, struct live_defs_state *state)` |
| 58 | `static bool` | **set_src_live** | `set_src_live(nir_src *src, void *void_state)` |
| 95 | `static bool` | **set_ssa_def_dead** | `set_ssa_def_dead(nir_def *def, void *void_state)` |
| 110 | `static bool` | **propagate_across_edge** | `propagate_across_edge(nir_block *pred, nir_block *succ, struct live_defs_state *state)` |
| 124 | `bool` | **is_decl_reg** | `is_decl_reg(nir_instr *instr)` |
| 134 | `uint32_t` | **live_defs** | `live_defs(nir_function_impl *impl, struct live_def *defs, uint32_t *live_map, bool has_store)` |

### `drivers/mwv207/nir/mwv207_lower.c`  (38 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 40 | `` | **get_texture_size** | `get_texture_size(nir_builder *b, nir_tex_instr *tex)` |
| 94 | `` | **compute_Lod** | `compute_Lod(nir_builder *b, nir_tex_instr *tex)` |
| 127 | `static void` | **mwv207_tex_offset** | `mwv207_tex_offset(nir_builder *b, nir_tex_instr *tex, struct mwv207_shader *shader)` |
| 202 | `static bool` | **is_const_ubo** | `is_const_ubo(const nir_instr *instr, const void *_data)` |
| 226 | `` | **lower_ubo_to_uniform** | `lower_ubo_to_uniform(nir_builder *b, nir_instr *instr, void *_data)` |
| 257 | `static bool` | **mwv207_nir_lower_ubo_to_uniform** | `mwv207_nir_lower_ubo_to_uniform(nir_shader *shader)` |
| 268 | `static void` | **mwv207_lower_sprite_coord** | `mwv207_lower_sprite_coord(nir_shader *nir, struct mwv207_shader *shader)` |
| 285 | `static bool` | **can_remove_dead_varying** | `can_remove_dead_varying(nir_variable *var, void *data)` |
| 292 | `static void` | **mwv207_lower_sprite_coord_late** | `mwv207_lower_sprite_coord_late(nir_shader *nir, struct mwv207_compile *c)` |
| 348 | `static void` | **mwv207_lower_io** | `mwv207_lower_io(nir_shader *nir, struct mwv207_shader *shader)` |
| 476 | `static bool` | **mwv207_alu_to_scalar_filter_cb** | `mwv207_alu_to_scalar_filter_cb(const nir_instr *instr, const void *data)` |
| 551 | `static void` | **mwv207_lower_alu_impl** | `mwv207_lower_alu_impl(nir_function_impl *impl, struct mwv207_compile *c)` |
| 606 | `static void` | **mwv207_lower_alu** | `mwv207_lower_alu(nir_shader *nir, struct mwv207_compile *c)` |
| 615 | `static void` | **mwv207_lower_global_impl** | `mwv207_lower_global_impl(nir_function_impl *impl)` |
| 723 | `static bool` | **mwv207_lower_global** | `mwv207_lower_global(nir_shader *nir)` |
| 732 | `static void` | **split_double_store_global** | `split_double_store_global(nir_function_impl *impl)` |
| 776 | `static bool` | **mwv207_lower_double_store_global** | `mwv207_lower_double_store_global(nir_shader *nir)` |
| 785 | `static bool` | **vec_dest_has_swizzle** | `vec_dest_has_swizzle(nir_alu_instr *vec, nir_def *ssa)` |
| 818 | `static uint32_t` | **insert_vec_mov** | `insert_vec_mov(nir_alu_instr *vec, uint32_t start_idx, nir_shader *nir)` |
| 862 | `static void` | **mwv207_lower_vecn_impl** | `mwv207_lower_vecn_impl(nir_function_impl *impl, struct mwv207_compile *c)` |
| 950 | `static void` | **mwv207_lower_vecn** | `mwv207_lower_vecn(nir_shader *nir, struct mwv207_compile *c)` |
| 959 | `static void` | **mwv207_exchange_texture_swizzle** | `mwv207_exchange_texture_swizzle(struct mwv207_context *ctx, nir_tex_instr *tex, nir_alu_instr *mov, nir_tex_instr *tex_ba, nir_builder b)` |
| 1014 | `static void` | **mwv207_lower_tex_128_impl** | `mwv207_lower_tex_128_impl(nir_function_impl *impl, struct mwv207_context *ctx)` |
| 1084 | `static void` | **mwv207_lower_fb_128_impl** | `mwv207_lower_fb_128_impl(nir_function_impl *impl, struct mwv207_context *ctx)` |
| 1184 | `static void` | **mwv207_lower_128bit** | `mwv207_lower_128bit(nir_shader *nir, struct mwv207_context *ctx)` |
| 1205 | `static void` | **mwv207_lower_rgba_swap_impl** | `mwv207_lower_rgba_swap_impl(nir_function_impl *impl, struct mwv207_context *ctx)` |
| 1263 | `static void` | **mwv207_lower_rgba_swap** | `mwv207_lower_rgba_swap(nir_shader *shader, struct mwv207_context *ctx)` |
| 1277 | `static void` | **mwv207_lower_txs** | `mwv207_lower_txs(nir_shader *nir, struct mwv207_context *ctx, struct mwv207_shader *shader)` |
| 1315 | `static void` | **mwv207_lower_texelFetch** | `mwv207_lower_texelFetch(nir_shader *nir, struct mwv207_context *ctx, struct mwv207_shader *shader)` |
| 1339 | `static void` | **mwv207_lower_roundEven** | `mwv207_lower_roundEven(nir_shader *nir, struct mwv207_context *ctx)` |
| 1377 | `static void` | **mwv207_nir_remove_shadow_tex** | `mwv207_nir_remove_shadow_tex(nir_tex_instr *tex, nir_def * (*set)(nir_builder *b, nir_def *src0, nir_def *src1), nir_builder *b, bool swap)` |
| 1407 | `static void` | **mwv207_lower_shadow_tex_impl** | `mwv207_lower_shadow_tex_impl(nir_function_impl *impl, struct mwv207_context *ctx, struct mwv207_shader *shader)` |
| 1477 | `static void` | **mwv207_lower_shadow_tex** | `mwv207_lower_shadow_tex(nir_shader *nir, struct mwv207_context *ctx, struct mwv207_shader *shader)` |
| 1489 | `static int` | **mwv207_glsl_type_size** | `mwv207_glsl_type_size(const struct glsl_type *type, bool bindless)` |
| 1495 | `static void` | **mwv207_optimize_loop** | `mwv207_optimize_loop(nir_shader *nir)` |
| 1535 | `` | **mem_access_size_align_cb** | `mem_access_size_align_cb(nir_intrinsic_op intrin, uint8_t bytes, uint8_t bit_size, uint32_t align, uint32_t align_offset, bool offset_is_const, const void *cb_data)` |
| 1552 | `static uint8_t` | **lower_vec816_alu** | `lower_vec816_alu(const nir_instr *instr, const void *cb_data)` |
| 1558 | `void` | **mwv207_lower** | `mwv207_lower(struct mwv207_context *ctx, struct mwv207_compile *c)` |

### `drivers/mwv207/nir/mwv207_lower_64bit.c`  (28 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 42 | `bool` | **mwv207_fp64_to_scalar_filter_cb** | `mwv207_fp64_to_scalar_filter_cb(const nir_instr *instr, const void *data)` |
| 62 | `static struct variable_pair *` | **get_var_pair** | `get_var_pair(nir_builder *b, nir_variable *old_var, struct hash_table *split_vars)` |
| 102 | `static bool` | **need_split_64bit_var** | `need_split_64bit_var(const nir_instr *instr, const void *data)` |
| 136 | `` | **merge_64bit_loads** | `merge_64bit_loads(nir_builder *b, nir_def *load1, nir_def *load2, bool out_is_vec3)` |
| 155 | `` | **split_double_load_uniform** | `split_double_load_uniform(nir_builder *b, nir_intrinsic_instr *intr)` |
| 175 | `` | **split_double_load_ubo** | `split_double_load_ubo(nir_builder *b, nir_intrinsic_instr *intr)` |
| 198 | `` | **split_double_load_ssbo** | `split_double_load_ssbo(nir_builder *b, nir_intrinsic_instr *intr)` |
| 217 | `` | **split_double_load_global** | `split_double_load_global(nir_builder *b, nir_intrinsic_instr *intr)` |
| 232 | `` | **split_store_deref_var** | `split_store_deref_var(nir_builder *b, nir_intrinsic_instr *intr, UNUSED nir_deref_instr *deref, struct hash_table *split_vars)` |
| 261 | `` | **split_store_deref_array** | `split_store_deref_array(nir_builder *b, nir_intrinsic_instr *intr, nir_deref_instr *deref, struct hash_table *split_vars)` |
| 296 | `` | **split_double_store_deref** | `split_double_store_deref(nir_builder *b, nir_intrinsic_instr *intr, struct hash_table *split_vars)` |
| 310 | `` | **split_load_deref_var** | `split_load_deref_var(nir_builder *b, nir_intrinsic_instr *intr, struct hash_table *split_vars)` |
| 330 | `` | **split_load_deref_array** | `split_load_deref_array(nir_builder *b, nir_intrinsic_instr *intr, nir_src *index, struct hash_table *split_vars)` |
| 357 | `` | **split_double_load_deref** | `split_double_load_deref(nir_builder *b, nir_intrinsic_instr *intr, struct hash_table *split_vars)` |
| 371 | `` | **split_load_const** | `split_load_const(nir_builder *b, nir_load_const_instr *lc)` |
| 381 | `` | **split_64bit_var** | `split_64bit_var(nir_builder *b, nir_instr *instr, void *data)` |
| 416 | `bool` | **mwv207_nir_split_64bit_var** | `mwv207_nir_split_64bit_var(nir_shader *shader)` |
| 430 | `static bool` | **is_64bit_uniforms_and_ubo** | `is_64bit_uniforms_and_ubo(const nir_instr *instr, const void *data)` |
| 445 | `` | **lower_64bit_uniforms_and_ubo** | `lower_64bit_uniforms_and_ubo(nir_builder *b, nir_instr *instr, void *data)` |
| 473 | `bool` | **mwv207_nir_lower_64bit_uniforms_and_ubo** | `mwv207_nir_lower_64bit_uniforms_and_ubo(nir_shader *shader)` |
| 482 | `` | **load_64_to_vec2** | `load_64_to_vec2(nir_intrinsic_instr *intr)` |
| 493 | `` | **store_64_to_vec2** | `store_64_to_vec2(nir_builder *b, nir_intrinsic_instr *intr)` |
| 524 | `static bool` | **store_64bit_intr** | `store_64bit_intr(nir_src *src, void *state)` |
| 532 | `static bool` | **double2vec2** | `double2vec2(nir_src *src, UNUSED void *state)` |
| 543 | `static bool` | **need_lower_to_vec2** | `need_lower_to_vec2(const nir_instr *instr, const void *data)` |
| 588 | `` | **load_deref_64_to_vec2** | `load_deref_64_to_vec2(nir_builder *b, nir_intrinsic_instr *intr)` |
| 619 | `` | **lower_64bit_to_vec2** | `lower_64bit_to_vec2(nir_builder *b, nir_instr *instr, void *data)` |
| 693 | `bool` | **mwv207_nir_64_to_vec2** | `mwv207_nir_64_to_vec2(nir_shader *shader)` |

### `drivers/mwv207/nir/mwv207_lower_dual.c`  (6 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 30 | `` | **get_factor_def** | `get_factor_def(struct mwv207_context *ctx, nir_builder *b, unsigned int factor, nir_def *blend_src, nir_def *blend_src1, nir_def *blend_dst)` |
| 110 | `` | **get_rt_texture_coord** | `get_rt_texture_coord(struct mwv207_context *ctx, nir_builder *b)` |
| 132 | `` | **create_rt_texture** | `create_rt_texture(struct mwv207_context *ctx, nir_shader *nir, nir_def *tex_coord)` |
| 154 | `` | **handle_blend_func** | `handle_blend_func(unsigned int func, nir_builder *b, nir_def *src, nir_def *dst)` |
| 181 | `static void` | **mwv207_lower_dual_blend_impl** | `mwv207_lower_dual_blend_impl(nir_function_impl *impl, struct mwv207_context *ctx, struct mwv207_shader *shader)` |
| 261 | `void` | **mwv207_lower_dual_blend** | `mwv207_lower_dual_blend(nir_shader *nir, struct mwv207_context *ctx, struct mwv207_shader *shader)` |

### `drivers/mwv207/nir/mwv207_lower_shared_atomic.c`  (5 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 30 | `` | **set_base_offset_impl** | `set_base_offset_impl(nir_instr *instr, nir_builder *b, nir_shader *nir)` |
| 93 | `` | **set_base_offset** | `set_base_offset(nir_shader *nir)` |
| 116 | `static bool` | **lower_instr** | `lower_instr(nir_shader *nir, nir_intrinsic_instr *instr, nir_builder *b, nir_def **base_offset_ptr)` |
| 147 | `static bool` | **check_has_shared_atomic** | `check_has_shared_atomic(nir_shader *nir)` |
| 170 | `void` | **mwv207_lower_shared_atomic** | `mwv207_lower_shared_atomic(nir_shader *nir, struct mwv207_compile *c, struct mwv207_context *ctx)` |

### `drivers/mwv207/nir/mwv207_lower_tg4.c`  (2 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 32 | `static bool` | **lower_instr** | `lower_instr(nir_builder *b, nir_instr *instr, struct mwv207_context *ctx, struct mwv207_shader *shader)` |
| 95 | `void` | **mwv207_nir_lower_tg4** | `mwv207_nir_lower_tg4(nir_shader *nir, struct mwv207_shader *shader, struct mwv207_context *ctx)` |

### `drivers/mwv207/nir/mwv207_nir_opt_peephole_fdiv.c`  (4 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 33 | `` | **get_load_const_for_src** | `get_load_const_for_src(nir_alu_src *src)` |
| 42 | `static bool` | **mwv207_nir_opt_peephole_fdiv_block** | `mwv207_nir_opt_peephole_fdiv_block(nir_builder *b, nir_block *block)` |
| 99 | `static bool` | **mwv207_nir_opt_peephole_fdiv_impl** | `mwv207_nir_opt_peephole_fdiv_impl(nir_function_impl *impl)` |
| 117 | `bool` | **mwv207_nir_opt_peephole_fdiv** | `mwv207_nir_opt_peephole_fdiv(nir_shader *shader)` |

### `drivers/mwv207/nir/mwv207_nir_opt_peephole_ffma.c`  (5 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 32 | `static inline bool` | **are_all_uses_fadd** | `are_all_uses_fadd(nir_def *def)` |
| 66 | `` | **get_mul_for_src** | `get_mul_for_src(nir_alu_src *src, unsigned num_components, uint8_t swizzle[4], bool *negate, bool *abs)` |
| 123 | `static bool` | **mwv207_nir_opt_peephole_ffma_block** | `mwv207_nir_opt_peephole_ffma_block(nir_builder *b, nir_block *block)` |
| 206 | `static bool` | **mwv207_nir_opt_peephole_ffma_impl** | `mwv207_nir_opt_peephole_ffma_impl(nir_function_impl *impl)` |
| 224 | `bool` | **mwv207_nir_opt_peephole_ffma** | `mwv207_nir_opt_peephole_ffma(nir_shader *shader)` |

### `drivers/mwv207/nir/mwv207_nir_opt_peephole_set.c`  (4 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 32 | `` | **get_cmp_for_src** | `get_cmp_for_src(nir_alu_src *src, unsigned num_components, uint8_t swizzle[4])` |
| 76 | `static bool` | **mwv207_nir_opt_peephole_set_block** | `mwv207_nir_opt_peephole_set_block(nir_builder *b, nir_block *block)` |
| 160 | `static bool` | **mwv207_nir_opt_peephole_set_impl** | `mwv207_nir_opt_peephole_set_impl(nir_function_impl *impl)` |
| 178 | `bool` | **mwv207_nir_opt_peephole_set** | `mwv207_nir_opt_peephole_set(nir_shader *shader)` |

### `drivers/mwv207/nir/mwv207_ra.c`  (12 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 172 | `static int` | **reg_get_type** | `reg_get_type(int virt_reg)` |
| 178 | `static int` | **reg_get_base** | `reg_get_base(struct mwv207_compile *c, int virt_reg)` |
| 208 | `static int` | **reg_get_class** | `reg_get_class(int virt_reg)` |
| 253 | `struct mwv207_inst_src` | **mwv207_const** | `mwv207_const(struct mwv207_compile *c, nir_const_value *value, uint32_t num_components)` |
| 302 | `static struct mwv207_inst_src` | **mwv207_reg_src** | `mwv207_reg_src(struct mwv207_compile *c, nir_src *src)` |
| 309 | `struct mwv207_inst_src` | **mwv207_src** | `mwv207_src(struct mwv207_compile *c, nir_src *src)` |
| 459 | `struct mwv207_inst_dst` | **mwv207_dest** | `mwv207_dest(struct mwv207_compile *c, nir_def *dest, uint32_t *p_swiz)` |
| 481 | `struct mwv207_inst_src` | **mwv207_src_from_dest** | `mwv207_src_from_dest(struct mwv207_compile *c, nir_def *dest)` |
| 498 | `static void` | **mwv207_spill_reg** | `mwv207_spill_reg(struct mwv207_compile *c, struct ra_graph *g, uint32_t num_nodes, struct live_def *defs, nir_function_impl *impl, uint32_t debug)` |
| 574 | `static bool` | **mwv207_ra** | `mwv207_ra(struct mwv207_compile *c, nir_shader *nir, struct ra_regs *regs, uint32_t debug)` |
| 799 | `void` | **mwv207_init_reg_state** | `mwv207_init_reg_state(struct mwv207_context *ctx, struct mwv207_compile *c, nir_shader *nir)` |
| 840 | `void` | **mwv207_free_reg_state** | `mwv207_free_reg_state(struct mwv207_compile *c)` |

### `drivers/mwv207/video-codec/common/mwv207_codec_time.c`  (3 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 33 | `void` | **mwv207_codec_time_init** | `mwv207_codec_time_init(struct mwv207_codec_time *codecTime)` |
| 39 | `void` | **mwv207_codec_time_record_start** | `mwv207_codec_time_record_start(struct mwv207_codec_time *codecTime)` |
| 53 | `void` | **mwv207_codec_time_compute** | `mwv207_codec_time_compute(struct mwv207_codec_time *codecTime)` |

### `drivers/mwv207/video-codec/common/mwv207_decoder_h264_nal_unit.c`  (1 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 66 | `uint32_t` | **mwv207_h264_bsd_decode_nal_unit** | `mwv207_h264_bsd_decode_nal_unit(struct mwv207_strm_data *stream, mwv207_h264_nal_unit *p_nal_unit)` |

### `drivers/mwv207/video-codec/common/mwv207_decoder_h264_slice_header.c`  (6 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 92 | `uint32_t` | **mwv207_h264_bsd_decode_slice_header** | `mwv207_h264_bsd_decode_slice_header( struct mwv207_strm_data *stream, struct mwv207_h264_picture_desc *pic_param, //  VASliceParameterBufferH264 *slice_param, mwv207_h264_slice_header *p_slice_header, mwv207_h264_nal_unit *p_nal_unit)` |
| 395 | `uint32_t` | **ref_pic_list_reordering** | `ref_pic_list_reordering( struct mwv207_strm_data *stream, mwv207_ref_pic_list_reordering *p_ref_pic_list_reordering, uint32_t num_ref_idx_active, uint32_t max_pic_num, uint32_t mvc)` |
| 490 | `uint32_t` | **dec_ref_pic_marking** | `dec_ref_pic_marking(struct mwv207_strm_data *stream, mwv207_dec_ref_pic_marking *p_dec_ref_pic_marking, uint32_t idr_pic_flag, uint32_t num_ref_frames)` |
| 611 | `static int` | **mwv207_h264_decode_ref_pic_marking** | `mwv207_h264_decode_ref_pic_marking(mwv207_h264_slice_header *sl, struct mwv207_strm_data *stream, uint32_t idr_pic_flag)` |
| 701 | `uint32_t` | **pred_weight_table** | `pred_weight_table(struct mwv207_strm_data *stream, mwv207_h264_slice_header *p_slice_header, uint32_t mono_chrome)` |
| 756 | `uint32_t` | **mwv207_h264_bsd_Ref_idx_override** | `mwv207_h264_bsd_Ref_idx_override(struct mwv207_strm_data *stream, struct mwv207_h264_picture_desc *pic_param, uint32_t *num_ref_idx_active_override_flag)` |

### `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_exp_golomb.c`  (2 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 60 | `uint32_t` | **mwv207_hevc_decode_exp_golomb_unsigned** | `mwv207_hevc_decode_exp_golomb_unsigned(struct mwv207_strm_data *stream, uint32_t *code_num)` |
| 148 | `uint32_t` | **mwv207_hevc_decode_exp_golomb_signed** | `mwv207_hevc_decode_exp_golomb_signed(struct mwv207_strm_data *stream, int32_t *value)` |

### `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_slice_header.c`  (2 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 34 | `static uint32_t` | **hevc_decode_short_term_ref_pic_set** | `hevc_decode_short_term_ref_pic_set(struct mwv207_strm_data *stream, struct mwv207_st_ref_pic_set *st_rps, uint32_t slice_header, uint32_t st_rps_idx)` |
| 160 | `uint32_t` | **mwv207_hevc_decode_slice_header** | `mwv207_hevc_decode_slice_header(struct mwv207_strm_data *stream, struct mwv207_h265_picture_desc *pic_param, uint32_t *hw_skip_bits, struct mwv207_hevc_slice_header *slice_header, uint32_t st_rps_bits /* , VASliceParameterBufferHEVC *slice_param */ )` |

### `drivers/mwv207/video-codec/common/mwv207_decoder_sw_stream.c`  (5 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 37 | `uint32_t` | **mwv207_sw_get_bits** | `mwv207_sw_get_bits(struct mwv207_strm_data *stream, uint32_t num_bits)` |
| 58 | `uint32_t` | **mwv207_sw_get_bits_unsigned_max** | `mwv207_sw_get_bits_unsigned_max(struct mwv207_strm_data *stream, uint32_t max_value)` |
| 77 | `uint32_t` | **mwv207_sw_show_bits** | `mwv207_sw_show_bits(struct mwv207_strm_data *stream, uint32_t num_bits)` |
| 190 | `uint32_t` | **mwv207_sw_flush_bits** | `mwv207_sw_flush_bits(struct mwv207_strm_data *stream, uint32_t num_bits)` |
| 331 | `uint32_t` | **mwv207_sw_is_byte_aligned** | `mwv207_sw_is_byte_aligned(const struct mwv207_strm_data *stream)` |

### `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.c`  (3 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 35 | `uint32_t` | **mwv207_sw_count_leading_zeros** | `mwv207_sw_count_leading_zeros(uint32_t value, uint32_t length)` |
| 50 | `uint32_t` | **mwv207_sw_num_bits** | `mwv207_sw_num_bits(uint32_t value)` |
| 56 | `uint8_t *` | **mwv207_sw_turn_around** | `mwv207_sw_turn_around( const uint8_t *strm, const uint8_t *buf, uint8_t *tmp_buf, uint32_t buf_size, uint32_t num_bits)` |

### `drivers/mwv207/video-codec/common/mwv207_decoder_vp9_slice_header.c`  (22 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 45 | `static uint32_t` | **check_sync_code** | `check_sync_code(struct mwv207_strm_data *rb)` |
| 57 | `static void` | **existing_fef_register** | `existing_fef_register(struct mwv207_vp9_slice_header *slice_header, uint32_t map)` |
| 63 | `static uint32_t` | **setup_frame_size** | `setup_frame_size(struct mwv207_strm_data *rb, struct mwv207_vp9_slice_header *slice_header)` |
| 95 | `static void` | **decode_lf_params** | `decode_lf_params(struct mwv207_strm_data *rb, struct mwv207_vp9_slice_header *slice_header)` |
| 168 | `static int32_t` | **decode_quantizer_delta** | `decode_quantizer_delta(struct mwv207_strm_data *rb)` |
| 189 | `static void` | **get_tile_n_bits** | `get_tile_n_bits(struct mwv207_vp9_slice_header *slice_header, uint32_t *min_log2_ntiles_ptr, uint32_t *delta_log2_ntiles)` |
| 213 | `static void` | **decode_segmentation_data** | `decode_segmentation_data(struct mwv207_strm_data *rb, struct mwv207_vp9_slice_header *slice_header)` |
| 304 | `static uint32_t` | **setup_frame_size_with_refs** | `setup_frame_size_with_refs(struct mwv207_strm_data *rb, struct mwv207_vp9_slice_header *slice_header)` |
| 375 | `static void` | **mwv207_vp9_init_mode_contexts** | `mwv207_vp9_init_mode_contexts(struct mwv207_vp9_slice_header *slice_header)` |
| 686 | `static void` | **mwv207_vp9_init_mbmode_probs** | `mwv207_vp9_init_mbmode_probs(struct mwv207_vp9_slice_header *x)` |
| 766 | `static void` | **mwv207_vp9_init_mv_probs** | `mwv207_vp9_init_mv_probs(struct mwv207_vp9_slice_header *x)` |
| 774 | `static void` | **mwv207_vp9_reset_probs** | `mwv207_vp9_reset_probs(struct mwv207_vp9_slice_header *slice_header)` |
| 821 | `static void` | **vp9_reset_decoder** | `vp9_reset_decoder(struct mwv207_vp9_slice_header *slice_header)` |
| 846 | `static void` | **vp9_get_probs** | `vp9_get_probs(struct mwv207_vp9_slice_header *slice_header)` |
| 855 | `static uint32_t` | **vp9_decode_uncompressed_header** | `vp9_decode_uncompressed_header(struct mwv207_vp9_slice_header *slice_header, struct mwv207_strm_data rb, uint32_t data_len)` |
| 1148 | `static uint32_t` | **vp9_decode_coeff_update** | `vp9_decode_coeff_update(struct mwv207_vp_bool_coder *bc, uint8_t prob_coeffs[BLOCK_TYPES][REF_TYPES][COEF_BANDS] [PREV_COEF_CONTEXTS][ENTROPY_NODES_PART1])` |
| 1197 | `static uint8_t` | **dwl_private_area_read_byte** | `dwl_private_area_read_byte(const uint8_t *p)` |
| 1203 | `static void` | **vp9_store_adapt_probs** | `vp9_store_adapt_probs(struct mwv207_vp9_slice_header *slice_header)` |
| 1210 | `static uint32_t` | **read_tile_size** | `read_tile_size(const uint8_t *cx_size)` |
| 1221 | `static void` | **update_nmv** | `update_nmv(struct mwv207_vp_bool_coder *bc, vp9_prob *const p, const vp9_prob upd_p)` |
| 1233 | `static uint32_t` | **mwv207_vp_decode_mv_update** | `mwv207_vp_decode_mv_update(struct mwv207_vp_bool_coder *bc, struct mwv207_vp9_slice_header *slice_header)` |
| 1276 | `static uint32_t` | **vp9_decode_compressed_header** | `vp9_decode_compressed_header(uint8_t *stream, uint32_t strm_len, struct mwv207_vp_bool_coder *bc, uint8_t *buffer, uint32_t buf_len, struct mwv207_vp9_slice_header *slice_header)` |

### `drivers/mwv207/video-codec/common/mwv207_log.c`  (1 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 48 | `void` | **mwv207_log_init** | `mwv207_log_init(void)` |

### `drivers/mwv207/video-codec/common/mwv207_video_cs.c`  (4 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 36 | `void` | **mwv207_video_cs_init** | `mwv207_video_cs_init(struct mwv207_video_cs *cs, struct mwv207_winsys *ws)` |
| 45 | `void` | **mwv207_video_cs_release** | `mwv207_video_cs_release(struct mwv207_video_cs *cs)` |
| 53 | `static void` | **copy_one_cmd** | `copy_one_cmd(struct mwv207_video_cs *cs, struct mwv207_video_reg_buf *regbuf)` |
| 82 | `void` | **mwv207_video_cs_submit** | `mwv207_video_cs_submit(struct mwv207_video_cs *cs, struct mwv207_video_reg_buf *regbuf)` |

### `drivers/mwv207/video-codec/common/mwv207_video_reg_buf.c`  (7 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 34 | `static void` | **bos_clear** | `bos_clear(struct mwv207_video_reg_buf *reg_buf)` |
| 43 | `static int` | **vbo_cmp** | `vbo_cmp(const void *a, const void *b)` |
| 51 | `static void` | **bos_sort** | `bos_sort(struct mwv207_video_reg_buf *reg_buf)` |
| 57 | `static void` | **add_bo** | `add_bo(struct mwv207_video_reg_buf *reg_buf, uint32_t swreg, struct mwv207_bo *bo, uint64_t offset, uint32_t flags, uint32_t type)` |
| 79 | `static void` | **dump_reg_stream_print** | `dump_reg_stream_print(struct mwv207_video_reg_buf *reg_buf,const char *fmt, ...)` |
| 91 | `struct mwv207_video_reg_buf *` | **mwv207_video_reg_buf_create** | `mwv207_video_reg_buf_create(struct mwv207_winsys *ws)` |
| 118 | `void` | **mwv207_video_reg_buf_destroy** | `mwv207_video_reg_buf_destroy(struct mwv207_video_reg_buf *reg_buf)` |

### `drivers/mwv207/video-codec/common/mwv207_vp9hwd_bool.c`  (13 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 33 | `static int` | **Vp9InvRecenterNonneg** | `Vp9InvRecenterNonneg(int v, int m)` |
| 43 | `static int` | **MergeIndex** | `MergeIndex(int v, int n, int modulus)` |
| 60 | `static int` | **InvRemapProb** | `InvRemapProb(int v, int m)` |
| 72 | `` | **mwv207_vp9_read_prob_diff_update** | `mwv207_vp9_read_prob_diff_update(struct mwv207_vp_bool_coder *bc, int oldp)` |
| 80 | `static uint8_t` | **DWLPrivateAreaReadByte** | `DWLPrivateAreaReadByte(const uint8_t *p)` |
| 86 | `uint32_t` | **mwv207_vp9_decode_bool** | `mwv207_vp9_decode_bool(struct mwv207_vp_bool_coder *br, int32_t probability)` |
| 170 | `static uint32_t` | **vp9_decode_bool_128** | `vp9_decode_bool_128(struct mwv207_vp_bool_coder *br)` |
| 252 | `static int` | **GetUnsignedBits** | `GetUnsignedBits(uint32_t num_values)` |
| 266 | `static uint32_t` | **BoolDecodeUniform** | `BoolDecodeUniform(struct mwv207_vp_bool_coder *bc, uint32_t n)` |
| 282 | `uint32_t` | **mwv207_vp9_decode_sub_exp** | `mwv207_vp9_decode_sub_exp(struct mwv207_vp_bool_coder *bc, uint32_t k, uint32_t num_syms)` |
| 307 | `void` | **mwv207_vp9_bool_start** | `mwv207_vp9_bool_start(struct mwv207_vp_bool_coder *br, const uint8_t *stream, uint32_t strm_len, const uint8_t *strm_buf, uint32_t buf_len)` |
| 347 | `void` | **mwv207_vp9_bool_stop** | `mwv207_vp9_bool_stop(struct mwv207_vp_bool_coder *bc)` |
| 353 | `uint32_t` | **mwv207_vp9_read_bits** | `mwv207_vp9_read_bits(struct mwv207_vp_bool_coder *br, int32_t bits)` |

### `drivers/mwv207/video-codec/mwv207/mwv207_dec_h264.c`  (32 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 453 | `static void` | **dec_h264_set_common_regs** | `dec_h264_set_common_regs(struct mwv207_video_decoder *decoder, struct mwv207_h264_picture_desc *h264)` |
| 493 | `static void` | **dec_h264_set_picparam_regs** | `dec_h264_set_picparam_regs(struct mwv207_video_decoder *decoder, struct mwv207_h264_picture_desc *pic_param)` |
| 659 | `static void` | **printSliceType** | `printSliceType(uint8_t slice_type)` |
| 681 | `static int` | **mwv207_dec_h264_init_poc** | `mwv207_dec_h264_init_poc(int pic_field_poc[2], int *pic_poc, const struct mwv207_h264_sps *sps, H264POCContext *pc, int picture_structure, int nal_ref_idc)` |
| 774 | `static void` | **generate_sliding_window_mmcos** | `generate_sliding_window_mmcos(struct mwv207_video_dec *dec, struct mwv207_h264_picture_desc *pic_param)` |
| 812 | `static int` | **pic_num_extract** | `pic_num_extract(struct mwv207_video_dec *h, int pic_num, int *structure)` |
| 832 | `static void` | **remove_short_at_index** | `remove_short_at_index(struct mwv207_video_dec *h, int i)` |
| 852 | `static struct mwv207_picture_h264 *` | **find_short** | `find_short(struct mwv207_video_dec *h, int frame_num, int *idx)` |
| 873 | `static struct mwv207_picture_h264 *` | **remove_short** | `remove_short(struct mwv207_video_dec *h, int frame_num, int ref_mask)` |
| 897 | `static struct mwv207_picture_h264 *` | **remove_long** | `remove_long(struct mwv207_video_dec *h, int i, int ref_mask)` |
| 922 | `static int` | **mwv207_h264_execute_ref_pic_marking** | `mwv207_h264_execute_ref_pic_marking( struct mwv207_video_dec *h, struct mwv207_h264_picture_desc *pic_param, struct pipe_video_buffer *target)` |
| 1109 | `static int` | **fill_my_ref_frames** | `fill_my_ref_frames(struct mwv207_video_dec *h, struct mwv207_h264_picture_desc *pic_param)` |
| 1136 | `void` | **mwv207_h264_remove_all_refs** | `mwv207_h264_remove_all_refs(struct mwv207_video_dec *h)` |
| 1164 | `static void` | **idr** | `idr(struct mwv207_video_dec *h)` |
| 1173 | `static void` | **dec_h264_set_sh_regs** | `dec_h264_set_sh_regs(struct mwv207_video_decoder *decoder, struct mwv207_h264_picture_desc *pic_param, struct pipe_video_buffer *target)` |
| 1319 | `static void` | **dec_avc_calc_sur_offset** | `dec_avc_calc_sur_offset(struct mwv207_video_decoder *decoder, struct mwv207_h264_picture_desc *pic_param, struct pipe_video_buffer *target)` |
| 1391 | `static int` | **dec_ensure_surface_res** | `dec_ensure_surface_res(struct mwv207_video_decoder *decoder, struct mwv207_h264_picture_desc *pic_param, struct pipe_video_buffer *target)` |
| 1406 | `static void` | **dec_h264_avc_set_pp_regs** | `dec_h264_avc_set_pp_regs(struct mwv207_video_decoder *decoder, struct mwv207_h264_picture_desc *pic_param, struct pipe_video_buffer *target)` |
| 1460 | `static bool` | **is_short_term_frame** | `is_short_term_frame(const struct mwv207_picture_h264 *pic)` |
| 1469 | `static int32_t` | **compare_pictures** | `compare_pictures(struct mwv207_h264_picture_desc *pic_param, const void *ptr1, const void *ptr2)` |
| 1525 | `static int32_t` | **compare_pictures_b** | `compare_pictures_b(const void *ptr1, const void *ptr2, int32_t curr_poc)` |
| 1578 | `static void` | **shell_sort** | `shell_sort(struct mwv207_video_decoder *decoder, struct mwv207_h264_picture_desc *pic_param, uint32_t *list, uint32_t type, int32_t par)` |
| 1615 | `static void` | **h264_init_ref_pic_list_1** | `h264_init_ref_pic_list_1(struct mwv207_video_decoder *decoder, struct mwv207_h264_picture_desc *pic_param, uint32_t *list0, uint32_t *list1)` |
| 1682 | `static void` | **h264_init_ref_pic_list** | `h264_init_ref_pic_list(struct mwv207_video_decoder *decoder, struct mwv207_h264_picture_desc *pic_param)` |
| 1716 | `static void` | **dec_h264_set_rpl_regs** | `dec_h264_set_rpl_regs(struct mwv207_video_decoder *decoder, struct mwv207_h264_picture_desc *pic_param)` |
| 1854 | `static void` | **dec_h264_set_cur_out_buffer_info** | `dec_h264_set_cur_out_buffer_info(struct mwv207_video_decoder *decoder, struct mwv207_h264_picture_desc *pic_param, struct pipe_video_buffer *target)` |
| 1965 | `static void` | **dec_h264_set_ref_buffer_info** | `dec_h264_set_ref_buffer_info(struct mwv207_video_decoder *decoder, struct mwv207_h264_picture_desc *pic_param, struct pipe_video_buffer *target)` |
| 2087 | `static void` | **dec_avc_set_matrix_flag** | `dec_avc_set_matrix_flag(struct mwv207_video_decoder *decoder, struct mwv207_h264_picture_desc *pic_param)` |
| 2129 | `static void` | **dec_h264_set_cabac_data** | `dec_h264_set_cabac_data(struct mwv207_video_decoder *decoder, struct mwv207_h264_picture_desc *pic_param)` |
| 2238 | `static void` | **dec_h264_set_stream_data** | `dec_h264_set_stream_data(struct mwv207_video_decoder *decoder, struct mwv207_h264_picture_desc *pic_param)` |
| 2281 | `void` | **dump_referece_frames** | `dump_referece_frames(struct mwv207_video_decoder *dec, struct mwv207_h264_picture_desc *pic_param)` |
| 2313 | `void` | **mwv207_dec_h264_set_regs** | `mwv207_dec_h264_set_regs(struct mwv207_video_decoder *decoder, struct pipe_video_buffer *target)` |

### `drivers/mwv207/video-codec/mwv207/mwv207_dec_hevc.c`  (12 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 280 | `static void` | **dec_hevc_calc_sur_offset** | `dec_hevc_calc_sur_offset(struct mwv207_video_decoder *decoder, struct mwv207_h265_picture_desc *pic_param, struct pipe_video_buffer *target)` |
| 378 | `static void` | **dec_hevc_set_pp_reg** | `dec_hevc_set_pp_reg(struct mwv207_video_decoder *decoder, struct mwv207_h265_picture_desc *pic_param, struct pipe_video_buffer *target)` |
| 422 | `static void` | **dec_hevc_set_picparam_reg** | `dec_hevc_set_picparam_reg(struct mwv207_video_decoder *decoder, struct mwv207_h265_picture_desc *pic_param)` |
| 669 | `static void` | **dec_hevc_set_skip_length_reg** | `dec_hevc_set_skip_length_reg(struct mwv207_video_decoder *decoder, struct mwv207_h265_picture_desc *pic_param)` |
| 712 | `static void` | **dec_hevc_set_out_reg** | `dec_hevc_set_out_reg(struct mwv207_video_decoder *decoder, struct mwv207_h265_picture_desc *pic_param, struct pipe_video_buffer *target)` |
| 866 | `static void` | **dec_hevc_ref_list_init** | `dec_hevc_ref_list_init(struct mwv207_video_decoder *decoder, struct mwv207_h265_picture_desc *pic_param, struct pipe_video_buffer *target)` |
| 889 | `static int32_t` | **CompareDeltaPoc** | `CompareDeltaPoc(const void *p1, const void *p2)` |
| 896 | `static void` | **dec_hevc_set_rpl_reg** | `dec_hevc_set_rpl_reg(struct mwv207_video_decoder *decoder, struct mwv207_h265_picture_desc *pic_param, struct pipe_video_buffer *target)` |
| 1051 | `static void` | **dec_hevc_check_and_alloc_asic_buffer** | `dec_hevc_check_and_alloc_asic_buffer( struct mwv207_video_decoder *decoder, struct mwv207_h265_picture_desc *pic_param, struct pipe_video_buffer *target)` |
| 1107 | `static void` | **dec_hevc_set_misc_value_reg** | `dec_hevc_set_misc_value_reg(struct mwv207_video_decoder *decoder, struct mwv207_h265_picture_desc *pic_param, struct pipe_video_buffer *target)` |
| 1194 | `static void` | **dec_hevc_set_stream_data_reg** | `dec_hevc_set_stream_data_reg(struct mwv207_video_decoder *decoder, struct mwv207_h265_picture_desc *pic_param, struct pipe_video_buffer *target)` |
| 1225 | `void` | **mwv207_dec_hevc_set_regs** | `mwv207_dec_hevc_set_regs(struct mwv207_video_decoder *decoder, struct pipe_video_buffer *target)` |

### `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.c`  (12 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 356 | `static void` | **CalOutTableIndex** | `CalOutTableIndex(uint32_t table_in_index, uint32_t coeff_in_index, uint32_t filter_size, uint32_t *table_out_index, uint32_t *coeff_out_index)` |
| 372 | `static double` | **getSplineCoeff** | `getSplineCoeff(double a, double b, double c, double d, double dist)` |
| 385 | `static void` | **InitPpUnitLanczosData** | `InitPpUnitLanczosData(struct mwv207_video_dec_hw_features *hw_feature, mwv207_pp_unit_int_config *ppu_cfg)` |
| 947 | `void` | **mwv207_cal_second_up_scale_ratio** | `mwv207_cal_second_up_scale_ratio(mwv207_pp_unit_int_config *ppu_cfg, uint32_t interlace)` |
| 1006 | `uint32_t` | **mwv207_check_pp_unit_config** | `mwv207_check_pp_unit_config(struct mwv207_video_dec_hw_features *hw_feature, uint32_t in_width, uint32_t in_height, uint32_t interlace, mwv207_pp_unit_int_config *ppu_cfg)` |
| 1248 | `void` | **mwv207_pp_set_regs** | `mwv207_pp_set_regs(struct mwv207_video_reg_buf *reg_buf, struct mwv207_video_dec_hw_features *hw_feature, mwv207_pp_unit_int_config *ppu_cfg, uint32_t mono_chrome, uint32_t bottom_field_flag)` |
| 1557 | `static void` | **CalcDec400TableSize** | `CalcDec400TableSize(mwv207_pp_unit_int_config *ppu_cfg, uint32_t mono_chrome_flag)` |
| 1568 | `void` | **mwv207_calc_each_pp_buffer_size** | `mwv207_calc_each_pp_buffer_size(mwv207_pp_unit_int_config *ppu_cfg, uint32_t dec400_mode, uint32_t mono_chrome)` |
| 1613 | `uint32_t` | **mwv207_calc_pp_unit_buffer_size** | `mwv207_calc_pp_unit_buffer_size(mwv207_pp_unit_int_config *ppu_cfg, uint32_t mono_chrome)` |
| 1652 | `void` | **mwv207_pp_unit_set_int_config** | `mwv207_pp_unit_set_int_config(mwv207_pp_unit_int_config *ppu_int_cfg, mwv207_pp_unit_config *ppu_ext_cfg, uint32_t pixel_width, uint32_t frame_only, uint32_t mono_chrome)` |
| 1851 | `enum mwv207_dec_picture_format` | **mwv207_trans_unit_config_2_format** | `mwv207_trans_unit_config_2_format(mwv207_pp_unit_int_config *ppu_int_cfg)` |
| 1956 | `void` | **mwv207_check_output_format** | `mwv207_check_output_format(mwv207_pp_unit_int_config *ppu_cfg, enum mwv207_dec_picture_format *output_format, uint32_t index)` |

### `drivers/mwv207/video-codec/mwv207/mwv207_video_dec.c`  (7 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 34 | `void` | **mwv207_dec_set_common_config_regs** | `mwv207_dec_set_common_config_regs(struct mwv207_video_dec *dec)` |
| 117 | `static void` | **avc_init_ref** | `avc_init_ref(struct mwv207_video_dec *dec)` |
| 132 | `struct mwv207_video_dec *` | **mwv207_video_dec_create** | `mwv207_video_dec_create(struct mwv207_winsys *ws)` |
| 146 | `void` | **mwv207_video_dec_destroy_buffer** | `mwv207_video_dec_destroy_buffer(struct mwv207_video_dec *dec)` |
| 170 | `void` | **mwv207_video_dec_destroy** | `mwv207_video_dec_destroy(struct mwv207_video_dec *dec)` |
| 181 | `void` | **mwv207_9200_video_dec_picture** | `mwv207_9200_video_dec_picture(struct mwv207_video_decoder *decoder, struct pipe_video_buffer *target)` |
| 196 | `int` | **mwv207_dec_pp_params_init** | `mwv207_dec_pp_params_init(struct mwv207_video_dec *dec, mwv207_pp_unit_config *ppu_ext_cfg, uint32_t bit_depth, struct pipe_video_buffer *target)` |

### `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_reg.c`  (5 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 54 | `void` | **mwv207_9200_set_dec_register** | `mwv207_9200_set_dec_register(struct mwv207_video_reg_buf *reg_buf, uint32_t id, uint32_t value)` |
| 85 | `void` | **mwv207_9200_set_addr_reg** | `mwv207_9200_set_addr_reg(struct mwv207_video_reg_buf *reg_buf, uint32_t id, struct mwv207_bo *bo, uint64_t offset, uint32_t flags)` |
| 105 | `void` | **mwv207_9200_set_addr_reg_hi** | `mwv207_9200_set_addr_reg_hi(struct mwv207_video_reg_buf *reg_buf, uint32_t id, struct mwv207_bo *bo, uint64_t offset, uint32_t flags)` |
| 125 | `uint32_t` | **mwv207_9200_get_dec_register** | `mwv207_9200_get_dec_register(struct mwv207_video_reg_buf *reg_buf, uint32_t id)` |
| 144 | `void` | **mwv207_9200_dump_register** | `mwv207_9200_dump_register(struct mwv207_video_reg_buf *reg_buf)` |

### `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_vpufeature.c`  (1 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 37 | `void` | **mwv207_video_dec_get_hw_features_by_id** | `mwv207_video_dec_get_hw_features_by_id( uint32_t hw_build_id, struct mwv207_video_dec_hw_features *hw_feature)` |

### `drivers/mwv207/video-codec/mwv207_video_buffer.c`  (21 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 51 | `void` | **mwv207_get_video_buffer_formats** | `mwv207_get_video_buffer_formats(struct pipe_screen *screen, enum pipe_format format, enum pipe_format out_format[VL_NUM_COMPONENTS])` |
| 70 | `const unsigned *` | **mwv207_video_buffer_plane_order** | `mwv207_video_buffer_plane_order(enum pipe_format format)` |
| 91 | `static enum pipe_format` | **mwv207_video_buffer_surface_format** | `mwv207_video_buffer_surface_format(enum pipe_format format)` |
| 103 | `bool` | **mwv207_video_buffer_is_format_supported** | `mwv207_video_buffer_is_format_supported(struct pipe_screen *screen, enum pipe_format format, enum pipe_video_profile profile, enum pipe_video_entrypoint entrypoint)` |
| 133 | `unsigned` | **mwv207_video_buffer_max_size** | `mwv207_video_buffer_max_size(struct pipe_screen *screen)` |
| 139 | `void` | **mwv207_video_buffer_set_associated_data** | `mwv207_video_buffer_set_associated_data( struct pipe_video_buffer *vbuf, struct pipe_video_codec *vcodec, void *associated_data, void (*destroy_associated_data)(void *))` |
| 158 | `void *` | **mwv207_video_buffer_get_associated_data** | `mwv207_video_buffer_get_associated_data(struct pipe_video_buffer *vbuf, struct pipe_video_codec *vcodec)` |
| 168 | `void` | **mwv207_video_buffer_template** | `mwv207_video_buffer_template(struct pipe_resource *templ, const struct pipe_video_buffer *tmpl, enum pipe_format resource_format, unsigned depth, unsigned array_size, unsigned usage, unsigned plane)` |
| 203 | `void` | **mwv207_video_buffer_destroy** | `mwv207_video_buffer_destroy(struct pipe_video_buffer *buffer)` |
| 231 | `static struct pipe_sampler_view **` | **mwv207_video_buffer_sampler_view_planes** | `mwv207_video_buffer_sampler_view_planes(struct pipe_video_buffer *buffer)` |
| 269 | `static struct pipe_sampler_view **` | **mwv207_video_buffer_sampler_view_components** | `mwv207_video_buffer_sampler_view_components(struct pipe_video_buffer *buffer)` |
| 323 | `static struct pipe_surface **` | **mwv207_video_buffer_surfaces** | `mwv207_video_buffer_surfaces(struct pipe_video_buffer *buffer)` |
| 367 | `struct pipe_video_buffer *` | **mwv207_video_buffer_create** | `mwv207_video_buffer_create(struct pipe_context *pipe, const struct pipe_video_buffer *tmpl)` |
| 408 | `struct pipe_video_buffer *` | **mwv207_video_buffer_create_ex** | `mwv207_video_buffer_create_ex( struct pipe_context *pipe, const struct pipe_video_buffer *tmpl, const enum pipe_format resource_formats[VL_NUM_COMPONENTS], unsigned depth, unsigned array_size, unsigned usage)` |
| 460 | `static void` | **mwv207_video_buffer_resources** | `mwv207_video_buffer_resources(struct pipe_video_buffer *buffer, struct pipe_resource **resources)` |
| 472 | `struct pipe_video_buffer *` | **mwv207_video_buffer_create_ex2** | `mwv207_video_buffer_create_ex2( struct pipe_context *pipe, const struct pipe_video_buffer *tmpl, struct pipe_resource *resources[VL_NUM_COMPONENTS])` |
| 507 | `struct pipe_video_buffer *` | **mwv207_video_buffer_create_as_resource** | `mwv207_video_buffer_create_as_resource(struct pipe_context *pipe, const struct pipe_video_buffer *tmpl)` |
| 546 | `struct pipe_video_buffer *` | **mwv207_pipe_video_buffer_create** | `mwv207_pipe_video_buffer_create(struct pipe_context *pipe, const struct pipe_video_buffer *templat)` |
| 566 | `struct mwv207_video_buffer *` | **mwv207_video_buffer_create_2** | `mwv207_video_buffer_create_2(struct pipe_context *pipe, unsigned size, unsigned usage)` |
| 591 | `void` | **mwv207_video_buffer_alloc_buffer** | `mwv207_video_buffer_alloc_buffer(struct pipe_context *pipe, struct mwv207_video_buffer *buffer, unsigned size, unsigned usage)` |
| 607 | `void` | **mwv207_video_buffer_clear** | `mwv207_video_buffer_clear(struct pipe_context *context, struct mwv207_video_buffer *buffer)` |

### `drivers/mwv207/video-codec/mwv207_video_buffer.h`  (1 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 88 | `static inline void` | **mwv207_video_buffer_adjust_size** | `mwv207_video_buffer_adjust_size(unsigned *width, unsigned *height, unsigned plane, enum pipe_video_chroma_format chroma_format, bool interlaced)` |

### `drivers/mwv207/video-codec/mwv207_video_codec.c`  (5 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 35 | `static bool` | **mwv207_profile_supported** | `mwv207_profile_supported(struct pipe_screen *screen, enum pipe_video_profile profile, enum pipe_video_entrypoint entrypoint)` |
| 58 | `static int` | **mwv207_get_video_param** | `mwv207_get_video_param(struct pipe_screen *pscreen, enum pipe_video_profile profile, enum pipe_video_entrypoint entrypoint, enum pipe_video_cap param)` |
| 88 | `static bool` | **mwv207_is_video_format_supported** | `mwv207_is_video_format_supported(struct pipe_screen *pscreen, enum pipe_format format, enum pipe_video_profile profile, enum pipe_video_entrypoint entrypoint)` |
| 137 | `void` | **mwv207_context_video_codec_init** | `mwv207_context_video_codec_init(struct pipe_context *context)` |
| 149 | `struct pipe_video_codec *` | **mwv207_create_codec** | `mwv207_create_codec(struct pipe_context *context, const struct pipe_video_codec *codec)` |

### `drivers/mwv207/video-codec/mwv207_video_decoder.c`  (9 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 58 | `static void` | **get_h264_msg** | `get_h264_msg(struct mwv207_video_decoder *dec, struct pipe_h264_picture_desc *pic)` |
| 190 | `static void` | **get_h265_msg** | `get_h265_msg(struct mwv207_video_decoder *dec, struct pipe_h265_picture_desc *pic)` |
| 385 | `static void` | **mwv207_video_decoder_destroy** | `mwv207_video_decoder_destroy(struct pipe_video_codec *decoder)` |
| 407 | `static void` | **mwv207_video_decoder_begin_frame** | `mwv207_video_decoder_begin_frame(struct pipe_video_codec *decoder, struct pipe_video_buffer *target, struct pipe_picture_desc *picture)` |
| 435 | `static void` | **mwv207_video_decoder_decode_macroblock** | `mwv207_video_decoder_decode_macroblock( struct pipe_video_codec *decoder, struct pipe_video_buffer *target, struct pipe_picture_desc *picture, const struct pipe_macroblock *macroblocks, unsigned num_macroblocks)` |
| 450 | `static void` | **mwv207_video_decoder_decode_bitstream** | `mwv207_video_decoder_decode_bitstream(struct pipe_video_codec *decoder, struct pipe_video_buffer *target, struct pipe_picture_desc *picture, unsigned num_buffers, const void *const *buffers, const unsigned *sizes)` |
| 522 | `static void` | **mwv207_video_decoder_end_frame** | `mwv207_video_decoder_end_frame(struct pipe_video_codec *decoder, struct pipe_video_buffer *target, struct pipe_picture_desc *picture)` |
| 588 | `static void` | **mwv207_video_decoder_flush** | `mwv207_video_decoder_flush(struct pipe_video_codec *decoder)` |
| 593 | `struct pipe_video_codec *` | **mwv207_create_decoder** | `mwv207_create_decoder(struct pipe_context *context, const struct pipe_video_codec *decoder)` |

### `drivers/mwv207/video-codec/mwv207_video_decoder.h`  (1 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 79 | `static inline struct mwv207_video_decoder *` | **mwv207_video_dec** | `mwv207_video_dec(struct pipe_video_codec *decoder)` |

### `winsys/mwv207/drm/mwv207_drm_bo.c`  (18 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 47 | `static void` | **mwv207_clean_up_buffer_managers** | `mwv207_clean_up_buffer_managers(struct mwv207_winsys *ws)` |
| 55 | `static void` | **mwv207_drm_bo_destory** | `mwv207_drm_bo_destory(struct mwv207_winsys *ws, struct mwv207_bo *bo)` |
| 80 | `static void` | **mwv207_bo_destroy** | `mwv207_bo_destroy(void *winsys, struct pb_buffer_lean *buf)` |
| 88 | `static void` | **mwv207_bo_slab_destroy** | `mwv207_bo_slab_destroy(void *winsys, struct pb_buffer *buf)` |
| 101 | `static void` | **mwv207_bo_destroy_or_cache** | `mwv207_bo_destroy_or_cache(void *winsys, struct pb_buffer *buf)` |
| 117 | `static struct mwv207_bo *` | **mwv207_drm_bo_create** | `mwv207_drm_bo_create(struct mwv207_winsys *ws, uint64_t size, enum mwv207_bo_domain domain, enum mwv207_bo_flag flag)` |
| 222 | `static int64_t` | **get_abs_timeout** | `get_abs_timeout(uint64_t timeout)` |
| 238 | `static int` | **mwv207_drm_bo_wait** | `mwv207_drm_bo_wait(struct mwv207_winsys *ws, struct mwv207_bo *bo, uint64_t timeout)` |
| 261 | `static void` | **mwv207_drm_bo_reference** | `mwv207_drm_bo_reference(struct mwv207_winsys *ws, struct mwv207_bo **dst, struct mwv207_bo *src)` |
| 270 | `static void *` | **mwv207_drm_bo_map** | `mwv207_drm_bo_map(struct mwv207_winsys *ws, struct mwv207_bo *bo)` |
| 297 | `static void` | **mwv207_drm_bo_unmap** | `mwv207_drm_bo_unmap(struct mwv207_winsys *ws, struct mwv207_bo *bo)` |
| 303 | `static uint32_t` | **mwv207_drm_bo_get_handle** | `mwv207_drm_bo_get_handle(struct mwv207_winsys *ws, struct mwv207_bo *bo, struct winsys_handle *whandle)` |
| 338 | `static struct mwv207_bo *` | **mwv207_drm_bo_from_handle** | `mwv207_drm_bo_from_handle(struct mwv207_winsys *ws, struct winsys_handle *whandle, uint32_t size)` |
| 412 | `static void` | **mwv207_drm_bo_dump** | `mwv207_drm_bo_dump(struct mwv207_winsys *ws, struct mwv207_bo *bo, uint32_t size)` |
| 437 | `static bool` | **mwv207_bo_can_reclaim** | `mwv207_bo_can_reclaim(void *winsys, struct pb_buffer_lean *buf)` |
| 449 | `static bool` | **mwv207_bo_can_reclaim_slab** | `mwv207_bo_can_reclaim_slab(void *priv, struct pb_slab_entry *entry)` |
| 463 | `static struct pb_slab *` | **mwv207_bo_slab_alloc** | `mwv207_bo_slab_alloc(void *priv, unsigned heap, unsigned entry_size, unsigned group_index)` |
| 531 | `static void` | **mwv207_bo_slab_free** | `mwv207_bo_slab_free(void *priv, struct pb_slab *pslab)` |

### `winsys/mwv207/drm/mwv207_drm_cs.c`  (13 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 46 | `static void` | **mwv207_drm_destroy_fence** | `mwv207_drm_destroy_fence(struct pipe_fence_handle *fence)` |
| 54 | `static struct pipe_fence_handle *` | **mwv207_drm_fence_create** | `mwv207_drm_fence_create(struct mwv207_winsys *ws, int fd)` |
| 70 | `static void` | **mwv207_drm_fence_reference** | `mwv207_drm_fence_reference(struct pipe_fence_handle **dst, struct pipe_fence_handle *src)` |
| 85 | `static int` | **mwv207_drm_fence_wait** | `mwv207_drm_fence_wait(struct mwv207_winsys *ws, struct pipe_fence_handle *fence, uint64_t timeout)` |
| 107 | `static int` | **mwv207_drm_fence_get_fd** | `mwv207_drm_fence_get_fd(struct mwv207_winsys *ws, struct pipe_fence_handle *fence)` |
| 114 | `static void` | **mwv207_drm_fence_accumulate** | `mwv207_drm_fence_accumulate(struct mwv207_winsys *ws, int *target_fd, struct pipe_fence_handle *fence)` |
| 123 | `static struct mwv207_cmd *` | **mwv207_drm_cmdbuf_create** | `mwv207_drm_cmdbuf_create(struct mwv207_winsys *ws)` |
| 153 | `static void` | **mwv207_drm_cmdbuf_destory** | `mwv207_drm_cmdbuf_destory(struct mwv207_winsys *ws, struct mwv207_cmd *cmd)` |
| 179 | `static int` | **mwv207_drm_cmdbuf_add_bo** | `mwv207_drm_cmdbuf_add_bo(struct mwv207_cmd *cmd, struct mwv207_bo *bo, uint32_t flags)` |
| 219 | `static int` | **mwv207_drm_cmdbuf_add_dat** | `mwv207_drm_cmdbuf_add_dat(struct mwv207_cmd *cmd, struct cmd_dat *src_dat)` |
| 274 | `static int` | **mwv207_drm_cmdbuf_add_relloc** | `mwv207_drm_cmdbuf_add_relloc(struct mwv207_cmd *cmd, uint32_t idx, uint32_t cmd_offset, uint32_t bo_offset, uint32_t type, uint32_t pad)` |
| 303 | `static void` | **mwv207_drm_cmdbuf_print** | `mwv207_drm_cmdbuf_print(struct mwv207_winsys *ws, struct mwv207_cmd *cmd)` |
| 372 | `static void` | **mwv207_drm_cmdbuf_commit** | `mwv207_drm_cmdbuf_commit(struct mwv207_winsys *ws, struct mwv207_cmd *cmd, int in_fence_fd, int *out_fence_fd)` |

### `winsys/mwv207/drm/mwv207_drm_winsys.c`  (6 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 76 | `static void` | **mwv207_cluster_init** | `mwv207_cluster_init(struct mwv207_winsys *ws)` |
| 108 | `static uint32_t` | **hash_uint32_t** | `hash_uint32_t(const void *key)` |
| 114 | `static bool` | **handle_equals** | `handle_equals(const void *key1, const void *key2)` |
| 120 | `static bool` | **mwv207_ws_init** | `mwv207_ws_init(struct mwv207_winsys *ws)` |
| 151 | `static bool` | **mwv207_drm_destroy** | `mwv207_drm_destroy(struct mwv207_winsys *ws)` |
| 209 | `struct pipe_screen *` | **mwv207_drm_create_screen** | `mwv207_drm_create_screen(int fd)` |

### `winsys/mwv207/drm/mwv207_drm_winsys.h`  (3 个)

| 行 | 返回类型 | 函数 | 签名 |
|---:|---|---|---|
| 74 | `static inline enum mwv207_bo_domain` | **mwv207_domain_from_heap** | `mwv207_domain_from_heap(enum mwv207_heap heap)` |
| 88 | `static inline enum mwv207_bo_flag` | **mwv207_flags_from_heap** | `mwv207_flags_from_heap(enum mwv207_heap heap)` |
| 102 | `static inline enum mwv207_heap` | **mwv207_get_heap_index** | `mwv207_get_heap_index(enum mwv207_bo_domain domain, enum mwv207_bo_flag flags)` |

## 结构体 / 联合体 / 枚举定义

| 文件 | 行 | 定义 |
|---|---:|---|
| `drivers/mwv207/include/mwv207_blit.h` | 30 | `struct mwv207_task_info` |
| `drivers/mwv207/include/mwv207_blit.h` | 41 | `struct mwv207_clear_info` |
| `drivers/mwv207/include/mwv207_blit.h` | 52 | `struct mwv207_region_info` |
| `drivers/mwv207/include/mwv207_cmd.h` | 40 | `enum cache_mask` |
| `drivers/mwv207/include/mwv207_context.h` | 53 | `enum mwv207_dirty` |
| `drivers/mwv207/include/mwv207_context.h` | 78 | `struct mwv207_dual_source_blend_info` |
| `drivers/mwv207/include/mwv207_context.h` | 85 | `struct mwv207_pipe_info` |
| `drivers/mwv207/include/mwv207_context.h` | 109 | `struct mwv207_context` |
| `drivers/mwv207/include/mwv207_format.h` | 67 | `struct mwv207_blit_Info` |
| `drivers/mwv207/include/mwv207_format.h` | 73 | `struct mwv207_hardware_Info` |
| `drivers/mwv207/include/mwv207_format.h` | 79 | `struct mwv207_blit_desc` |
| `drivers/mwv207/include/mwv207_format.h` | 89 | `struct mwv207_pixel_desc` |
| `drivers/mwv207/include/mwv207_format.h` | 98 | `struct mwv207_texture_config` |
| `drivers/mwv207/include/mwv207_format.h` | 125 | `struct mwv207_texture_desc` |
| `drivers/mwv207/include/mwv207_query.h` | 33 | `struct mwv207_query_info` |
| `drivers/mwv207/include/mwv207_query.h` | 39 | `struct mwv207_query` |
| `drivers/mwv207/include/mwv207_resource.h` | 49 | `enum mwv207_tile_mode` |
| `drivers/mwv207/include/mwv207_resource.h` | 55 | `struct mwv207_transfer` |
| `drivers/mwv207/include/mwv207_resource.h` | 60 | `struct mwv207_tile_info` |
| `drivers/mwv207/include/mwv207_resource.h` | 69 | `struct mwv207_level_info` |
| `drivers/mwv207/include/mwv207_resource.h` | 91 | `struct mwv207_sample_info` |
| `drivers/mwv207/include/mwv207_resource.h` | 99 | `struct mwv207_resource` |
| `drivers/mwv207/include/mwv207_screen.h` | 39 | `struct mwv207_screen` |
| `drivers/mwv207/include/mwv207_shader.h` | 42 | `struct mwv207_shader_state` |
| `drivers/mwv207/include/mwv207_state.h` | 35 | `struct mwv207_element` |
| `drivers/mwv207/include/mwv207_state.h` | 43 | `struct mwv207_elements_state` |
| `drivers/mwv207/include/mwv207_state.h` | 48 | `struct mwv207_rasterizer_state` |
| `drivers/mwv207/include/mwv207_state.h` | 61 | `struct mwv207_blend_state` |
| `drivers/mwv207/include/mwv207_state.h` | 75 | `struct mwv207_depth_stencil_alpha_state` |
| `drivers/mwv207/include/mwv207_state.h` | 95 | `struct mwv207_stencil_ref` |
| `drivers/mwv207/include/mwv207_state.h` | 101 | `struct mwv207_stream_output_target` |
| `drivers/mwv207/include/mwv207_state.h` | 107 | `struct mwv207_global_bindings_state` |
| `drivers/mwv207/include/mwv207_state.h` | 112 | `struct mwv207_stream_output` |
| `drivers/mwv207/include/mwv207_surface.h` | 35 | `struct mwv207_tex_info` |
| `drivers/mwv207/include/mwv207_surface.h` | 44 | `struct mwv207_pixel_info` |
| `drivers/mwv207/include/mwv207_surface.h` | 59 | `struct mwv207_surface` |
| `drivers/mwv207/include/mwv207_surface.h` | 70 | `struct mwv207_framebuffer_state` |
| `drivers/mwv207/include/mwv207_surface.h` | 76 | `struct mwv207_sampler_desc` |
| `drivers/mwv207/include/mwv207_surface.h` | 87 | `struct mwv207_sampler` |
| `drivers/mwv207/include/mwv207_surface.h` | 92 | `struct mwv207_sampler_view` |
| `drivers/mwv207/nir/mwv207_common.h` | 78 | `enum mwv207_pass_flags` |
| `drivers/mwv207/nir/mwv207_common.h` | 133 | `enum mwv207_immediate_contents` |
| `drivers/mwv207/nir/mwv207_common.h` | 158 | `struct mwv207_inst_dst` |
| `drivers/mwv207/nir/mwv207_common.h` | 167 | `struct mwv207_inst_tex` |
| `drivers/mwv207/nir/mwv207_common.h` | 193 | `struct mwv207_inst_src` |
| `drivers/mwv207/nir/mwv207_common.h` | 213 | `struct mwv207_inst` |
| `drivers/mwv207/nir/mwv207_common.h` | 238 | `struct mwv207_op_info` |
| `drivers/mwv207/nir/mwv207_common.h` | 249 | `struct live_def` |
| `drivers/mwv207/nir/mwv207_common.h` | 255 | `struct live_defs_state` |
| `drivers/mwv207/nir/mwv207_compiler.h` | 33 | `struct mwv207_shader_key` |
| `drivers/mwv207/nir/mwv207_compiler.h` | 65 | `struct mwv207_shader_inout` |
| `drivers/mwv207/nir/mwv207_compiler.h` | 75 | `struct mwv207_shader_io_file` |
| `drivers/mwv207/nir/mwv207_compiler.h` | 80 | `struct mwv207_shader_uniform_info` |
| `drivers/mwv207/nir/mwv207_compiler.h` | 87 | `struct mwv207_varying` |
| `drivers/mwv207/nir/mwv207_compiler.h` | 93 | `struct mwv207_shader_link_info` |
| `drivers/mwv207/nir/mwv207_compiler.h` | 102 | `struct mwv207_shader` |
| `drivers/mwv207/nir/mwv207_compiler.h` | 154 | `struct mwv207_compile` |
| `drivers/mwv207/nir/mwv207_disasm.h` | 33 | `enum dst_addr_reg_type` |
| `drivers/mwv207/nir/mwv207_disasm.h` | 39 | `enum kinds_of_inst` |
| `drivers/mwv207/nir/mwv207_disasm.h` | 72 | `struct instr` |
| `drivers/mwv207/nir/mwv207_disasm.h` | 262 | `struct mwv207_inst_ctrl` |
| `drivers/mwv207/nir/mwv207_lower_64bit.c` | 31 | `struct variable_pair` |
| `drivers/mwv207/nir/mwv207_lower_64bit.c` | 36 | `struct inst64` |
| `drivers/mwv207/video-codec/common/mwv207_codec_time.h` | 37 | `struct mwv207_codec_time` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 139 | `enum mwv207_block_size_type` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 156 | `enum mwv207_partition_type` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 167 | `enum mwv207_vp9_frame_type` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 173 | `enum mwv207_mb_prediction_mode` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 204 | `enum mwv207_block_prediction_mode` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 234 | `enum mwv207_interpolation_filter_type` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 245 | `enum mwv207_comp_pred_mode_type` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 252 | `enum mwv207_txfm_mode` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 261 | `enum mwv207_mv_reference_frame` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 270 | `enum mwv207_seg_level_features` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 283 | `enum mwv207_tx_size` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 291 | `enum mwv207_split_mv_partitioning_type` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 299 | `enum mwv207_pred_id` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 311 | `enum mwv207_mv_joint_type` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 318 | `enum mwv207_mv_class_type` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 332 | `struct mwv207_nmv_context` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 347 | `struct mwv207_mmv_context_counts` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 366 | `struct mwv207_vp9_adaptive_entropy_probs` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 404 | `struct mwv207_vp9_entropy_probs` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 428 | `struct mwv207_vp9_entropy_counts` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_slice_header.h` | 51 | `struct mwv207_st_ref_pic_elem` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_slice_header.h` | 55 | `struct mwv207_st_ref_pic_set` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_slice_header.h` | 60 | `struct mwv207_hevc_slice_header` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_stream.h` | 33 | `struct mwv207_strm_data` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 44 | `struct mwv207_strm_info` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_vp9_slice_header.c` | 476 | `struct vp9_token` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_vp9_slice_header.h` | 31 | `enum Vp9ColorSpace` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_vp9_slice_header.h` | 686 | `struct mwv207_vp9_slice_header` |
| `drivers/mwv207/video-codec/common/mwv207_video_cs.h` | 34 | `struct mwv207_video_cs` |
| `drivers/mwv207/video-codec/common/mwv207_video_enums.h` | 35 | `enum mwv207_video_format` |
| `drivers/mwv207/video-codec/common/mwv207_video_enums.h` | 46 | `enum mwv207_video_profile` |
| `drivers/mwv207/video-codec/common/mwv207_video_enums.h` | 76 | `enum mwv207_video_cap` |
| `drivers/mwv207/video-codec/common/mwv207_video_enums.h` | 89 | `enum mwv207_video_entrypoint` |
| `drivers/mwv207/video-codec/common/mwv207_video_info.h` | 32 | `enum mwv207_video_format` |
| `drivers/mwv207/video-codec/common/mwv207_video_info.h` | 54 | `struct mwv207_video_mvc_element` |
| `drivers/mwv207/video-codec/common/mwv207_video_info.h` | 67 | `struct mwv207_video_h264` |
| `drivers/mwv207/video-codec/common/mwv207_video_info.h` | 119 | `struct mwv207_video_h265` |
| `drivers/mwv207/video-codec/common/mwv207_video_info.h` | 188 | `struct mwv207_video_vc1` |
| `drivers/mwv207/video-codec/common/mwv207_video_info.h` | 197 | `struct mwv207_video_mpeg2` |
| `drivers/mwv207/video-codec/common/mwv207_video_info.h` | 225 | `struct mwv207_video_mpeg4` |
| `drivers/mwv207/video-codec/common/mwv207_video_reg_buf.h` | 36 | `struct mwv207_video_bo` |
| `drivers/mwv207/video-codec/common/mwv207_video_reg_buf.h` | 45 | `struct mwv207_video_reg_buf` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 43 | `enum mwv207_mpeg12_picture_coding_type` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 53 | `enum mwv207_mpeg12_picture_structure` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 63 | `enum mwv207_mpeg12_macroblock_type` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 74 | `enum mwv207_mpeg12_motion_type` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 85 | `enum mwv207_mpeg12_dct_type` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 90 | `enum mwv207_mpeg12_field_select` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 97 | `enum mwv207_h264_slice_type` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 105 | `enum mwv207_h264_enc_picture_type` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 113 | `enum mwv207_h265_enc_picture_type` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 121 | `enum mwv207_h264_enc_rate_control_method` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 129 | `enum mwv207_h265_enc_rate_control_method` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 137 | `struct mwv207_picture_desc` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 142 | `struct mwv207_quant_matrix` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 146 | `struct mwv207_macroblock` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 150 | `struct mwv207_mpeg12_picture_desc` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 173 | `struct mwv207_mpeg12_macroblock` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 203 | `struct mwv207_mpeg4_picture_desc` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 227 | `struct mwv207_vc1_picture_desc` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 263 | `struct mwv207_h264_sps` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 286 | `struct mwv207_h264_pps` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 309 | `struct mwv207_picture_h264` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 319 | `struct mwv207_h264_picture_desc` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 352 | `struct mwv207_h264_enc_rate_control` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 367 | `struct mwv207_h264_enc_motion_estimation` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 377 | `struct mwv207_h264_enc_pic_control` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 387 | `struct mwv207_h264_enc_picture_desc` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 418 | `struct mwv207_h265_enc_seq_param` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 441 | `struct mwv207_h265_enc_pic_param` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 447 | `struct mwv207_h265_enc_slice_param` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 458 | `struct mwv207_h265_enc_rate_control` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 474 | `struct mwv207_h265_enc_picture_desc` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 494 | `struct mwv207_h265_sps` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 531 | `struct mwv207_h265_pps` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 571 | `struct mwv207_picture_h265` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 583 | `struct mwv207_h265_picture_desc` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 614 | `struct mwv207_mjpeg_picture_desc` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 668 | `struct mwv207_vp9_segment_parameter` |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 690 | `struct mwv207_vp9_picture_desc` |
| `drivers/mwv207/video-codec/common/mwv207_vp9hwd_bool.h` | 43 | `struct mwv207_vp_bool_coder` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 178 | `struct DecHwConfig` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 227 | `struct DecSwHwBuild` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 233 | `enum DecDpbFlags` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 244 | `enum DecDpbMode` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 246 | `enum DecDecoderMode` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 254 | `enum mwv207_delogo_mode` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 262 | `enum mwv207_dec_picture_format` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 410 | `enum DecPicCodingType` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 419 | `enum DecPicturePixelFormat` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 432 | `enum DecErrorHandling` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 457 | `struct mwv207_dec_crop_cfg` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 465 | `enum mwv207_scale_mode` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 467 | `struct mwv207_dec_fixed_scale_cfg` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_hevc.c` | 52 | `struct RefPicDeltaPoc` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.h` | 105 | `struct mwv207_pp_unit_regs` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec.h` | 66 | `struct mwv207_video_dec_param` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec.h` | 119 | `struct mwv207_video_dec` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_reg.h` | 75 | `enum mwv207_9200_hw_if_name` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_vpufeature.h` | 33 | `enum mwv207_vpu_release_ver` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_vpufeature.h` | 57 | `struct mwv207_video_dec_hw_features` |
| `drivers/mwv207/video-codec/mwv207_video_buffer.h` | 43 | `struct mwv207_offset` |
| `drivers/mwv207/video-codec/mwv207_video_buffer.h` | 71 | `struct mwv207_video_buffer` |
| `drivers/mwv207/video-codec/mwv207_video_decoder.h` | 47 | `struct mwv207_video_decoder` |
| `winsys/mwv207/drm/mwv207_drm.h` | 51 | `struct drm_mwv207_gem_create_in` |
| `winsys/mwv207/drm/mwv207_drm.h` | 57 | `struct drm_mwv207_gem_create_out` |
| `winsys/mwv207/drm/mwv207_drm.h` | 61 | `union drm_mwv207_gem_create` |
| `winsys/mwv207/drm/mwv207_drm.h` | 66 | `struct drm_mwv207_gem_mmap_in` |
| `winsys/mwv207/drm/mwv207_drm.h` | 70 | `struct drm_mwv207_gem_mmap_out` |
| `winsys/mwv207/drm/mwv207_drm.h` | 73 | `union drm_mwv207_gem_mmap` |
| `winsys/mwv207/drm/mwv207_drm.h` | 80 | `struct drm_mwv207_gem_wait` |
| `winsys/mwv207/drm/mwv207_drm.h` | 89 | `struct drm_mwv207_ctx` |
| `winsys/mwv207/drm/mwv207_drm.h` | 114 | `struct drm_mwv207_submit` |
| `winsys/mwv207/drm/mwv207_drm.h` | 138 | `struct drm_mwv207_submit_syncobj` |
| `winsys/mwv207/drm/mwv207_drm.h` | 146 | `struct drm_mwv207_bo_acc` |
| `winsys/mwv207/drm/mwv207_drm.h` | 156 | `struct drm_mwv207_submit_reloc` |
| `winsys/mwv207/drm/mwv207_drm.h` | 165 | `struct drm_mwv207_submit_cmd_dat` |
| `winsys/mwv207/drm/mwv207_drm.h` | 176 | `enum drm_mwv207_info_key` |
| `winsys/mwv207/drm/mwv207_drm.h` | 184 | `struct drm_mwv207_info_rec` |
| `winsys/mwv207/drm/mwv207_drm.h` | 191 | `struct drm_mwv207_info` |
| `winsys/mwv207/drm/mwv207_drm_cs.c` | 40 | `struct pipe_fence_handle` |
| `winsys/mwv207/drm/mwv207_drm_winsys.h` | 52 | `enum mwv207_bo_domain` |
| `winsys/mwv207/drm/mwv207_drm_winsys.h` | 58 | `enum mwv207_bo_flag` |
| `winsys/mwv207/drm/mwv207_drm_winsys.h` | 66 | `enum mwv207_heap` |
| `winsys/mwv207/drm/mwv207_drm_winsys.h` | 111 | `struct mwv207_bo` |
| `winsys/mwv207/drm/mwv207_drm_winsys.h` | 138 | `struct mwv207_slab` |
| `winsys/mwv207/drm/mwv207_drm_winsys.h` | 144 | `struct cmd_dat` |
| `winsys/mwv207/drm/mwv207_drm_winsys.h` | 153 | `struct cmd_dat_arrary` |
| `winsys/mwv207/drm/mwv207_drm_winsys.h` | 163 | `struct mwv207_cmd` |
| `winsys/mwv207/drm/mwv207_drm_winsys.h` | 188 | `struct mwv207_winsys` |

## 函数宏

| 文件 | 行 | 宏 | 定义 |
|---|---:|---|---|
| `drivers/mwv207/include/mwv207_cmd.h` | 66 | `ALIGN_BIT` | `#define ALIGN_BIT(data, offset) (((uint32_t) (data)) << (offset))` |
| `drivers/mwv207/include/mwv207_cmd.h` | 68 | `VALUE` | `#define VALUE(reg, field, value)  (ALIGN_BIT((uint32_t) (value), reg##_##field##_Offset) & (reg##_##field##_Ma` |
| `drivers/mwv207/include/mwv207_cmd.h` | 70 | `NAME` | `#define NAME(reg, field, value)  (ALIGN_BIT(reg##_##field##_##value, reg##_##field##_Offset) & (reg##_##field#` |
| `drivers/mwv207/include/mwv207_cmd.h` | 72 | `MASK` | `#define MASK(reg, field)  (~(reg##_##field##_Mask \| reg##_MASK_##field##_Mask))` |
| `drivers/mwv207/include/mwv207_cmd.h` | 74 | `MASK_VALUE` | `#define MASK_VALUE(reg, field, value) \` |
| `drivers/mwv207/include/mwv207_cmd.h` | 80 | `MASK_NAME` | `#define MASK_NAME(reg, field, value) \` |
| `drivers/mwv207/include/mwv207_cmd.h` | 86 | `MWV207_STALL` | `#define MWV207_STALL(cmd, value) \` |
| `drivers/mwv207/include/mwv207_cmd.h` | 92 | `MWV207_EMIT` | `#define MWV207_EMIT(cmd, reg, value) \` |
| `drivers/mwv207/include/mwv207_cmd.h` | 98 | `MWV207_EMITF` | `#define MWV207_EMITF(cmd, reg, value) \` |
| `drivers/mwv207/include/mwv207_cmd.h` | 104 | `MWV207_EMITS` | `#define MWV207_EMITS(cmd, reg, count, value) \` |
| `drivers/mwv207/include/mwv207_cmd.h` | 112 | `MWV207_EMITFS` | `#define MWV207_EMITFS(cmd, reg, count, value) \` |
| `drivers/mwv207/include/mwv207_cmd.h` | 120 | `MWV207_SNAP_TO_PAGE` | `#define MWV207_SNAP_TO_PAGE(cmd)\` |
| `drivers/mwv207/include/mwv207_cmd.h` | 126 | `MWV207_BLIT_LOCK` | `#define MWV207_BLIT_LOCK(ctx, cmd_buf) \` |
| `drivers/mwv207/include/mwv207_cmd.h` | 132 | `MWV207_BLIT_UNLOCK` | `#define MWV207_BLIT_UNLOCK(ctx, cmd_buf) \` |
| `drivers/mwv207/include/mwv207_format.h` | 34 | `BLT_SWIZZLE_EX` | `#define BLT_SWIZZLE_EX(r, g, b, a) \` |
| `drivers/mwv207/include/mwv207_format.h` | 40 | `SWIZZLE_SETS` | `#define SWIZZLE_SETS(r, g, b, a) {r, g, b ,a }` |
| `drivers/mwv207/include/mwv207_format.h` | 42 | `SWIZZLE_STATE` | `#define SWIZZLE_STATE(incoming_swizzle) \` |
| `drivers/mwv207/include/mwv207_format.h` | 54 | `TX_FORMAT` | `#define TX_FORMAT(oldFormat, newFormat) \` |
| `drivers/mwv207/include/mwv207_resource.h` | 44 | `ALIGN_NP2` | `#define ALIGN_NP2(n, align) (((n) + (align) - 1) - (((n) + (align) - 1) % (align)))` |
| `drivers/mwv207/include/mwv207_screen.h` | 33 | `RET` | `#define RET(x) do {                  \` |
| `drivers/mwv207/nir/mwv207_common.h` | 47 | `INST_SWIZ_X` | `#define INST_SWIZ_X(x)                 (((x) << INST_SWIZ_X__SHIFT) & INST_SWIZ_X__MASK)` |
| `drivers/mwv207/nir/mwv207_common.h` | 50 | `INST_SWIZ_Y` | `#define INST_SWIZ_Y(x)                 (((x) << INST_SWIZ_Y__SHIFT) & INST_SWIZ_Y__MASK)` |
| `drivers/mwv207/nir/mwv207_common.h` | 53 | `INST_SWIZ_Z` | `#define INST_SWIZ_Z(x)                 (((x) << INST_SWIZ_Z__SHIFT) & INST_SWIZ_Z__MASK)` |
| `drivers/mwv207/nir/mwv207_common.h` | 56 | `INST_SWIZ_W` | `#define INST_SWIZ_W(x)                 (((x) << INST_SWIZ_W__SHIFT) & INST_SWIZ_W__MASK)` |
| `drivers/mwv207/nir/mwv207_common.h` | 58 | `INST_SWIZ_BROADCAST` | `#define INST_SWIZ_BROADCAST(x)         (INST_SWIZ_X(x) \| INST_SWIZ_Y(x) \| INST_SWIZ_Z(x) \| INST_SWIZ_W(x))` |
| `drivers/mwv207/nir/mwv207_common.h` | 62 | `INST_SWIZ` | `#define INST_SWIZ(x,y,z,w)         (INST_SWIZ_X(x) \| INST_SWIZ_Y(y) \| INST_SWIZ_Z(z) \| INST_SWIZ_W(w))` |
| `drivers/mwv207/nir/mwv207_common.h` | 63 | `SWIZZLE` | `#define SWIZZLE(c0,c1,c2,c3) \` |
| `drivers/mwv207/nir/mwv207_common.h` | 69 | `COND` | `#define COND(bool, val) ((bool) ? (val) : 0)` |
| `drivers/mwv207/nir/mwv207_common.h` | 71 | `VS_BALANCE` | `#define VS_BALANCE(A) (((20480 / (0x400 - 2 * A * 0x10)) + 9) / 10)` |
| `drivers/mwv207/nir/mwv207_common.h` | 73 | `compile_error` | `#define compile_error(ctx, args...) ({ \` |
| `drivers/mwv207/nir/mwv207_compiler.h` | 174 | `OPT` | `#define OPT(nir, pass, ...) ({                             \` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1022 | `OPC_NO_OPND` | `#define OPC_NO_OPND(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, NO_OPND_INST, (flags), 0}` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1023 | `OPC_3_SRCS_CC_ALU` | `#define OPC_3_SRCS_CC_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, TRI_SRCS_CC_ALU_INST, (flags)` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1024 | `OPC_3_SRCS_ALU` | `#define OPC_3_SRCS_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, TRI_SRCS_ALU_INST, (flags), (0x1` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1025 | `OPC_2_SRCS_SRC0_SRC1_ALU` | `#define OPC_2_SRCS_SRC0_SRC1_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, BIN_SRCS_SRC0_SRC1_ALU` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1026 | `OPC_2_SRCS_SRC0_SRC1_CC_ALU` | `#define OPC_2_SRCS_SRC0_SRC1_CC_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, BIN_SRCS_SRC0_SRC1_` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1027 | `OPC_2_SRCS_SRC0_SRC2_ALU` | `#define OPC_2_SRCS_SRC0_SRC2_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, BIN_SRCS_SRC0_SRC2_ALU` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1028 | `OPC_2_SRCS_SRC1_SRC2_ALU` | `#define OPC_2_SRCS_SRC1_SRC2_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, BIN_SRCS_SRC1_SRC2_ALU` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1029 | `OPC_1_SRC_SRC0_ALU` | `#define OPC_1_SRC_SRC0_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, UNA_SRC_SRC0_ALU_INST, (flag` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1030 | `OPC_1_SRC_SRC1_ALU` | `#define OPC_1_SRC_SRC1_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, UNA_SRC_SRC1_ALU_INST, (flag` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1031 | `OPC_1_SRC_SRC2_ALU` | `#define OPC_1_SRC_SRC2_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, UNA_SRC_SRC2_ALU_INST, (flag` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1032 | `OPC_PACK` | `#define OPC_PACK(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, PACK_INST, (flags), (0x1 \| 0x2 \| 0x4)}` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1033 | `OPC_SAMPLE` | `#define OPC_SAMPLE(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, SAMPLE_INST, (flags), (0x1 \| 0x2 \| 0` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1034 | `OPC_LOAD` | `#define OPC_LOAD(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, LOAD_INST, (flags), (0x1 \| 0x2)}` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1035 | `OPC_IMG_LOAD` | `#define OPC_IMG_LOAD(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, IMG_LOAD_INST, (1U<<2) \| (flags), ` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1036 | `OPC_STORE` | `#define OPC_STORE(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, STORE_INST, (flags), (0x1 \| 0x2 \| 0x4` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1037 | `OPC_IMG_STORE` | `#define OPC_IMG_STORE(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, IMG_STORE_INST, (1U<<2) \| (flags)` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1038 | `OPC_IMG_ATOM` | `#define OPC_IMG_ATOM(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, IMG_ATOM_INST, (1U<<2) \| (flags), ` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1039 | `OPC_STORE_ATTR` | `#define OPC_STORE_ATTR(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, STORE_ATTR_INST, (flags), (0x1 \|` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1040 | `OPC_SELECT_MAP` | `#define OPC_SELECT_MAP(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, SELECT_MAP_INST, (flags), (0x1 \|` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1041 | `OPC_BRANCH` | `#define OPC_BRANCH(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_branch, DIRECT_BRANCH0_INST, (flags), (0x1` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1042 | `OPC_CALL` | `#define OPC_CALL(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_call, DIRECT_CALL_INST, (flags), (0x4)}` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1043 | `OPC_LOOP` | `#define OPC_LOOP(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, LOOP_INST, (flags), (0x2)}` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1044 | `OPC_CONV` | `#define OPC_CONV(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, CONV_INST, (flags), (0x2 \| 0x4)}` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1045 | `OPC_SCATTER` | `#define OPC_SCATTER(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, SCATTER_INST, (flags), (0x2 \| 0x4)}` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1046 | `OPC_CMPLX` | `#define OPC_CMPLX(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, CMPLX_INST, (flags), (0x1 \| 0x2 \| 0x4` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1047 | `OPC_CLAMP0_MAX` | `#define OPC_CLAMP0_MAX(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_clamp0_max, BIN_SRCS_SRC0_SRC1_ALU_CC_` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1048 | `OPC_EXT_EMIT` | `#define OPC_EXT_EMIT(opc, flags) [0x7f + INST_EXT_OPCODE_##opc] = {#opc, convert_inst, EMIT_INST, (flags), (0x` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1049 | `OPC_EXT_2_SRCS_SRC0_SRC1_ALU` | `#define OPC_EXT_2_SRCS_SRC0_SRC1_ALU(opc, flags) [0x7f + INST_EXT_OPCODE_##opc] = {#opc, convert_inst, BIN_SRC` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1050 | `OPC_EXT_1_SRC_SRC0_ALU` | `#define OPC_EXT_1_SRC_SRC0_ALU(opc, flags) [0x7f + INST_EXT_OPCODE_##opc] = {#opc, convert_inst, UNA_SRC_SRC0_` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1051 | `OPC_SAMPLE_EXT` | `#define OPC_SAMPLE_EXT(opc, flags) [0x7f + INST_EXT_OPCODE_##opc] = {#opc, convert_inst, SAMPLE_EXT_INST, (fla` |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1052 | `OPC_EXT_LOAD` | `#define OPC_EXT_LOAD(opc, flags) [0x7f + INST_EXT_OPCODE_##opc] = {#opc, convert_inst, LOAD_INST, (flags), (0x` |
| `drivers/mwv207/nir/mwv207_disasm.h` | 31 | `PRINT` | `#define PRINT(format, ...) print_with_update_length(print_length, format, ##__VA_ARGS__)` |
| `drivers/mwv207/nir/mwv207_emit.h` | 32 | `emit` | `#define emit(type, args...) mwv207_emit_##type(c, args)` |
| `drivers/mwv207/nir/mwv207_emit.h` | 34 | `DST_REG` | `#define DST_REG(idx, m)   ((struct mwv207_inst_dst){.use = 1, .reg = idx, .write_mask = m})` |
| `drivers/mwv207/nir/mwv207_emit.h` | 35 | `ALU_SWIZ` | `#define ALU_SWIZ(s)       INST_SWIZ((s)->swizzle[0], (s)->swizzle[1], (s)->swizzle[2], (s)->swizzle[3])` |
| `drivers/mwv207/nir/mwv207_emit.h` | 37 | `SRC_CONST` | `#define SRC_CONST(idx, s) ((struct mwv207_inst_src){.use=1, .rgroup = 0x2, .reg=idx, .swiz=s})` |
| `drivers/mwv207/nir/mwv207_emit.h` | 38 | `SRC_REG` | `#define SRC_REG(idx, s)   ((struct mwv207_inst_src){.use=1, .rgroup = 0x0, .reg=idx, .swiz=s})` |
| `drivers/mwv207/nir/mwv207_emit.h` | 40 | `CONST_VAL` | `#define CONST_VAL(a, b) (nir_const_value) {.u64 = (uint64_t)(a) << 32 \| (uint64_t)(b)}` |
| `drivers/mwv207/nir/mwv207_emit.h` | 41 | `CONST` | `#define CONST(x)        CONST_VAL(MWV207_IMMEDIATE_CONSTANT, x)` |
| `drivers/mwv207/nir/mwv207_emit.h` | 42 | `UNIFORM` | `#define UNIFORM(x)      CONST_VAL(MWV207_IMMEDIATE_UNIFORM, x)` |
| `drivers/mwv207/nir/mwv207_emit.h` | 43 | `TEXSCALE` | `#define TEXSCALE(x, i)  CONST_VAL(MWV207_IMMEDIATE_TEXRECT_SCALE_X + (i), x)` |
| `drivers/mwv207/nir/mwv207_emit.h` | 44 | `TEXSIZE` | `#define TEXSIZE(x, i)   CONST_VAL(MWV207_UNIFORM_TEXTURE_WIDTH + (i), x)` |
| `drivers/mwv207/nir/mwv207_lower.c` | 1532 | `do_by_default` | `#define do_by_default(OPT) (debug_get_bool_option("OPT_"#OPT, false))` |
| `drivers/mwv207/nir/mwv207_opcode.c` | 42 | `OPCT` | `#define OPCT(nir, op, src, cond, type, round, eop) [nir_op_##nir] = { \` |
| `drivers/mwv207/nir/mwv207_opcode.c` | 50 | `OPC` | `#define OPC(nir, op, src, cond) OPCT(nir, op, src, cond, F32, DEFAULT, NONE)` |
| `drivers/mwv207/nir/mwv207_opcode.c` | 51 | `IOPC` | `#define IOPC(nir, op, src, cond) OPCT(nir, op, src, cond, S32, DEFAULT, NONE)` |
| `drivers/mwv207/nir/mwv207_opcode.c` | 52 | `UOPC` | `#define UOPC(nir, op, src, cond) OPCT(nir, op, src, cond, U32, DEFAULT, NONE)` |
| `drivers/mwv207/nir/mwv207_opcode.c` | 53 | `U16_OPC` | `#define U16_OPC(nir, op, src, cond) OPCT(nir, op, src, cond, U16, DEFAULT, NONE)` |
| `drivers/mwv207/nir/mwv207_opcode.c` | 54 | `I16_OPC` | `#define I16_OPC(nir, op, src, cond) OPCT(nir, op, src, cond, S16, DEFAULT, NONE)` |
| `drivers/mwv207/nir/mwv207_opcode.c` | 55 | `EIOPC` | `#define EIOPC(nir, eop, src, cond) OPCT(nir, EXTENDED, src, cond, S32, DEFAULT, eop)` |
| `drivers/mwv207/nir/mwv207_opcode.c` | 56 | `EUOPC` | `#define EUOPC(nir, eop, src, cond) OPCT(nir, EXTENDED, src, cond, U32, DEFAULT, eop)` |
| `drivers/mwv207/nir/mwv207_opcode.c` | 57 | `OP` | `#define OP(nir, op, src) OPC(nir, op, src, MWV207_TRUE)` |
| `drivers/mwv207/nir/mwv207_opcode.c` | 58 | `IOP` | `#define IOP(nir, op, src) IOPC(nir, op, src, MWV207_TRUE)` |
| `drivers/mwv207/nir/mwv207_opcode.c` | 59 | `UOP` | `#define UOP(nir, op, src) UOPC(nir, op, src, MWV207_TRUE)` |
| `drivers/mwv207/nir/mwv207_opcode.c` | 60 | `EIOP` | `#define EIOP(nir, eop, src) EIOPC(nir, eop, src, MWV207_TRUE)` |
| `drivers/mwv207/nir/mwv207_opcode.c` | 61 | `EUOP` | `#define EUOP(nir, eop, src) EUOPC(nir, eop, src, MWV207_TRUE)` |
| `drivers/mwv207/nir/mwv207_opcode.c` | 62 | `U16_OP` | `#define U16_OP(nir, op, src) U16_OPC(nir, op, src, MWV207_TRUE)` |
| `drivers/mwv207/nir/mwv207_opcode.c` | 63 | `I16_OP` | `#define I16_OP(nir, op, src) I16_OPC(nir, op, src, MWV207_TRUE)` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 361 | `VP9HWPAD` | `#define VP9HWPAD(x, y) uint8_t x[y]` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_h264_nal_unit.h` | 44 | `IS_IDR_NAL_UNIT` | `#define IS_IDR_NAL_UNIT(p_nal_unit)                                          \` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_h264_slice_header.h` | 48 | `IS_P_SLICE` | `#define IS_P_SLICE(slice_type)                                               \` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_h264_slice_header.h` | 51 | `IS_SP_SLICE` | `#define IS_SP_SLICE(slice_type)                                              \` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_h264_slice_header.h` | 54 | `IS_I_SLICE` | `#define IS_I_SLICE(slice_type)                                               \` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_h264_slice_header.h` | 57 | `IS_B_SLICE` | `#define IS_B_SLICE(slice_type)                                               \` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_slice_header.h` | 89 | `IS_HEVC_P_SLICE` | `#define IS_HEVC_P_SLICE(slice_type)                                          \` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_slice_header.h` | 92 | `IS_HEVC_I_SLICE` | `#define IS_HEVC_I_SLICE(slice_type)                                          \` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_slice_header.h` | 95 | `IS_HEVC_B_SLICE` | `#define IS_HEVC_B_SLICE(slice_type)                                          \` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_stream.c` | 30 | `ASSERT` | `#define ASSERT(expr) assert(expr)` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.c` | 30 | `ASSERT` | `#define ASSERT(expr) assert(expr)` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 35 | `SLIECE_DEBUG` | `#define SLIECE_DEBUG(fmt, args...)                                           \` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 38 | `SLIECE_DEBUG` | `#define SLIECE_DEBUG(fmt, args...)` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 62 | `MIN` | `#define MIN(a, b) (((a) < (b)) ? (a) : (b))` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 64 | `MAX` | `#define MAX(a, b) (((a) > (b)) ? (a) : (b))` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 66 | `ABS` | `#define ABS(a) (((a) < 0) ? -(a) : (a))` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 68 | `CLIP3` | `#define CLIP3(x, y, z) (((z) < (x)) ? (x) : (((z) > (y)) ? (y) : (z)))` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 70 | `CLIP1` | `#define CLIP1(z) (((z) < 0) ? 0 : (((z) > 255) ? 255 : (z)))` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 72 | `ALLOCATE` | `#define ALLOCATE(ptr, count, type)                                           \` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 77 | `FREE` | `#define FREE(ptr)                                                            \` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 85 | `SW_CHECK_SYNTAX_ELEMENT` | `#define SW_CHECK_SYNTAX_ELEMENT(rv, elem)                                    \` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 91 | `SW_CHECK_END_OF_STREAM` | `#define SW_CHECK_END_OF_STREAM(rv, elem)                                     \` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 97 | `NEXT_MULTIPLE` | `#define NEXT_MULTIPLE(value, n) (((value) + (n)-1) & ~((n)-1))` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 99 | `MAJOR_VERSION` | `#define MAJOR_VERSION(hw_id) (((hw_id)&0xF000) >> 12)` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 100 | `MINOR_VERSION` | `#define MINOR_VERSION(hw_id) (((hw_id)&0x0FF0) >> 4)` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 102 | `IS_G1_LEGACY` | `#define IS_G1_LEGACY(hw_id) (((hw_id) >> 16) == 0x6731)` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 103 | `IS_G2_LEGACY` | `#define IS_G2_LEGACY(hw_id) (((hw_id) >> 16) == 0x6732)` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 105 | `IS_LEGACY` | `#define IS_LEGACY(hw_id)                                                     \` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 107 | `IS_JMDEC` | `#define IS_JMDEC(asic_id) (((asic_id) >> 16) == 0x8001)` |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 108 | `IS_KMB_A0` | `#define IS_KMB_A0(asic_id) (((asic_id)&0xffff) == 0x6010)` |
| `drivers/mwv207/video-codec/common/mwv207_log.h` | 34 | `ASSERT` | `#define ASSERT(expr) assert(expr)` |
| `drivers/mwv207/video-codec/common/mwv207_log.h` | 52 | `MWV207_LOG` | `#define MWV207_LOG(level, fmt, args...)                                       \` |
| `drivers/mwv207/video-codec/common/mwv207_utils.h` | 31 | `IS_ALIGNED` | `#define IS_ALIGNED(i, n) (((i) & ((n)-1)) == 0)` |
| `drivers/mwv207/video-codec/common/mwv207_utils.h` | 33 | `ARRAY_ELEMS` | `#define ARRAY_ELEMS(a) (sizeof(a) / sizeof((a)[0]))` |
| `drivers/mwv207/video-codec/common/mwv207_utils.h` | 34 | `ALIGN` | `#define ALIGN(a) (1 << (a))` |
| `drivers/mwv207/video-codec/common/mwv207_utils.h` | 35 | `NEXT_MULTIPLE` | `#define NEXT_MULTIPLE(value, n) (((value) + (n)-1) & ~((n)-1))` |
| `drivers/mwv207/video-codec/common/mwv207_utils.h` | 36 | `ALIGN_FLOOR` | `#define ALIGN_FLOOR(i, n) ((i) & ~((n)-1))` |
| `drivers/mwv207/video-codec/common/mwv207_video_info.h` | 43 | `MWV207_VIDEO_BANK_WIDTH` | `#define MWV207_VIDEO_BANK_WIDTH(x) ((x) << 0)` |
| `drivers/mwv207/video-codec/common/mwv207_video_info.h` | 44 | `MWV207_VIDEO_BANK_HEIGHT` | `#define MWV207_VIDEO_BANK_HEIGHT(x) ((x) << 3)` |
| `drivers/mwv207/video-codec/common/mwv207_video_info.h` | 45 | `MWV207_VIDEO_MACRO_TILE_ASPECT_RATIO` | `#define MWV207_VIDEO_MACRO_TILE_ASPECT_RATIO(x) ((x) << 6)` |
| `drivers/mwv207/video-codec/common/mwv207_video_info.h` | 46 | `MWV207_VIDEO_NUM_BANKS` | `#define MWV207_VIDEO_NUM_BANKS(x) ((x) << 9)` |
| `drivers/mwv207/video-codec/common/mwv207_vp9hwd_bool.h` | 35 | `STREAM_TRACE` | `#define STREAM_TRACE(x, y)` |
| `drivers/mwv207/video-codec/common/mwv207_vp9hwd_bool.h` | 36 | `STREAM_TRACE_CFF` | `#define STREAM_TRACE_CFF(x, y, a)` |
| `drivers/mwv207/video-codec/common/mwv207_vp9hwd_bool.h` | 37 | `VP9DEC_DEBUG` | `#define VP9DEC_DEBUG(x)` |
| `drivers/mwv207/video-codec/common/mwv207_vp9hwd_bool.h` | 39 | `CHECK_END_OF_STREAM` | `#define CHECK_END_OF_STREAM(s)                                               \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 321 | `IS_PIC_PACKED_RGB` | `#define IS_PIC_PACKED_RGB(fmt)                                               \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 328 | `IS_PIC_PLANAR_RGB` | `#define IS_PIC_PLANAR_RGB(fmt)                                               \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 332 | `IS_PIC_MONOCHROME` | `#define IS_PIC_MONOCHROME(fmt)                                               \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 337 | `IS_PIC_PLANAR` | `#define IS_PIC_PLANAR(fmt)                                                   \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 344 | `IS_PIC_NV21` | `#define IS_PIC_NV21(fmt)                                                     \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 351 | `IS_PIC_SEMIPLANAR` | `#define IS_PIC_SEMIPLANAR(fmt)                                               \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 357 | `IS_PIC_8BIT` | `#define IS_PIC_8BIT(fmt)                                                     \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 364 | `IS_PIC_10BIT` | `#define IS_PIC_10BIT(fmt)                                                    \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 373 | `IS_PIC_16BIT` | `#define IS_PIC_16BIT(fmt)                                                    \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 383 | `IS_PIC_24BIT` | `#define IS_PIC_24BIT(fmt)                                                    \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 386 | `IS_PIC_32BIT` | `#define IS_PIC_32BIT(fmt)                                                    \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 391 | `IS_PIC_48BIT` | `#define IS_PIC_48BIT(fmt)                                                    \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 394 | `IS_PIC_TILE` | `#define IS_PIC_TILE(fmt)                                                     \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 407 | `IS_PIC_RFC` | `#define IS_PIC_RFC(fmt) ((fmt) == DEC_OUT_FRM_RFC)` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_h264.c` | 47 | `FIELD_PICTURE` | `#define FIELD_PICTURE(h) ((h)->picture_structure != PICT_FRAME)` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.c` | 40 | `MIN` | `#define MIN(a, b) (((a) < (b)) ? (a) : (b))` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.c` | 42 | `MAX` | `#define MAX(a, b) (((a) > (b)) ? (a) : (b))` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.c` | 45 | `DOWN_SCALE_SIZE` | `#define DOWN_SCALE_SIZE(w, ds) (((w) / (ds)) & ~0x1)` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.c` | 46 | `NEXT_MULTIPLE` | `#define NEXT_MULTIPLE(value, n) (((value) + (n)-1) & ~((n)-1))` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.c` | 47 | `ALIGN` | `#define ALIGN(a) (1 << (a))` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.c` | 48 | `TOFIX` | `#define TOFIX(d, q) ((uint64_t)((d) * ((uint64_t)1 << (q))))` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.c` | 49 | `FDIVI` | `#define FDIVI(a, b) ((a) / (b))` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.h` | 71 | `IS_PACKED_RGB` | `#define IS_PACKED_RGB(fmt)                                                   \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.h` | 77 | `IS_PLANAR_RGB` | `#define IS_PLANAR_RGB(fmt)                                                   \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.h` | 81 | `IS_ALPHA_FORMAT` | `#define IS_ALPHA_FORMAT(fmt)                                                 \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.h` | 85 | `IS_4PLAN_FORMAT` | `#define IS_4PLAN_FORMAT(fmt)                                                 \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.h` | 89 | `IS_8BITS_FORMAT` | `#define IS_8BITS_FORMAT(fmt)                                                 \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.h` | 94 | `IS_16BITS_FORMAT` | `#define IS_16BITS_FORMAT(fmt)                                                \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.h` | 312 | `MAKE_FIXED_UNI` | `#define MAKE_FIXED_UNI(x) ((x) << FIXED_BITS_UNI)` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.h` | 314 | `FLOOR` | `#define FLOOR(x) ((x) / (FIXED_RADIX))` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.h` | 317 | `DEC400_PPn_TABLE_OFFSET` | `#define DEC400_PPn_TABLE_OFFSET(n) (((n) * (DEC400_PP_TABLE_SIZE)))` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.h` | 318 | `DEC400_PPn_Y_TABLE_OFFSET` | `#define DEC400_PPn_Y_TABLE_OFFSET(n) (DEC400_PPn_TABLE_OFFSET(n))` |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.h` | 319 | `DEC400_PPn_UV_TABLE_OFFSET` | `#define DEC400_PPn_UV_TABLE_OFFSET(n)                                        \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec.h` | 54 | `IS_JMDEC` | `#define IS_JMDEC(asic_id) (((asic_id) >> 16) == 0x8001)` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec.h` | 55 | `IS_DEC_KMB_A0` | `#define IS_DEC_KMB_A0(asic_id) (((asic_id)&0xffff) == 0x6010)` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec.h` | 56 | `IS_DEC_TBH` | `#define IS_DEC_TBH(asic_id, hw_build_id)                                     \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec.h` | 60 | `IS_DEC_KMB_B0` | `#define IS_DEC_KMB_B0(asic_id, hw_build_id)                                  \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec.h` | 62 | `IS_DEC_KMB_B0_B` | `#define IS_DEC_KMB_B0_B(asic_id, hw_build_id)                                \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec.h` | 64 | `IS_DEC_OYB` | `#define IS_DEC_OYB(asic_id, hw_build_id) (((hw_build_id)&0xffff) == 0x1F66)` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_cfg.h` | 298 | `X170_CHECK_BUS_ADDRESS` | `#define X170_CHECK_BUS_ADDRESS(d) ((d) < 64 ? 1 : 0)` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_cfg.h` | 299 | `X170_CHECK_VIRTUAL_ADDRESS` | `#define X170_CHECK_VIRTUAL_ADDRESS(d) (((void *)(d) < (void *)64) ? 1 : 0)` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_cfg.h` | 300 | `X170_CHECK_BUS_ADDRESS_AGLINED` | `#define X170_CHECK_BUS_ADDRESS_AGLINED(d)                                    \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_reg.h` | 109 | `MWV207_9200_IS_G1_LEGACY` | `#define MWV207_9200_IS_G1_LEGACY(hw_id) (((hw_id) >> 16) == 0x6731)` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_reg.h` | 110 | `MWV207_9200_IS_G2_LEGACY` | `#define MWV207_9200_IS_G2_LEGACY(hw_id) (((hw_id) >> 16) == 0x6732)` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_reg.h` | 112 | `MWV207_9200_IS_LEGACY` | `#define MWV207_9200_IS_LEGACY(hw_id)                                          \` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_reg.h` | 114 | `MWV207_9200_IS_JMDEC` | `#define MWV207_9200_IS_JMDEC(asic_id) (((asic_id) >> 16) == 0x8001)` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_reg.h` | 115 | `MWV207_9200_IS_KMB_A0` | `#define MWV207_9200_IS_KMB_A0(asic_id) (((asic_id)&0xffff) == 0x6010)` |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_vpufeature.c` | 59 | `UNUSED` | `#define UNUSED(x) (void)(x)` |

