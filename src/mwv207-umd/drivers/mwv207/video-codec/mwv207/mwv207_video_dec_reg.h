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

#ifndef MWV207_9200_VDIEO_DEC_REG_H
#define MWV207_9200_VDIEO_DEC_REG_H
#include "mwv207_video_dec_cfg.h"
#include "mwv207_video_reg_buf.h"

#define USE_64BIT_ENV
#define DEC_HW_ALIGN_MASK 0x0F
#define DEC_X170_ALIGN_MASK 0x07

#define DEC_8170_IRQ_RDY 0x02
#define DEC_8170_IRQ_BUS 0x04
#define DEC_8170_IRQ_BUFFER 0x08
#define DEC_8170_IRQ_ASO 0x10
#define DEC_8170_IRQ_ERROR 0x20
#define DEC_8170_IRQ_SLICE 0x40
#define DEC_8170_IRQ_TIMEOUT 0x80
#define DEC_8170_IRQ_NOLASTSLICE 0x100
#define DEC_8170_IRQ_NONESLICE 0x200

#define DEC_8190_IRQ_ABORT 0x01
#define DEC_8190_IRQ_RDY DEC_8170_IRQ_RDY
#define DEC_8190_IRQ_BUS DEC_8170_IRQ_BUS
#define DEC_8190_IRQ_BUFFER DEC_8170_IRQ_BUFFER
#define DEC_8190_IRQ_ASO DEC_8170_IRQ_ASO
#define DEC_8190_IRQ_ERROR DEC_8170_IRQ_ERROR
#define DEC_8190_IRQ_SLICE DEC_8170_IRQ_SLICE
#define DEC_8190_IRQ_TIMEOUT DEC_8170_IRQ_TIMEOUT
#define DEC_8190_IRQ_NOLASTSLICE DEC_8170_IRQ_NOLASTSLICE
#define DEC_8190_IRQ_NONESLICE DEC_8170_IRQ_NONESLICE

#define DEC_IRQ_DISABLE 0x10
#define DEC_ABORT 0x20

#define DEC_HW_IRQ_RDY 0x02
#define DEC_HW_IRQ_BUS 0x04
#define DEC_HW_IRQ_BUFFER 0x08
#define DEC_HW_IRQ_ASO 0x10
#define DEC_HW_IRQ_ERROR 0x20
#define DEC_HW_IRQ_SLICE 0x40
#define DEC_HW_IRQ_TIMEOUT 0x80
#define DEC_HW_IRQ_NOLASTSLICE 0x100
#define DEC_HW_IRQ_NONESLICE 0x200
#define DEC_HW_IRQ_EXT_TIMEOUT 0x400
#define DEC_HW_IRQ_TILE 0x1000

#define PP_REG_START 60

enum mwv207_9200_hw_if_name {

#include "mwv207_video_dec_8170enum.h"
   JMDEC_DEC_IRQ_STAT,
   JMDEC_PP_IRQ_STAT,
   JMDEC_LAST_REG,

   JMDEC_MPEG4_DC_BASE_LSB = JMDEC_I4X4_OR_DC_BASE_LSB,
   JMDEC_MPEG4_DC_BASE_MSB = JMDEC_I4X4_OR_DC_BASE_MSB,
   JMDEC_INTRA_4X4_BASE_LSB = JMDEC_I4X4_OR_DC_BASE_LSB,
   JMDEC_INTRA_4X4_BASE_MSB = JMDEC_I4X4_OR_DC_BASE_MSB,

   JMDEC_VP6HWGOLDEN_BASE_LSB = JMDEC_REFER4_BASE_LSB,
   JMDEC_VP6HWGOLDEN_BASE_MSB = JMDEC_REFER4_BASE_MSB,
   JMDEC_VP6HWPART1_BASE_LSB = JMDEC_REFER13_BASE_LSB,
   JMDEC_VP6HWPART1_BASE_MSB = JMDEC_REFER13_BASE_MSB,
   JMDEC_VP6HWPART2_BASE_LSB = JMDEC_RLC_VLC_BASE_LSB,
   JMDEC_VP6HWPART2_BASE_MSB = JMDEC_RLC_VLC_BASE_MSB,
   JMDEC_VP6HWPROBTBL_BASE_LSB = JMDEC_QTABLE_BASE_LSB,
   JMDEC_VP6HWPROBTBL_BASE_MSB = JMDEC_QTABLE_BASE_MSB,

   JMDEC_PJPEG_COEFF_BUF_LSB = JMDEC_DIR_MV_BASE_LSB,
   JMDEC_PJPEG_COEFF_BUF_MSB = JMDEC_DIR_MV_BASE_MSB,

   JMDEC_INTER_VIEW_BASE_LSB = JMDEC_REFER15_BASE_LSB,
   JMDEC_INTER_VIEW_BASE_MSB = JMDEC_REFER15_BASE_MSB,
   JMDEC_INTER_VIEW_CBASE_LSB = JMDEC_REFER15_CBASE_LSB,
   JMDEC_INTER_VIEW_CBASE_MSB = JMDEC_REFER15_CBASE_MSB,
   JMDEC_INTER_VIEW_TYBASE_LSB = JMDEC_REFER15_TYBASE_LSB,
   JMDEC_INTER_VIEW_TYBASE_MSB = JMDEC_REFER15_TYBASE_MSB,
   JMDEC_INTER_VIEW_TCBASE_LSB = JMDEC_REFER15_TCBASE_LSB,
   JMDEC_INTER_VIEW_TCBASE_MSB = JMDEC_REFER15_TCBASE_MSB
};

#define MWV207_9200_IS_G1_LEGACY(hw_id) (((hw_id) >> 16) == 0x6731)
#define MWV207_9200_IS_G2_LEGACY(hw_id) (((hw_id) >> 16) == 0x6732)

#define MWV207_9200_IS_LEGACY(hw_id)                                          \
   ((((hw_id) >> 16) == 0x6731) || (((hw_id) >> 16) == 0x6732))
#define MWV207_9200_IS_JMDEC(asic_id) (((asic_id) >> 16) == 0x8001)
#define MWV207_9200_IS_KMB_A0(asic_id) (((asic_id)&0xffff) == 0x6010)

void mwv207_9200_set_dec_register(struct mwv207_video_reg_buf *reg_buf,
                                 uint32_t id,
                                 uint32_t value);
void mwv207_9200_set_addr_reg(struct mwv207_video_reg_buf *reg_buf,
                             uint32_t id,
                             struct mwv207_bo *bo,
                             uint64_t offset,
                             uint32_t flags);
void mwv207_9200_set_addr_reg_hi(struct mwv207_video_reg_buf *reg_buf,
                                uint32_t id,
                                struct mwv207_bo *bo,
                                uint64_t offset,
                                uint32_t flags);
uint32_t
mwv207_9200_get_dec_register(struct mwv207_video_reg_buf *reg_buf, uint32_t id);
void mwv207_9200_dump_register(struct mwv207_video_reg_buf *reg_buf);

#endif

