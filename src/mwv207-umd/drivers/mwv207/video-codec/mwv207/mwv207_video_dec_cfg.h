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

#ifndef __DEC_X170_CFG_H__
#define __DEC_X170_CFG_H__

#define DEC_X170_LITTLE_ENDIAN 0
#define DEC_X170_BIG_ENDIAN 1

#define DEC_X170_BUS_BURST_LENGTH_UNDEFINED 0
#define DEC_X170_BUS_BURST_LENGTH_4 4
#define DEC_X170_BUS_BURST_LENGTH_8 8
#define DEC_X170_BUS_BURST_LENGTH_16 16

#define DEC_X170_ASIC_SERVICE_PRIORITY_DEFAULT 0
#define DEC_X170_ASIC_SERVICE_PRIORITY_WR_1 1
#define DEC_X170_ASIC_SERVICE_PRIORITY_WR_2 2
#define DEC_X170_ASIC_SERVICE_PRIORITY_RD_1 3
#define DEC_X170_ASIC_SERVICE_PRIORITY_RD_2 4

#define DEC_X170_OUTPUT_FORMAT_RASTER_SCAN 0
#define DEC_X170_OUTPUT_FORMAT_TILED 1

#define DEC_X170_BUS_WIDTH_32 0
#define DEC_X170_BUS_WIDTH_64 1
#define DEC_X170_BUS_WIDTH_128 2
#define DEC_X170_BUS_WIDTH_256 3

#define MWV207DEC_STREAM_SWAP_8 1
#define MWV207DEC_STREAM_SWAP_16 2
#define MWV207DEC_STREAM_SWAP_32 4
#define MWV207DEC_STREAM_SWAP_64 8

#define MWV207DEC_DOUBLE_BUFFER_ENABLE 0

#define DEC_X170_APF_DISABLE 0

#define DEC_X170_CLOCK_GATING 1
#define DEC_X170_CLOCK_GATING_RUNTIME 0

#ifndef DEC_X170_USING_IRQ

#define DEC_X170_USING_IRQ 1
#endif

#ifndef DEC_X170_ASIC_SERVICE_PRIORITY

#define DEC_X170_ASIC_SERVICE_PRIORITY DEC_X170_ASIC_SERVICE_PRIORITY_DEFAULT
#endif

#ifndef DEC_X170_SCMD_DISABLE

#define DEC_X170_SCMD_DISABLE 0
#endif

#ifndef DEC_X170_APF_DISABLE
/* Advanced prefetch disable flag. If disable flag is set, product shall
 * operate akin to 9190 and earlier products. */
#define DEC_X170_APF_DISABLE 0
#endif

#define DEC_X170_APF_SINGLE_PU_MODE 0

#ifndef DEC_X170_BUS_BURST_LENGTH

#define DEC_X170_BUS_BURST_LENGTH DEC_X170_BUS_BURST_LENGTH_16
#endif

#ifndef DEC_X170_INPUT_STREAM_ENDIAN

#define DEC_X170_INPUT_STREAM_ENDIAN DEC_X170_LITTLE_ENDIAN
#endif

#ifndef DEC_X170_OUTPUT_PICTURE_ENDIAN

#define DEC_X170_OUTPUT_PICTURE_ENDIAN DEC_X170_LITTLE_ENDIAN
#endif

#ifndef DEC_X170_LATENCY_COMPENSATION

#define DEC_X170_LATENCY_COMPENSATION 0
#endif

#ifndef DEC_X170_INTERNAL_CLOCK_GATING

#define DEC_X170_INTERNAL_CLOCK_GATING DEC_X170_CLOCK_GATING
#endif
#ifndef DEC_X170_INTERNAL_CLOCK_GATING_RUNTIME

#define DEC_X170_INTERNAL_CLOCK_GATING_RUNTIME DEC_X170_CLOCK_GATING_RUNTIME
#endif
#ifndef DEC_X170_OUTPUT_FORMAT

#define DEC_X170_OUTPUT_FORMAT DEC_X170_OUTPUT_FORMAT_RASTER_SCAN
#endif

#ifndef DEC_X170_DATA_DISCARD_ENABLE
#define DEC_X170_DATA_DISCARD_ENABLE 0
#endif

#ifndef DEC_X170_OUTPUT_SWAP_32_ENABLE
#define DEC_X170_OUTPUT_SWAP_32_ENABLE 1
#endif

