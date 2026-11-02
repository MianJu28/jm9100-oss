/*
* Copyright (c) 2020 ChangSha JingJiaMicro Electronics Co., Ltd.
* All rights reserved.
*
* Author:
*      shanjinkui <shanjinkui@jingjiamicro.com>
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice (including the next
* paragraph) shall be included in all copies or substantial portions of the
* Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* ChangSha JingJiaMicro Electronics Co., Ltd. AND/OR ITS SUPPLIERS BE LIABLE
* FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#include "util/list.h"
#include "util/u_dump.h"
#include "util/u_memory.h"
#include "util/os_time.h"

#include "mwv207_cmd.h"
#include "mwv207_query.h"
#include "mwv207_context.h"

static struct pipe_query *
mwv207_create_query(struct pipe_context *pctx,
                    unsigned query_type, unsigned index)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_query *query = CALLOC_STRUCT(mwv207_query);
   assert(query);

   query->type = query_type;
   query->bo = ctx->ws->bo_create(ctx->ws, 64*100,
      MWV207_DOMAIN_VRAM, MWV207_FLAG_CPU_ACCESS);
   query->tfb = ctx->ws->bo_create(ctx->ws, 64*100,
      MWV207_DOMAIN_VRAM, MWV207_FLAG_CPU_ACCESS);
   query->query_id = 0;

   void *data = ctx->ws->bo_map(ctx->ws, query->bo);
   memset(data, 0, 64*100);
   ctx->ws->bo_unmap(ctx->ws, query->bo);

   list_inithead(&query->active_list);

   return (struct pipe_query *)query;
}

static void
mwv207_destroy_query(struct pipe_context *pctx,
                     struct pipe_query *q)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_query *query = (struct mwv207_query *)q;
   ctx->ws->bo_reference(ctx->ws, &query->bo, NULL);
   ctx->ws->bo_reference(ctx->ws, &query->tfb, NULL);
   FREE(q);
}

static bool
mwv207_begin_query(struct pipe_context *pctx,
                   struct pipe_query *q)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_query *query = (struct mwv207_query *)q;

   bool only_depth = (ctx->fb_state.nr_cbufs == 0 && ctx->fb_state.zsbuf != 0);

   uint64_t time_stamp = 0;
   uint32_t time_stamp_high = 0, time_stamp_low = 0;
   uint32_t *cmd_buf = NULL;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   switch (query->type) {
   case PIPE_QUERY_OCCLUSION_COUNTER:
   case PIPE_QUERY_OCCLUSION_PREDICATE:
   case PIPE_QUERY_OCCLUSION_PREDICATE_CONSERVATIVE:
   case PIPE_QUERY_SO_OVERFLOW_PREDICATE:
   case PIPE_QUERY_SO_OVERFLOW_ANY_PREDICATE:
      if (only_depth) {
         MWV207_EMIT(cmd_buf, 0xE18,
                     (ALIGN_BIT(0x7, 0x0) & (0x7)));
      } else {
         MWV207_EMIT(cmd_buf, 0xE18,
                     (ALIGN_BIT(0x6, 0x0) & (0x7)));
      }

      mwv207_add_relocs(ctx, &cmd_buf, 0xE09,
                        query->bo, query->query_id * 64, 0x1);
      break;
   case PIPE_QUERY_TIMESTAMP:
   case PIPE_QUERY_TIMESTAMP_DISJOINT:
   case PIPE_QUERY_TIME_ELAPSED:
      mwv207_add_relocs(ctx, &cmd_buf, 0xE1A,
                        query->bo, 0, 0x1);
      time_stamp = os_time_get_nano();
      time_stamp_high = ((time_stamp >> 32) & 0xffffffff);
      time_stamp_low = (time_stamp & 0xffffffff);
      MWV207_EMIT(cmd_buf, 0xE26, time_stamp_high);
      MWV207_EMIT(cmd_buf, 0xE1B, time_stamp_low);
      break;
   case PIPE_QUERY_PRIMITIVES_GENERATED:
      mwv207_add_relocs(ctx, &cmd_buf, 0x7005,
                        query->bo, 0, 0x1);

      if (ctx->query_info->suspend) {
         MWV207_EMIT(cmd_buf, 0x7001,
                     (ALIGN_BIT(0x4, 0x0) & (0x7)));

         if (ctx->stream_out.num_targets == 0) {
            MWV207_EMIT(cmd_buf, 0x7004,
                        (ALIGN_BIT(0x4, 0x0) & (0x7)) |
                        (ALIGN_BIT(0x1, 0x8) & (0xF00)));
         }
      } else {
         MWV207_EMIT(cmd_buf, 0x7004,
                     (ALIGN_BIT(0x1, 0x0) & (0x7)) |
                     (ALIGN_BIT(0x1, 0x8) & (0xF00)));

         if (ctx->stream_out.num_targets == 0) {
            mwv207_add_relocs(ctx, &cmd_buf, 0x7002,
                              query->tfb, 0, 0x1);
            MWV207_EMIT(cmd_buf, 0x7030, 0);
            MWV207_EMIT(cmd_buf, 0x7020, 0);
            MWV207_EMIT(cmd_buf, 0x7001,
                        (ALIGN_BIT(0x1, 0x0) & (0x7)));
         }
      }
      break;
   case PIPE_QUERY_PRIMITIVES_EMITTED:
      mwv207_add_relocs(ctx, &cmd_buf, 0x7005,
                        query->bo, 0, 0x1);

      if (ctx->query_info->suspend) {
         MWV207_EMIT(cmd_buf, 0x7004,
                     (ALIGN_BIT(0x4, 0x0) & (0x7)) |
                     (ALIGN_BIT(0x0, 0x8) & (0xF00)));
      } else {
         MWV207_EMIT(cmd_buf, 0x7004,
                     (ALIGN_BIT(0x1, 0x0) & (0x7)) |
                     (ALIGN_BIT(0x0, 0x8) & (0xF00)));
      }
      break;
   case PIPE_QUERY_SO_STATISTICS:
      break;
   case PIPE_QUERY_GPU_FINISHED:
      break;
   case PIPE_QUERY_PIPELINE_STATISTICS:
      break;
   case PIPE_QUERY_PIPELINE_STATISTICS_SINGLE:
      break;
   default:
      unreachable("mwv207: unknown query type");
   }

   mwv207_end_record(ctx->cmd, &cmd_buf);

   if (!ctx->query_info->suspend)
      list_addtail(&query->active_list, &ctx->query_info->active_queries);

   return true;
}

static bool
mwv207_end_query(struct pipe_context *pctx,
                 struct pipe_query *q)
{
   uint32_t *cmd_buf = NULL;
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_query *query = (struct mwv207_query *)q;

   uint64_t time_stamp = 0;
   uint32_t time_stamp_high = 0, time_stamp_low = 0;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   switch (query->type) {
   case PIPE_QUERY_OCCLUSION_COUNTER:
   case PIPE_QUERY_OCCLUSION_PREDICATE:
   case PIPE_QUERY_OCCLUSION_PREDICATE_CONSERVATIVE:
   case PIPE_QUERY_SO_OVERFLOW_PREDICATE:
   case PIPE_QUERY_SO_OVERFLOW_ANY_PREDICATE:
      MWV207_EMIT(cmd_buf, 0xE0C, 0x01df5e76);
      query->query_id++;
      break;
   case PIPE_QUERY_TIMESTAMP:
      mwv207_add_relocs(ctx, &cmd_buf, 0xE1A,
                        query->bo, 0, 0x1);
      time_stamp = os_time_get_nano();
      time_stamp_high = ((time_stamp >> 32) & 0xffffffff);
      time_stamp_low = (time_stamp & 0xffffffff);
      MWV207_EMIT(cmd_buf, 0xE26, time_stamp_high);
      MWV207_EMIT(cmd_buf, 0xE1B, time_stamp_low);
      break;

   case PIPE_QUERY_PRIMITIVES_GENERATED:
      MWV207_EMIT(cmd_buf, 0x7004,
                  (ALIGN_BIT(0x2, 0x0) & (0x7)) |
                  (ALIGN_BIT(0x1, 0x8) & (0xF00)));

      if (ctx->stream_out.num_targets == 0) {
         MWV207_EMIT(cmd_buf, 0x7001,
                     (ALIGN_BIT(0x2, 0x0) & (0x7)));
      }
      break;
   case PIPE_QUERY_PRIMITIVES_EMITTED:
      MWV207_EMIT(cmd_buf, 0x7004,
                  (ALIGN_BIT(0x2, 0x0) & (0x7)) |
                  (ALIGN_BIT(0x0, 0x8) & (0xF00)));
      break;
   default:
      break;
   }

   mwv207_end_record(ctx->cmd, &cmd_buf);

   if (!ctx->query_info->suspend && query->type != PIPE_QUERY_TIMESTAMP)
      list_delinit(&query->active_list);

   return true;
}

static bool
mwv207_get_query_result(struct pipe_context *pctx,
                        struct pipe_query *q,
                        bool wait,
                        union pipe_query_result *result)
{
   uint64_t count = 0;
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_query *query = (struct mwv207_query *)q;

   pctx->flush(pctx, NULL, 0);
   ctx->ws->bo_wait(ctx->ws, query->bo, OS_TIMEOUT_INFINITE);

   uint64_t *results = ctx->ws->bo_map(ctx->ws, query->bo);

   /* 硬件布局（反编译 + 上机实测确认）：
    *   每个 begin/end 占用 BO 里一个 64 字节槽（见 begin 的 query_id*64 偏移）；
    *   槽内前 cluster_count 个 u32 分别是各簇的计数，需按 cluster_mask 求和。
    *   旧代码按 uint64_t 步进读（results[i]）且用 <= 越界，故恒为 0 / 0xFFFFFFFF。
    * FIX(jm9100-oss) */
   {
      uint32_t *w = (uint32_t *)results;
      for (int i = 0; i < query->query_id; i++) {
         uint32_t *slot = w + i * 16;          /* 64 字节 = 16 个 u32 */
         for (int c = 0; c < ctx->ws->cluster_count; c++)
            count += slot[c];
      }
   }

   util_query_clear_result(result, query->type);

   switch (query->type) {
   case PIPE_QUERY_OCCLUSION_PREDICATE:
   case PIPE_QUERY_OCCLUSION_PREDICATE_CONSERVATIVE:
   case PIPE_QUERY_SO_OVERFLOW_PREDICATE:
   case PIPE_QUERY_SO_OVERFLOW_ANY_PREDICATE:
      result->b = (count != 0);
      break;
   case PIPE_QUERY_GPU_FINISHED:
      result->b = true;
      break;
   case PIPE_QUERY_OCCLUSION_COUNTER:
      result->u64 = count;
      break;
   case PIPE_QUERY_TIMESTAMP:
   case PIPE_QUERY_TIMESTAMP_DISJOINT:
      result->u64 = results[0];
      break;
   case PIPE_QUERY_TIME_ELAPSED:
      result->u64 = os_time_get_nano() - results[0];
      break;
   case PIPE_QUERY_PRIMITIVES_EMITTED:
   case PIPE_QUERY_PRIMITIVES_GENERATED:
      result->u64 = results[0];
      break;
   case PIPE_QUERY_SO_STATISTICS:
   case PIPE_QUERY_PIPELINE_STATISTICS:
   case PIPE_QUERY_PIPELINE_STATISTICS_SINGLE:
   default:
      /* FIX(jm9100-oss): E7 —— 这几类查询的 begin_query 只 break、不发射任何
       * 寄存器（硬件侧没有对应的计数器），所以这里根本拿不到数据。
       * 原实现落到 default 的 unreachable()，release 构建下 unreachable
       * 是 __builtin_unreachable()，等于把"不支持"变成未定义行为 ——
       * 一旦应用真的用 GL_ARB_pipeline_statistics_query，就是随机崩溃。
       * 正确做法是按 Gallium 契约返回 false（表示"结果不可用"），
       * 上层 st/mesa 会把它报成 GL_QUERY_RESULT_AVAILABLE = 0，
       * 应用拿到的是合法的"暂无结果"，不会崩。
       * result 已在 switch 之前 util_query_clear_result() 清零，这里只需
       * 归还 BO 映射并复位槽位计数。 */
      memset(results, 0, 64 * query->query_id);
      ctx->ws->bo_unmap(ctx->ws, query->bo);
      query->query_id = 0;
      return false;
   }

   memset(results, 0, 64 * query->query_id);
   ctx->ws->bo_unmap(ctx->ws, query->bo);
   query->query_id = 0;

   return true;
}

