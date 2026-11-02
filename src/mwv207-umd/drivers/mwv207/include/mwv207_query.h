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

#ifndef MWV207_QUERY_H
#define MWV207_QUERY_H

struct mwv207_context;

struct mwv207_query_info {
   bool enabled;
   bool suspend;
   struct list_head active_queries;
};

struct mwv207_query {
   enum pipe_query_type type;
   struct mwv207_bo *bo;
   struct mwv207_bo *tfb;

   unsigned query_id;

   struct list_head active_list;
};

void
mwv207_context_query_init(struct pipe_context *ctx);

/* 条件渲染：当前是否允许发出绘制。无谓词时恒 true。
 * FIX(jm9100-oss) */
bool
mwv207_check_render_cond(struct pipe_context *pctx);

void
mwv207_context_query_destory(struct pipe_context *pctx);

#endif