#ifndef DEC_X170_INPUT_DATA_SWAP_32_ENABLE
#define DEC_X170_INPUT_DATA_SWAP_32_ENABLE 1
#endif

#ifndef DEC_X170_INPUT_STREAM_SWAP_32_ENABLE
#define DEC_X170_INPUT_STREAM_SWAP_32_ENABLE 1
#endif

#ifndef DEC_X170_INPUT_DATA_ENDIAN
#define DEC_X170_INPUT_DATA_ENDIAN DEC_X170_BIG_ENDIAN
#endif

#ifndef DEC_X170_AXI_ID_R
#define DEC_X170_AXI_ID_R 0
#endif

#ifndef DEC_X170_AXI_ID_W
#define DEC_X170_AXI_ID_W 0
#endif

/* Enable/Disable AXI bus read and write ID service specific IDs.
        Set these to 1 to enable service specific, incremental read/write IDs.
   For example, if DEC_X170_AXI_ID_R_E is set, all read services will get
   unique ID within the range
        DEC_X170_AXI_ID_R...(DEC_X170_AXI_ID_R+NUM_OF_READ_SERVICES). */
#ifndef DEC_X170_AXI_ID_R_E
#define DEC_X170_AXI_ID_R_E 0
#endif

#ifndef DEC_X170_AXI_ID_W_E
#define DEC_X170_AXI_ID_W_E 0
#endif

#ifndef DEC_X170_SERVICE_MERGE_DISABLE
#define DEC_X170_SERVICE_MERGE_DISABLE 0
#endif

#ifndef DEC_X170_BUS_WIDTH
#define DEC_X170_BUS_WIDTH DEC_X170_BUS_WIDTH_128
#endif

#ifndef MWV207DEC_STREAM_SWAP
#if 0
#define MWV207DEC_STREAM_SWAP                                                 \
   (MWV207DEC_STREAM_SWAP_8 | MWV207DEC_STREAM_SWAP_16                         \
    | MWV207DEC_STREAM_SWAP_32)
#else

#define MWV207DEC_STREAM_SWAP 0
#endif
#endif

#ifndef MWV207DEC_MAX_BURST
#define MWV207DEC_MAX_BURST 64
#endif

#ifndef MWV207DEC_INTERNAL_DOUBLE_REF_BUFFER
#define MWV207DEC_INTERNAL_DOUBLE_REF_BUFFER MWV207DEC_DOUBLE_BUFFER_ENABLE
#endif

#ifndef MWV207DEC_TIMEOUT_OVERRIDE
#define MWV207DEC_TIMEOUT_OVERRIDE 0x500000
#endif

#if (DEC_X170_DATA_DISCARD_ENABLE                                            \
     && (DEC_X170_OUTPUT_FORMAT == DEC_X170_OUTPUT_FORMAT_TILED))
#error                                                                       \
   "Bad value specified: DEC_X170_DATA_DISCARD_ENABLE && (DEC_X170_OUTPUT_FORMAT == DEC_X170_OUTPUT_FORMAT_TILED)"
#endif

#if (DEC_X170_OUTPUT_PICTURE_ENDIAN > 1)
#error "Bad value specified for DEC_X170_OUTPUT_PICTURE_ENDIAN"
#endif

#if (DEC_X170_OUTPUT_FORMAT > 1)
#error "Bad value specified for DEC_X170_OUTPUT_FORMAT"
#endif

#if (DEC_X170_BUS_BURST_LENGTH > 31)
#error "Bad value specified for DEC_X170_AMBA_BURST_LENGTH"
#endif

#if (DEC_X170_ASIC_SERVICE_PRIORITY > 4)
#error "Bad value specified for DEC_X170_ASIC_SERVICE_PRIORITY"
#endif

#if (DEC_X170_LATENCY_COMPENSATION > 63)
#error "Bad value specified for DEC_X170_LATENCY_COMPENSATION"
#endif

#if (DEC_X170_OUTPUT_SWAP_32_ENABLE > 1)
#error "Bad value specified for DEC_X170_OUTPUT_SWAP_32_ENABLE"
#endif

#if (DEC_X170_INPUT_DATA_SWAP_32_ENABLE > 1)
#error "Bad value specified for DEC_X170_INPUT_DATA_SWAP_32_ENABLE"
#endif

#if (DEC_X170_INPUT_STREAM_SWAP_32_ENABLE > 1)
#error "Bad value specified for DEC_X170_INPUT_STREAM_SWAP_32_ENABLE"
#endif