/* 条件渲染（GL_NV_conditional_render）。
 *
 * 硬件侧其实有能力做：MWV207REG_OCCLUSION_CONTROL.MODE 里
 * BOOLEAN_CONDITIONAL(0x2) / BOOLEAN_CONDITIONAL_EARLY(0x5) 就是"按谓词决定是否
 * 绘制"，但那需要把谓词所在的 BO 地址喂进 0xE0C 那条命令、并让命令流按条件跳转，
 * 改动面大且难以自证。
 *
 * 这里改成 CPU 侧判断（与 llvmpipe/softpipe 同构，语义完全合法）：
 *   1. BeginConditionalRender 时把已结束的查询结果读回来（flush + wait）；
 *   2. 之后每个绘制/清除点看这个缓存值，false 就直接不发命令。
 * GL_QUERY_*_NO_WAIT 的语义是"实现**允许**不等待"，因此这里等待并不违规。
 * 结果只读一次并缓存——查询已 end，值不会再变。
 *
 * FIX(jm9100-oss) */
static void
mwv207_render_condition(struct pipe_context *pctx,
                        struct pipe_query *query,
                        bool condition,
                        enum pipe_render_cond_flag mode)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   ctx->render_cond_query = query;
   ctx->render_cond_cond = condition;
   ctx->render_cond_mode = mode;
   ctx->render_cond_cached = false;
   ctx->render_cond_pass = true;
}

