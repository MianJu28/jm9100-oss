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

#ifndef MWV207_LOG_H
#define MWV207_LOG_H

#include <stdio.h>
#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif

#define MWV207_LEVEL_NONE 0
#define MWV207_LEVEL_ERROR 1
#define MWV207_LEVEL_WARNING 2
#define MWV207_LEVEL_FIXME 3
#define MWV207_LEVEL_INFO 4
#define MWV207_LEVEL_DEBUG 5
#define MWV207_LEVEL_LOG 6
#define MWV207_LEVEL_TRACE_REG 7
#define MWV207_LEVEL_MEMDUMP 9
#define MWV207_LEVEL_COUNT 10
extern int mwv207_log_level;
extern char mwv207_level[MWV207_LEVEL_COUNT][20];

void mwv207_log_init(void);

#define MWV207_LOG(level, fmt, args...)                                       \
   if (level < mwv207_log_level) {                                            \
      printf(__FILE__ ":%d:%s() %s " fmt,                                    \
             __LINE__,                                                       \
             __func__,                                                       \
             mwv207_level[level],                                             \
             ##args);                                                        \
   }

#endif