#if (DEC_X170_OUTPUT_SWAP_32_ENABLE > 1)
#error "Bad value specified for DEC_X170_INPUT_DATA_ENDIAN"
#endif

#if (DEC_X170_DATA_DISCARD_ENABLE > 1)
#error "Bad value specified for DEC_X170_DATA_DISCARD_ENABLE"
#endif

#if (DEC_X170_SERVICE_MERGE_DISABLE > 1)
#error "Bad value specified for DEC_X170_SERVICE_MERGE_DISABLE"
#endif

#if ((DEC_X170_INTERNAL_CLOCK_GATING == 0)                                   \
     && DEC_X170_INTERNAL_CLOCK_GATING_RUNTIME)
#error                                                                       \
   "DEC_X170_INTERNAL_CLOCK_GATING_RUNTIME can only be enabled when DEC_X170_INTERNAL_CLOCK_GATING enabled"
#endif

#define DEC_MAX_PPU_COUNT 5

#if (DEC_MAX_PPU_COUNT == 1)
#define DEC_X170_REGISTERS 342
#define PP_UNIFIED_REGS 22
#elif (DEC_MAX_PPU_COUNT == 2)
#define DEC_X170_REGISTERS 359
#define PP_UNIFIED_REGS (22 + 17)
#elif (DEC_MAX_PPU_COUNT == 3)
#define DEC_X170_REGISTERS 376
#define PP_UNIFIED_REGS (22 + 17 * 2)
#elif (DEC_MAX_PPU_COUNT == 4)
#define DEC_X170_REGISTERS 437
#define PP_UNIFIED_REGS (22 + 17 * 3 + 22 + 22)
#elif (DEC_MAX_PPU_COUNT == 5)
#define DEC_X170_REGISTERS 503
#define PP_UNIFIED_REGS (183)
#endif

#define TOTAL_X170_ORIGIN_REGS 119

#define TOTAL_X170_EXPAND_REGS 36

#define TOTAL_X170_EXPAND_PP_REGS 9

#define PP_START_REGS (TOTAL_X170_ORIGIN_REGS + TOTAL_X170_EXPAND_REGS)

#define ERROR_CONCEAL_START_REGS 315
#define TOTAL_ERROR_CONCEAL_REGS 2

#define PP_START_UNIFIED_REGS 320
#define TOTAL_X170_REGISTERS (PP_START_UNIFIED_REGS + PP_UNIFIED_REGS)
#define DEC_X170_EXPAND_REGS 27

#define DEC_X170_MAX_STREAM ((1 << 24) - 1)
#define DEC_X170_MAX_STREAM_G2 ((1 << 30) - 1)
#define DEC_X170_MAX_STREAM_JMDEC ((1 << 30) - 1)

#ifndef DEC_X170_TIMEOUT_LENGTH
#define DEC_X170_TIMEOUT_LENGTH -1
#endif

#define DEC_X170_HW_TIMEOUT_INT_ENA 1

#define DEC_X170_REFBU_WIDTH 64
#define DEC_X170_REFBU_LATENCY 20
#define DEC_X170_REFBU_NONSEQ 8
#define DEC_X170_REFBU_SEQ 1

#define X170_CHECK_BUS_ADDRESS(d) ((d) < 64 ? 1 : 0)
#define X170_CHECK_VIRTUAL_ADDRESS(d) (((void *)(d) < (void *)64) ? 1 : 0)
#define X170_CHECK_BUS_ADDRESS_AGLINED(d)                                    \
   (((d) < 64 || ((d) & (DEC_X170_BUS_BURST_LENGTH - 1))) ? 1 : 0)

/* Max number of macroblock rows to memset/memcpy in partial freeze error
 * concealment */
#define DEC_X170_MAX_EC_COPY_ROWS 8

#define DEC_X170_TIMEOUT_CYCLES 0xFFFFFF

/* Deafult maximum threshold of AXI read/write outstanding transactions */
#define DEC_X170_MAX_RD_OUTSTANDING_THRESHOLD 64
#define DEC_X170_MAX_WR_OUTSTANDING_THRESHOLD 64

#define DEC_X170_PP_OUTPUT_SWAP 0

#define DEC_X170_BUS_ADDR_ALIGNMENT 64

#define MWV207DEC_PP_IN_BLK_SIZE_64X16 0
#define MWV207DEC_PP_IN_BLK_SIZE_64X64 1

#endif