bool
mwv207_check_render_cond(struct pipe_context *pctx)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   if (!ctx->render_cond_query)
      return true;

   if (!ctx->render_cond_cached) {
      union pipe_query_result result = {0};
      bool wait = (ctx->render_cond_mode == PIPE_RENDER_COND_WAIT ||
                   ctx->render_cond_mode == PIPE_RENDER_COND_BY_REGION_WAIT);
      bool ok = pctx->get_query_result(pctx, ctx->render_cond_query, wait, &result);
      /* 谓词型查询把结果放在 result.b，计数型放在 result.u64；
       * 两者都从偏移 0 起，且这里已把整个 union 清零，故按 u64 读是安全的。 */
      bool pass = ok && result.u64 != 0;

      /* condition == true 表示"反相"：查询为假时才绘制。 */
      ctx->render_cond_pass = ctx->render_cond_cond ? !pass : pass;
      ctx->render_cond_cached = true;
   }

   return ctx->render_cond_pass;
}

static void
mwv207_suspend_queries(struct pipe_context *pctx)
{
   struct mwv207_query *query;
   struct mwv207_context *ctx = mwv207_context(pctx);
   LIST_FOR_EACH_ENTRY(query, &ctx->query_info->active_queries, active_list)
      mwv207_end_query(pctx, (struct pipe_query *)query);
}

static void
mwv207_resume_queries(struct pipe_context *pctx)
{
   struct mwv207_query *query;
   struct mwv207_context *ctx = mwv207_context(pctx);
   LIST_FOR_EACH_ENTRY(query, &ctx->query_info->active_queries, active_list)
      mwv207_begin_query(pctx, (struct pipe_query *)query);
}

static void
mwv207_set_active_query_state(struct pipe_context *pctx, bool enable)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   ctx->query_info->enabled = enable;
   ctx->query_info->suspend = true;

   if (enable)
      mwv207_resume_queries(pctx);
   else
      mwv207_suspend_queries(pctx);

   ctx->query_info->suspend = false;
}

void
mwv207_context_query_init(struct pipe_context *pctx)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   ctx->query_info = CALLOC_STRUCT(mwv207_query_info);
   assert(ctx->query_info);
   list_inithead(&ctx->query_info->active_queries);

   pctx->create_query  = mwv207_create_query;
   pctx->destroy_query = mwv207_destroy_query;
   pctx->begin_query   = mwv207_begin_query;
   pctx->end_query     = mwv207_end_query;

   pctx->get_query_result       = mwv207_get_query_result;
   pctx->set_active_query_state = mwv207_set_active_query_state;
   pctx->render_condition       = mwv207_render_condition;   /* FIX(jm9100-oss) */
}

void
mwv207_context_query_destroy(struct pipe_context *pctx)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   if (ctx->query_info)
      FREE(ctx->query_info);
}
