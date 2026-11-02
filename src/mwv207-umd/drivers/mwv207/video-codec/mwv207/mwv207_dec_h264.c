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
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "mwv207_dec_h264.h"
#include "mwv207_dec_ppu.h"
#include "mwv207_decoder_h264_nal_unit.h"
#include "mwv207_decoder_h264_slice_header.h"
#include "mwv207_decoder_sw_util.h"
#include "mwv207_utils.h"
#include "mwv207_video_dec_cfg.h"
#include "vl/vl_video_buffer.h"

#define ASIC_MB_RLC_BUFFER_SIZE 880
#define ASIC_MB_CTRL_BUFFER_SIZE 8
#define ASIC_MB_MV_BUFFER_SIZE 64
#define ASIC_MB_I4X4_BUFFER_SIZE 8
#define ASIC_CABAC_INIT_BUFFER_SIZE 3680
#define ASIC_SCALING_LIST_SIZE 6 * 16 + 2 * 64
#define ASIC_POC_BUFFER_SIZE 34 * 4

#define ASIC_POC_BUFFER_SIZE_H10 144

#define FIELD_PICTURE(h) ((h)->picture_structure != PICT_FRAME)

static const uint32_t ref_base[16] = {JMDEC_REFER0_BASE_LSB,
                                      JMDEC_REFER1_BASE_LSB,
                                      JMDEC_REFER2_BASE_LSB,
                                      JMDEC_REFER3_BASE_LSB,
                                      JMDEC_REFER4_BASE_LSB,
                                      JMDEC_REFER5_BASE_LSB,
                                      JMDEC_REFER6_BASE_LSB,
                                      JMDEC_REFER7_BASE_LSB,
                                      JMDEC_REFER8_BASE_LSB,
                                      JMDEC_REFER9_BASE_LSB,
                                      JMDEC_REFER10_BASE_LSB,
                                      JMDEC_REFER11_BASE_LSB,
                                      JMDEC_REFER12_BASE_LSB,
                                      JMDEC_REFER13_BASE_LSB,
                                      JMDEC_REFER14_BASE_LSB,
                                      JMDEC_REFER15_BASE_LSB};
static const uint32_t ref_base_msb[16] = {JMDEC_REFER0_BASE_MSB,
                                          JMDEC_REFER1_BASE_MSB,
                                          JMDEC_REFER2_BASE_MSB,
                                          JMDEC_REFER3_BASE_MSB,
                                          JMDEC_REFER4_BASE_MSB,
                                          JMDEC_REFER5_BASE_MSB,
                                          JMDEC_REFER6_BASE_MSB,
                                          JMDEC_REFER7_BASE_MSB,
                                          JMDEC_REFER8_BASE_MSB,
                                          JMDEC_REFER9_BASE_MSB,
                                          JMDEC_REFER10_BASE_MSB,
                                          JMDEC_REFER11_BASE_MSB,
                                          JMDEC_REFER12_BASE_MSB,
                                          JMDEC_REFER13_BASE_MSB,
                                          JMDEC_REFER14_BASE_MSB,
                                          JMDEC_REFER15_BASE_MSB};

static const uint32_t ref_cbase[16] = {JMDEC_REFER0_CBASE_LSB,
                                       JMDEC_REFER1_CBASE_LSB,
                                       JMDEC_REFER2_CBASE_LSB,
                                       JMDEC_REFER3_CBASE_LSB,
                                       JMDEC_REFER4_CBASE_LSB,
                                       JMDEC_REFER5_CBASE_LSB,
                                       JMDEC_REFER6_CBASE_LSB,
                                       JMDEC_REFER7_CBASE_LSB,
                                       JMDEC_REFER8_CBASE_LSB,
                                       JMDEC_REFER9_CBASE_LSB,
                                       JMDEC_REFER10_CBASE_LSB,
                                       JMDEC_REFER11_CBASE_LSB,
                                       JMDEC_REFER12_CBASE_LSB,
                                       JMDEC_REFER13_CBASE_LSB,
                                       JMDEC_REFER14_CBASE_LSB,
                                       JMDEC_REFER15_CBASE_LSB};

static const uint32_t ref_cbase_msb[16] = {JMDEC_REFER0_CBASE_MSB,
                                           JMDEC_REFER1_CBASE_MSB,
                                           JMDEC_REFER2_CBASE_MSB,
                                           JMDEC_REFER3_CBASE_MSB,
                                           JMDEC_REFER4_CBASE_MSB,
                                           JMDEC_REFER5_CBASE_MSB,
                                           JMDEC_REFER6_CBASE_MSB,
                                           JMDEC_REFER7_CBASE_MSB,
                                           JMDEC_REFER8_CBASE_MSB,
                                           JMDEC_REFER9_CBASE_MSB,
                                           JMDEC_REFER10_CBASE_MSB,
                                           JMDEC_REFER11_CBASE_MSB,
                                           JMDEC_REFER12_CBASE_MSB,
                                           JMDEC_REFER13_CBASE_MSB,
                                           JMDEC_REFER14_CBASE_MSB,
                                           JMDEC_REFER15_CBASE_MSB};

static const uint32_t ref_dbase[16] = {JMDEC_REFER0_DBASE_LSB,
                                       JMDEC_REFER1_DBASE_LSB,
                                       JMDEC_REFER2_DBASE_LSB,
                                       JMDEC_REFER3_DBASE_LSB,
                                       JMDEC_REFER4_DBASE_LSB,
                                       JMDEC_REFER5_DBASE_LSB,
                                       JMDEC_REFER6_DBASE_LSB,
                                       JMDEC_REFER7_DBASE_LSB,
                                       JMDEC_REFER8_DBASE_LSB,
                                       JMDEC_REFER9_DBASE_LSB,
                                       JMDEC_REFER10_DBASE_LSB,
                                       JMDEC_REFER11_DBASE_LSB,
                                       JMDEC_REFER12_DBASE_LSB,
                                       JMDEC_REFER13_DBASE_LSB,
                                       JMDEC_REFER14_DBASE_LSB,
                                       JMDEC_REFER15_DBASE_LSB};

static const uint32_t ref_dbase_msb[16] = {JMDEC_REFER0_DBASE_MSB,
                                           JMDEC_REFER1_DBASE_MSB,
                                           JMDEC_REFER2_DBASE_MSB,
                                           JMDEC_REFER3_DBASE_MSB,
                                           JMDEC_REFER4_DBASE_MSB,
                                           JMDEC_REFER5_DBASE_MSB,
                                           JMDEC_REFER6_DBASE_MSB,
                                           JMDEC_REFER7_DBASE_MSB,
                                           JMDEC_REFER8_DBASE_MSB,
                                           JMDEC_REFER9_DBASE_MSB,
                                           JMDEC_REFER10_DBASE_MSB,
                                           JMDEC_REFER11_DBASE_MSB,
                                           JMDEC_REFER12_DBASE_MSB,
                                           JMDEC_REFER13_DBASE_MSB,
                                           JMDEC_REFER14_DBASE_MSB,
                                           JMDEC_REFER15_DBASE_MSB};

static const uint32_t ref_tybase[16] = {JMDEC_REFER0_TYBASE_LSB,
                                        JMDEC_REFER1_TYBASE_LSB,
                                        JMDEC_REFER2_TYBASE_LSB,
                                        JMDEC_REFER3_TYBASE_LSB,
                                        JMDEC_REFER4_TYBASE_LSB,
                                        JMDEC_REFER5_TYBASE_LSB,
                                        JMDEC_REFER6_TYBASE_LSB,
                                        JMDEC_REFER7_TYBASE_LSB,
                                        JMDEC_REFER8_TYBASE_LSB,
                                        JMDEC_REFER9_TYBASE_LSB,
                                        JMDEC_REFER10_TYBASE_LSB,
                                        JMDEC_REFER11_TYBASE_LSB,
                                        JMDEC_REFER12_TYBASE_LSB,
                                        JMDEC_REFER13_TYBASE_LSB,
                                        JMDEC_REFER14_TYBASE_LSB,
                                        JMDEC_REFER15_TYBASE_LSB};

static const uint32_t ref_tybase_msb[16] = {JMDEC_REFER0_TYBASE_MSB,
                                            JMDEC_REFER1_TYBASE_MSB,
                                            JMDEC_REFER2_TYBASE_MSB,
                                            JMDEC_REFER3_TYBASE_MSB,
                                            JMDEC_REFER4_TYBASE_MSB,
                                            JMDEC_REFER5_TYBASE_MSB,
                                            JMDEC_REFER6_TYBASE_MSB,
                                            JMDEC_REFER7_TYBASE_MSB,
                                            JMDEC_REFER8_TYBASE_MSB,
                                            JMDEC_REFER9_TYBASE_MSB,
                                            JMDEC_REFER10_TYBASE_MSB,
                                            JMDEC_REFER11_TYBASE_MSB,
                                            JMDEC_REFER12_TYBASE_MSB,
                                            JMDEC_REFER13_TYBASE_MSB,
                                            JMDEC_REFER14_TYBASE_MSB,
                                            JMDEC_REFER15_TYBASE_MSB};

static const uint32_t ref_tcbase[16] = {JMDEC_REFER0_TCBASE_LSB,
                                        JMDEC_REFER1_TCBASE_LSB,
                                        JMDEC_REFER2_TCBASE_LSB,
                                        JMDEC_REFER3_TCBASE_LSB,
                                        JMDEC_REFER4_TCBASE_LSB,
                                        JMDEC_REFER5_TCBASE_LSB,
                                        JMDEC_REFER6_TCBASE_LSB,
                                        JMDEC_REFER7_TCBASE_LSB,
                                        JMDEC_REFER8_TCBASE_LSB,
                                        JMDEC_REFER9_TCBASE_LSB,
                                        JMDEC_REFER10_TCBASE_LSB,
                                        JMDEC_REFER11_TCBASE_LSB,
                                        JMDEC_REFER12_TCBASE_LSB,
                                        JMDEC_REFER13_TCBASE_LSB,
                                        JMDEC_REFER14_TCBASE_LSB,
                                        JMDEC_REFER15_TCBASE_LSB};

static const uint32_t ref_tcbase_msb[16] = {JMDEC_REFER0_TCBASE_MSB,
                                            JMDEC_REFER1_TCBASE_MSB,
                                            JMDEC_REFER2_TCBASE_MSB,
                                            JMDEC_REFER3_TCBASE_MSB,
                                            JMDEC_REFER4_TCBASE_MSB,
                                            JMDEC_REFER5_TCBASE_MSB,
                                            JMDEC_REFER6_TCBASE_MSB,
                                            JMDEC_REFER7_TCBASE_MSB,
                                            JMDEC_REFER8_TCBASE_MSB,
                                            JMDEC_REFER9_TCBASE_MSB,
                                            JMDEC_REFER10_TCBASE_MSB,
                                            JMDEC_REFER11_TCBASE_MSB,
                                            JMDEC_REFER12_TCBASE_MSB,
                                            JMDEC_REFER13_TCBASE_MSB,
                                            JMDEC_REFER14_TCBASE_MSB,
                                            JMDEC_REFER15_TCBASE_MSB};

#if 0
static const uint32_t ref_field_mode[16] = {
    JMDEC_REFER0_FIELD_E, JMDEC_REFER1_FIELD_E, JMDEC_REFER2_FIELD_E,
    JMDEC_REFER3_FIELD_E, JMDEC_REFER4_FIELD_E, JMDEC_REFER5_FIELD_E,
    JMDEC_REFER6_FIELD_E, JMDEC_REFER7_FIELD_E, JMDEC_REFER8_FIELD_E,
    JMDEC_REFER9_FIELD_E, JMDEC_REFER10_FIELD_E, JMDEC_REFER11_FIELD_E,
    JMDEC_REFER12_FIELD_E, JMDEC_REFER13_FIELD_E, JMDEC_REFER14_FIELD_E,
    JMDEC_REFER15_FIELD_E
};
#endif

static const uint32_t ref_pic_num[16] = {JMDEC_REFER0_NBR,
                                         JMDEC_REFER1_NBR,
                                         JMDEC_REFER2_NBR,
                                         JMDEC_REFER3_NBR,
                                         JMDEC_REFER4_NBR,
                                         JMDEC_REFER5_NBR,
                                         JMDEC_REFER6_NBR,
                                         JMDEC_REFER7_NBR,
                                         JMDEC_REFER8_NBR,
                                         JMDEC_REFER9_NBR,
                                         JMDEC_REFER10_NBR,
                                         JMDEC_REFER11_NBR,
                                         JMDEC_REFER12_NBR,
                                         JMDEC_REFER13_NBR,
                                         JMDEC_REFER14_NBR,
                                         JMDEC_REFER15_NBR};

static const uint32_t ref_pic_list_p[16] = {JMDEC_PINIT_RLIST_F0,
                                            JMDEC_PINIT_RLIST_F1,
                                            JMDEC_PINIT_RLIST_F2,
                                            JMDEC_PINIT_RLIST_F3,
                                            JMDEC_PINIT_RLIST_F4,
                                            JMDEC_PINIT_RLIST_F5,
                                            JMDEC_PINIT_RLIST_F6,
                                            JMDEC_PINIT_RLIST_F7,
                                            JMDEC_PINIT_RLIST_F8,
                                            JMDEC_PINIT_RLIST_F9,
                                            JMDEC_PINIT_RLIST_F10,
                                            JMDEC_PINIT_RLIST_F11,
                                            JMDEC_PINIT_RLIST_F12,
                                            JMDEC_PINIT_RLIST_F13,
                                            JMDEC_PINIT_RLIST_F14,
                                            JMDEC_PINIT_RLIST_F15};

static const uint32_t ref_pic_list0[16] = {JMDEC_INIT_RLIST_F0,
                                           JMDEC_INIT_RLIST_F1,
                                           JMDEC_INIT_RLIST_F2,
                                           JMDEC_INIT_RLIST_F3,
                                           JMDEC_INIT_RLIST_F4,
                                           JMDEC_INIT_RLIST_F5,
                                           JMDEC_INIT_RLIST_F6,
                                           JMDEC_INIT_RLIST_F7,
                                           JMDEC_INIT_RLIST_F8,
                                           JMDEC_INIT_RLIST_F9,
                                           JMDEC_INIT_RLIST_F10,
                                           JMDEC_INIT_RLIST_F11,
                                           JMDEC_INIT_RLIST_F12,
                                           JMDEC_INIT_RLIST_F13,
                                           JMDEC_INIT_RLIST_F14,
                                           JMDEC_INIT_RLIST_F15};

static const uint32_t ref_pic_list1[16] = {JMDEC_INIT_RLIST_B0,
                                           JMDEC_INIT_RLIST_B1,
                                           JMDEC_INIT_RLIST_B2,
                                           JMDEC_INIT_RLIST_B3,
                                           JMDEC_INIT_RLIST_B4,
                                           JMDEC_INIT_RLIST_B5,
                                           JMDEC_INIT_RLIST_B6,
                                           JMDEC_INIT_RLIST_B7,
                                           JMDEC_INIT_RLIST_B8,
                                           JMDEC_INIT_RLIST_B9,
                                           JMDEC_INIT_RLIST_B10,
                                           JMDEC_INIT_RLIST_B11,
                                           JMDEC_INIT_RLIST_B12,
                                           JMDEC_INIT_RLIST_B13,
                                           JMDEC_INIT_RLIST_B14,
                                           JMDEC_INIT_RLIST_B15};
static const uint32_t cabac_init_values[4 * 460 * 2 / 4] = {
   0x14f10236, 0x034a14f1, 0x0236034a, 0xe47fe968, 0xfa35ff36, 0x07330000,
   0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
   0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
   0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
   0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
   0x0029003f, 0x003f003f, 0xf7530456, 0x0061f948, 0x0d29033e, 0x000b0137,
   0x0045ef7f, 0xf3660052, 0xf94aeb6b, 0xe57fe17f, 0xe87fee5f, 0xe57feb72,
   0xe27fef7b, 0xf473f07a, 0xf573f43f, 0xfe44f154, 0xf368fd46, 0xf85df65a,
   0xe27fff4a, 0xfa61f95b, 0xec7ffc38, 0xfb52f94c, 0xea7df95d, 0xf557fd4d,
   0xfb47fc3f, 0xfc44f454, 0xf93ef941, 0x083d0538, 0xfe420140, 0x003dfe4e,
   0x01320734, 0x0a23002c, 0x0b26012d, 0x002e052c, 0x1f110133, 0x07321c13,
   0x10210e3e, 0xf36cf164, 0xf365f35b, 0xf45ef658, 0xf054f656, 0xf953f357,
   0xed5e0146, 0x0048fb4a, 0x123bf866, 0xf164005f, 0xfc4b0248, 0xf54bfd47,
   0x0f2ef345, 0x003e0041, 0x1525f148, 0x09391036, 0x003e0c48, 0x18000f09,
   0x08190d12, 0x0f090d13, 0x0a250c12, 0x061d1421, 0x0f1e042d, 0x013a003e,
   0x073d0c26, 0x0b2d0f27, 0x0b2a0d2c, 0x102d0c29, 0x0a311e22, 0x122a0a37,
   0x1133112e, 0x00591aed, 0x16ef1aef, 0x1ee71cec, 0x21e925e5, 0x21e928e4,
   0x26ef21f5, 0x28f129fa, 0x26012911, 0x1efa1b03, 0x1a1625f0, 0x23fc26f8,
   0x26fd2503, 0x26052a00, 0x23102716, 0x0e301b25, 0x153c0c44, 0x0261fd47,
   0xfa2afb32, 0xfd36fe3e, 0x003a013f, 0xfe48ff4a, 0xf75bfb43, 0xfb1bfd27,
   0xfe2c002e, 0xf040f844, 0xf64efa4d, 0xf656f45c, 0xf137f63c, 0xfa3efc41,
   0xf449f84c, 0xf950f758, 0xef6ef561, 0xec54f54f, 0xfa49fc4a, 0xf356f360,
   0xf561ed75, 0xf84efb21, 0xfc30fe35, 0xfd3ef347, 0xf64ff456, 0xf35af261,
   0x0000fa5d, 0xfa54f84f, 0x0042ff47, 0x003efe3c, 0xfe3bfb4b, 0xfd3efc3a,
   0xf742ff4f, 0x00470344, 0x0a2cf93e, 0x0f240e28, 0x101b0c1d, 0x012c1424,
   0x1220052a, 0x01300a3e, 0x112e0940, 0xf468f561, 0xf060f958, 0xf855f955,
   0xf755f358, 0x0442fd4d, 0xfd4cfa4c, 0x0a3aff4c, 0xff53f963, 0xf25f025f,
   0x004cfb4a, 0x0046f54b, 0x01440041, 0xf249033e, 0x043eff44, 0xf34b0b37,
   0x05400c46, 0x0f060613, 0x07100c0e, 0x120d0d0b, 0x0d0f0f10, 0x0c170d17,
   0x0f140e1a, 0x0e2c1128, 0x112f1811, 0x15151916, 0x1f1b161d, 0x13230e32,
   0x0a39073f, 0xfe4dfc52, 0xfd5e0945, 0xf46d24dd, 0x24de20e6, 0x25e22ce0,
   0x22ee22f1, 0x28f121f9, 0x23fb2100, 0x2602210d, 0x17230d3a, 0x1dfd1a00,
   0x161e1ff9, 0x23f122fd, 0x220324ff, 0x2205200b, 0x2305220c, 0x270b1e1d,
   0x221a1d27, 0x13421f15, 0x1f1f1932, 0xef78ec70, 0xee72f555, 0xf15cf259,
   0xe647f151, 0xf2500044, 0xf246e838, 0xe944e832, 0xf54a17f3, 0x1af328f1,
   0x31f22c03, 0x2d062c22, 0x21361352, 0xfd4bff17, 0x0122012b, 0x0036fe37,
   0x003d0140, 0x0044f75c, 0xf26af361, 0xf15af45a, 0xee58f649, 0xf74ff256,
   0xf649f646, 0xf645fb42, 0xf740fb3a, 0x023b15f6, 0x18f51cf8, 0x1cff1d03,
   0x1d092314, 0x1d240e43, 0x14f10236, 0x034a14f1, 0x0236034a, 0xe47fe968,
   0xfa35ff36, 0x07331721, 0x17021500, 0x01090031, 0xdb760539, 0xf34ef541,
   0x013e0c31, 0xfc491132, 0x1240092b, 0x1d001a43, 0x105a0968, 0xd27fec68,
   0x0143f34e, 0xf541013e, 0xfa56ef5f, 0xfa3d092d, 0xfd45fa51, 0xf5600637,
   0x0743fb56, 0x0258003a, 0xfd4cf65e, 0x05360445, 0xfd510058, 0xf943fb4a,
   0xfc4afb50, 0xf948013a, 0x0029003f, 0x003f003f, 0xf7530456, 0x0061f948,
   0x0d29033e, 0x002dfc4e, 0xfd60e57e, 0xe462e765, 0xe943e452, 0xec5ef053,
   0xea6eeb5b, 0xee66f35d, 0xe37ff95c, 0xfb59f960, 0xf36cfd2e, 0xff41ff39,
   0xf75dfd4a, 0xf75cf857, 0xe97e0536, 0x063c063b, 0x0645ff30, 0x0044fc45,
   0xf858fe55, 0xfa4eff4b, 0xf94d0236, 0x0532fd44, 0x0132062a, 0xfc51013f,
   0xfc460043, 0x0239fe4c, 0x0b230440, 0x013d0b23, 0x12190c18, 0x0d1d0d24,
   0xf65df949, 0xfe490d2e, 0x0931f964, 0x09350235, 0x0535fe3d, 0x00380038,
   0xf33ffb3c, 0xff3e0439, 0xfa450439, 0x0e270433, 0x0d440340, 0x013d093f,
   0x07321027, 0x052c0434, 0x0b30fb3c, 0xff3b003b, 0x1621052c, 0x0e2bff4e,
   0x003c0945, 0x0b1c0228, 0x032c0031, 0x002e022c, 0x0233002f, 0x0427023e,
   0x062e0036, 0x0336023a, 0x043f0633, 0x06390735, 0x06340637, 0x0b2d0e24,
   0x0835ff52, 0x0737fd4e, 0x0f2e161f, 0xff541907, 0x1ef91c03, 0x1c042000,
   0x22ff1e06, 0x1e062009, 0x1f131a1b, 0x1a1e2514, 0x1c221146, 0x0143053b,
   0x0943101e, 0x12201223, 0x161d181f, 0x1726122b, 0x14290b3f, 0x093b0940,
   0xff5efe59, 0xf76cfa4c, 0xfe2c002d, 0x0034fd40, 0xfe3bfc46, 0xfc4bf852,
   0xef66f74d, 0x0318002a, 0x00300037, 0xfa3bf947, 0xf453f557, 0xe277013a,
   0xfd1dff24, 0x0126022b, 0xfa37003a, 0x0040fd4a, 0xf65a0046, 0xfc1d051f,
   0x072a013b, 0xfe3afd48, 0xfd51f561, 0x003a0805, 0x0a0e0e12, 0x0d1b0228,
   0x003afd46, 0xfa4ff855, 0x0000f36a, 0xf06af657, 0xeb72ee6e, 0xf262ea6e,
   0xeb6aee67, 0xeb6be96c, 0xe670f660, 0xf45ffb5b, 0xf75dea5e, 0xfb560943,
   0xfc50f655, 0xff46073c, 0x093a053d, 0x0c320f32, 0x12311136, 0x0a29072e,
   0xff330731, 0x08340929, 0x062f0237, 0x0d290a2c, 0x06320535, 0x0d31043f,
   0x0640fe45, 0xfe3b0646, 0x0a2c091f, 0x0c2b0335, 0x0e220a26, 0xfd340d28,
   0x1120072c, 0x07260d32, 0x0a391a2b, 0x0e0b0b0e, 0x090b120b, 0x150917fe,
   0x20f120f1, 0x22eb27e9, 0x2adf29e1, 0x2ee426f4, 0x151d2de8, 0x35d330e6,
   0x41d52bed, 0x27f61e09, 0x121a141b, 0x0039f252, 0xfb4bed61, 0xdd7d1b00,
   0x1c001ffc, 0x1b062208, 0x1e0a1816, 0x21131620, 0x1a1f1529, 0x1a2c172f,
   0x10410e47, 0x083c063f, 0x11411518, 0x17141a17, 0x1b201c17, 0x1c181728,
   0x18201c1d, 0x172a1339, 0x1635163d, 0x0b560c28, 0x0b330e3b, 0xfc4ff947,
   0xfb45f746, 0xf842f644, 0xed49f445, 0xf046f143, 0xec3eed46, 0xf042ea41,
   0xec3f09fe, 0x1af721f7, 0x27f929fe, 0x2d033109, 0x2d1b243b, 0xfa42f923,
   0xf92af82d, 0xfb30f438, 0xfa3cfb3e, 0xf842f84c, 0xfb55fa51, 0xf64df951,
   0xef50ee49, 0xfc4af653, 0xf747f743, 0xff3df842, 0xf242003b, 0x023b15f3,
   0x21f227f9, 0x2efe3302, 0x3c063d11, 0x37222a3e, 0x14f10236, 0x034a14f1,
   0x0236034a, 0xe47fe968, 0xfa35ff36, 0x07331619, 0x22001000, 0xfe090429,
   0xe3760241, 0xfa47f34f, 0x05340932, 0xfd460a36, 0x1a221316, 0x28003902,
   0x29241a45, 0xd37ff165, 0xfc4cfa47, 0xf34f0534, 0x0645f35a, 0x0034082b,
   0xfe45fb52, 0xf660023b, 0x024bfd57, 0xfd640138, 0xfd4afa55, 0x003bfd51,
   0xf956fb5f, 0xff42ff4d, 0x0146fe56, 0xfb48003d, 0x0029003f, 0x003f003f,
   0xf7530456, 0x0061f948, 0x0d29033e, 0x0d0f0733, 0x0250d97f, 0xee5bef60,
   0xe651dd62, 0xe866e961, 0xe577e863, 0xeb6eee66, 0xdc7f0050, 0xfb59f95e,
   0xfc5c0027, 0x0041f154, 0xdd7ffe49, 0xf468f75b, 0xe17f0337, 0x07380737,
   0x083dfd35, 0x0044f94a, 0xf758f367, 0xf35bf759, 0xf25cf84c, 0xf457e96e,
   0xe869f64e, 0xec70ef63, 0xb27fba7f, 0xce7fd27f, 0xfc42fb4e, 0xfc47f848,
   0x023bff37, 0xf946fa4b, 0xf859de77, 0xfd4b2014, 0x1e16d47f, 0x0036fb3d,
   0x003aff3c, 0xfd3df843, 0xe754f24a, 0xfb410534, 0x0239003d, 0xf745f546,
   0x1237fc47, 0x003a073d, 0x09291219, 0x0920052b, 0x092f002c, 0x0033022e,
   0x1326fc42, 0x0f260c2a, 0x09220059, 0x042d0a1c, 0x0a1f21f5, 0x34d5120f,
   0x1c0023ea, 0x26e72200, 0x27ee20f4, 0x66a20000, 0x38f121fc, 0x1d0a25fb,
   0x33e327f7, 0x34de45c6, 0x43c12cfb, 0x200737e3, 0x20010000, 0x1b2421e7,
   0x22e224e4, 0x26e426e5, 0x22ee23f0, 0x22f220f8, 0x25fa2300, 0x1e0a1c12,
   0x1a191d29, 0x004b0248, 0x084d0e23, 0x121f1123, 0x151e112d, 0x142a122d,
   0x1b1a1036, 0x07421038, 0x0b490a43, 0xf674e970, 0xf147f93d, 0x0035fb42,
   0xf54df750, 0xf754f657, 0xde7feb65, 0xfd27fb35, 0xf93df54b, 0xf14def5b,
   0xe76be76f, 0xe47af54c, 0xf62cf634, 0xf639f73a, 0xf048f945, 0xfc45fb4a,
   0xf7560242, 0xf7220120, 0x0b1f0534, 0xfe37fe43, 0x0049f859, 0x03340704,
   0x0a081108, 0x10130325, 0xff3dfb49, 0xff46fc4e, 0x0000eb7e, 0xe97cec6e,
   0xe67ee77c, 0xef69e579, 0xe575ef66, 0xe675e574, 0xdf7af65f, 0xf264f85f,
   0xef6fe472, 0xfa59fe50, 0xfc52f755, 0xf851ff48, 0x05400143, 0x09380045,
   0x01450745, 0xf945fa43, 0xf04dfe40, 0x023dfa43, 0xfd400239, 0xfd41fd42,
   0x003e0933, 0xff42fe47, 0xfe4bff46, 0xf7480e3c, 0x1025002f, 0x12230b25,
   0x0c290a29, 0x02300c29, 0x0d29003b, 0x03321328, 0x03421232, 0x13fa12fa,
   0x0e001af4, 0x1ff021e7, 0x21ea25e4, 0x27e22ae2, 0x2fd62ddc, 0x31de29ef,
   0x200945b9, 0x3fc142c0, 0x4db636d9, 0x34dd29f6, 0x240028ff, 0x1e0e1c1a,
   0x17250c37, 0x0b4125df, 0x27dc28db, 0x26e22edf, 0x2ae228e8, 0x31e326f4,
   0x28f626fd, 0x2efb1f14, 0x1d1e192c, 0x0c300b31, 0x1a2d1616, 0x17161b15,
   0x21141a1c, 0x1e181b22, 0x122a1927, 0x12320c46, 0x15360e47, 0x0b531920,
   0x15311536, 0xfb55fa51, 0xf64df951, 0xef50ee49, 0xfc4af653, 0xf747f743,
   0xff3df842, 0xf242003b, 0x023b11f6, 0x20f32af7, 0x31fb3500, 0x4003440a,
   0x421b2f39, 0xfb470018, 0xff24fe2a, 0xfe34f739, 0xfa3ffc41, 0xfc43f952,
   0xfd51fd4c, 0xf948fa4e, 0xf448f244, 0xfd46fa4c, 0xfb42fb3e, 0x0039fc3d,
   0xf73c0136, 0x023a11f6, 0x20f32af7, 0x31fb3500, 0x4003440a, 0x421b2f39,
   0x14f10236, 0x034a14f1, 0x0236034a, 0xe47fe968, 0xfa35ff36, 0x07331d10,
   0x19000e00, 0xf633fd3e, 0xe5631a10, 0xfc55e866, 0x05390639, 0xef490e39,
   0x1428140a, 0x1d003600, 0x252a0c61, 0xe07fea75, 0xfe4afc55, 0xe8660539,
   0xfa5df258, 0xfa2c0437, 0xf559f167, 0xeb741339, 0x143a0454, 0x0660013f,
   0xfb55f36a, 0x053f064b, 0xfd5aff65, 0x0337fc4f, 0xfe4bf461, 0xf932013c,
   0x0029003f, 0x003f003f, 0xf7530456, 0x0061f948, 0x0d29033e, 0x0722f758,
   0xec7fdc7f, 0xef5bf25f, 0xe754e756, 0xf459ef5b, 0xe17ff24c, 0xee67f35a,
   0xdb7f0b50, 0x054c0254, 0x054efa37, 0x043df253, 0xdb7ffb4f, 0xf568f55b,
   0xe27f0041, 0xfe4f0048, 0xfc5cfa38, 0x0344f847, 0xf362fc56, 0xf458fb52,
   0xfd48fc43, 0xf848f059, 0xf745ff3b, 0x05420439, 0xfc47fe47, 0x023aff4a,
   0xfc2cff45, 0x003ef933, 0xfc2ffa2a, 0xfd29fa35, 0x084cf74e, 0xf5530934,
   0x0043fb5a, 0x0143f148, 0xfb4bf850, 0xeb53eb40, 0xf31fe740, 0xe35e094b,
   0x113ff84a, 0xfb23fe1b, 0x0d5b0341, 0xf945084d, 0xf642033e, 0xfd44ec51,
   0x001e0107, 0xfd17eb4a, 0x1042e97c, 0x11252cee, 0x32deea7f, 0x0427002a,
   0x07220b1d, 0x081f0625, 0x072a0328, 0x08210d2b, 0x0d24042f, 0x0337023a,
   0x063c082c, 0x0b2c0e2a, 0x07300438, 0x04340d25, 0x0931133a, 0x0a300c2d,
   0x00451421, 0x083f23ee, 0x21e71cfd, 0x180a1b00, 0x22f234d4, 0x27e81311,
   0x1f19241d, 0x1821220f, 0x1e141649, 0x1422131f, 0x1b2c1310, 0x0f240f24,
   0x151c1915, 0x1e141f0c, 0x1b10182a, 0x005d0e38, 0x0f391a26, 0xe87fe873,
   0xea52f73e, 0x0035003b, 0xf255f359, 0xf35ef55c, 0xe37feb64, 0xf239f443,
   0xf547f64d, 0xeb55f058, 0xe968f162, 0xdb7ff652, 0xf830f83d, 0xf842f946,
   0xf24bf64f, 0xf753f45c, 0xee6cfc4f, 0xea45f04b, 0xfe3a013a, 0xf34ef753,
   0xfc51f363, 0xf351fa26, 0xf33efa3a, 0xfe3bf049, 0xf64cf356, 0xf753f657,
   0x0000ea7f, 0xe77fe778, 0xe57fed72, 0xe975e776, 0xe675e871, 0xe476e178,
   0xdb7cf65e, 0xf166f663, 0xf36ace7f, 0xfb5c1139, 0xfb56f35e, 0xf45bfe4d,
   0x0047ff49, 0x0440f951, 0x05400f39, 0x01430044, 0xf6430144, 0x004d0240,
   0x0044fb4e, 0x0737053b, 0x02410e36, 0x0f2c053c, 0x0246fe4c, 0xee560c46,
   0x0540f446, 0x0b370538, 0x00450241, 0xfa4a0536, 0x0736fa4c, 0xf552fe4d,
   0xfe4d192a, 0x11f310f7, 0x11f41beb, 0x25e229d8, 0x2ad730d1, 0x27e02ed8,
   0x34cd2ed7, 0x34d92bed, 0x200b3dc9, 0x38d23ece, 0x51bd2dec, 0x23fe1c0f,
   0x22012701, 0x1e111426, 0x122d0f36, 0x004f24f0, 0x25f225ef, 0x2001220f,
   0x1d0f1819, 0x22161f10, 0x23121f1c, 0x2129241c, 0x1b2f153e, 0x121f131a,
   0x24181817, 0x1b10181e, 0x1f1d1629, 0x162a103c, 0x0f340e3c, 0x034ef07b,
   0x15351638, 0x193d1521, 0x1332113d, 0xfd4ef84a, 0xf748f648, 0xee4bf447,
   0xf53ffb46, 0xef4bf248, 0xf043f835, 0xf23bf734, 0xf54409fe, 0x1ef61ffc,
   0x21ff2107, 0x1f0c2517, 0x1f261440, 0xf747f925, 0xf82cf531, 0xf638f43b,
   0xf83ff743, 0xfa44f64f, 0xfd4ef84a, 0xf748f648, 0xee4bf447, 0xf53ffb46,
   0xef4bf248, 0xf043f835, 0xf23bf734, 0xf54409fe, 0x1ef61ffc, 0x21ff2107,
   0x1f0c2517, 0x1f261440};

static void
dec_h264_set_common_regs(struct mwv207_video_decoder *decoder,
                         struct mwv207_h264_picture_desc *h264)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;

   if (dec->dec_param.rlc_mode
          == 0
       && h264->pps.sps.frame_mbs_only_flag) {
      dec->dec_param.high10p_mode = 1;
      reg_buf->set_reg(reg_buf, JMDEC_DEC_MODE, DEC_MODE_H264_HIGH10P);
      reg_buf->set_reg(reg_buf, JMDEC_RLC_MODE_E, 0);
      reg_buf->set_reg(reg_buf, JMDEC_BUFF_ID, 0);
   } else {
      dec->dec_param.high10p_mode = 0;
      reg_buf->set_reg(reg_buf, JMDEC_DEC_MODE, DEC_MODE_H264);
      reg_buf->set_reg(reg_buf, JMDEC_RLC_MODE_E, 0);

      reg_buf->set_reg(reg_buf, JMDEC_DEC_LATENCY, 0);
      reg_buf->set_reg(reg_buf, JMDEC_DEC_CLK_GATE_E, 0);
      reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_ENDIAN, 1);
      reg_buf->set_reg(reg_buf, JMDEC_DEC_MAX_BURST, 16);
      reg_buf->set_reg(reg_buf, JMDEC_DEC_DATA_DISC_E, 0);
      reg_buf->set_reg(reg_buf, JMDEC_SERV_MERGE_DIS, 0);
      reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_DIS, 0);
      reg_buf->set_reg(reg_buf, JMDEC_CH_8PIX_ILEAV_E, 0);
      reg_buf->set_reg(reg_buf, JMDEC_PP_OUT_E_U, dec->dec_param.pp_enabled);
      reg_buf->set_reg(reg_buf, JMDEC_PP_CR_FIRST, dec->dec_param.cr_first);
   }

   mwv207_dec_set_common_config_regs(dec);

   reg_buf->set_reg(reg_buf, JMDEC_PRED_BC_TAP_0_0, 1);
   reg_buf->set_reg(reg_buf, JMDEC_PRED_BC_TAP_0_1, (uint32_t)(-5));
   reg_buf->set_reg(reg_buf, JMDEC_PRED_BC_TAP_0_2, 20);
   reg_buf->set_reg(reg_buf, JMDEC_BUFFER_EMPTY_INT_E, 1);
   reg_buf->set_reg(reg_buf, JMDEC_BLOCK_BUFFER_MODE_E, 0);
}

static void
dec_h264_set_picparam_regs(struct mwv207_video_decoder *decoder,
                           struct mwv207_h264_picture_desc *pic_param)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;

   struct mwv207_video_dec_hw_features *hw_feature = &dec->hw_feature;

   int i = 0;

   uint32_t curr_pic_width_in_mbs = align(decoder->base.width, 16) / 16;
   uint32_t curr_pic_height_in_mbs = align(decoder->base.height, 16) / 16;

   if (dec->dec_param.mc_enabled) {
      reg_buf->set_reg(reg_buf, JMDEC_DEC_MULTICORE_E, 1);
      reg_buf->set_reg(reg_buf, JMDEC_DEC_WRITESTAT_E, 1);
   } else {
      reg_buf->set_reg(reg_buf, JMDEC_DEC_MULTICORE_E, 0);
      reg_buf->set_reg(reg_buf, JMDEC_DEC_WRITESTAT_E, 0);
   }

   reg_buf->set_reg(reg_buf, JMDEC_MIN_CB_SIZE, 3);
   reg_buf->set_reg(reg_buf, JMDEC_MAX_CB_SIZE, 4);
   reg_buf->set_reg(
      reg_buf, JMDEC_PIC_WIDTH_IN_CBS, curr_pic_width_in_mbs << 1);
   reg_buf->set_reg(
      reg_buf, JMDEC_PIC_HEIGHT_IN_CBS, curr_pic_height_in_mbs << 1);

   reg_buf->set_reg(reg_buf, JMDEC_PARTIAL_CTB_X, 0);
   reg_buf->set_reg(reg_buf, JMDEC_PARTIAL_CTB_Y, 0);
   reg_buf->set_reg(reg_buf, JMDEC_PIC_WIDTH_4X4, curr_pic_width_in_mbs << 2);
   reg_buf->set_reg(
      reg_buf, JMDEC_PIC_HEIGHT_4X4, curr_pic_height_in_mbs << 2);
   

   reg_buf->set_reg(reg_buf, JMDEC_IGNORE_SLICE_ERROR_E, 0);

   reg_buf->set_reg(reg_buf,
                    JMDEC_INIT_QP,
                    (uint8_t)(pic_param->pps.pic_init_qp_minus26 + 26));

   reg_buf->set_reg(reg_buf,
                    JMDEC_REF_FRAMES,
                    pic_param->num_ref_frames);

   i = 0;
   while ((1 << (pic_param->pps.sps.log2_max_frame_num_minus4 + 4)) >> i) {
      i++;
   }
   reg_buf->set_reg(reg_buf, JMDEC_FRAMENUM_LEN, i - 1);
   reg_buf->set_reg(
      reg_buf, JMDEC_FRAMENUM, pic_param->frame_num & ~(0x1000));
   reg_buf->set_reg(reg_buf,
                    JMDEC_CONST_INTRA_E,
                    pic_param->pps.constrained_intra_pred_flag);
   reg_buf->set_reg(reg_buf,
                    JMDEC_FILT_CTRL_PRES,
                    pic_param->pps.deblocking_filter_control_present_flag);
   reg_buf->set_reg(reg_buf,
                    JMDEC_RDPIC_CNT_PRES,
                    pic_param->pps.redundant_pic_cnt_present_flag);
   reg_buf->set_reg(
      reg_buf, JMDEC_CH_QP_OFFSET, pic_param->pps.chroma_qp_index_offset);
   reg_buf->set_reg(reg_buf,
                    JMDEC_CH_QP_OFFSET2,
                    pic_param->pps.second_chroma_qp_index_offset);

   reg_buf->set_reg(reg_buf,
                    JMDEC_DIR_8X8_INFER_E,
                    pic_param->pps.sps.direct_8x8_inference_flag);
   reg_buf->set_reg(
      reg_buf, JMDEC_WEIGHT_PRED_E, pic_param->pps.weighted_pred_flag);
   reg_buf->set_reg(
      reg_buf, JMDEC_WEIGHT_BIPR_IDC, pic_param->pps.weighted_bipred_idc);

   reg_buf->set_reg(reg_buf,
                    JMDEC_FIELDPIC_FLAG_E,
                    !pic_param->pps.sps.frame_mbs_only_flag);
   reg_buf->set_reg(reg_buf,
                    JMDEC_PIC_INTERLACE_E,
                    !pic_param->pps.sps.frame_mbs_only_flag
                       && (pic_param->pps.sps.mb_adaptive_frame_field_flag
                           || pic_param->field_pic_flag));

   reg_buf->set_reg(
      reg_buf, JMDEC_PIC_FIELDMODE_E, pic_param->field_pic_flag);
   reg_buf->set_reg(reg_buf,
                    JMDEC_SEQ_MBAFF_E,
                    pic_param->pps.sps.mb_adaptive_frame_field_flag);

   reg_buf->set_reg(
      reg_buf, JMDEC_8X8TRANS_FLAG_E, pic_param->pps.transform_8x8_mode_flag);

   reg_buf->set_reg(
      reg_buf, JMDEC_BLACKWHITE_E, pic_param->pps.sps.chroma_format_idc == 0);

   reg_buf->set_reg(
      reg_buf, JMDEC_CABAC_E, pic_param->pps.entropy_coding_mode_flag);

   dec->dec_param.tiled_stride_enable =
      hw_feature->tiled_mode_support && hw_feature->field_dpb_support;

   if (dec->dec_param.high10p_mode) {
      uint32_t dec_ystride, dec_cstride;
      uint32_t pixel_width = dec->dec_param.bit_depth;
      if (hw_feature->dec_stride_support) {

         if (dec->dec_param.use_video_compressor) {
            if (hw_feature->rfc_stride_support) {
               dec_ystride =
                  NEXT_MULTIPLE(8 * curr_pic_width_in_mbs * 16 * pixel_width,
                                ALIGN(dec->dec_param.w_align) * 8)
                  >> 6;
               dec_cstride =
                  NEXT_MULTIPLE(4 * curr_pic_width_in_mbs * 16 * pixel_width,
                                ALIGN(dec->dec_param.w_align) * 8)
                  >> 6;
            } else {
               /* No stride for decoder ref output when compression is
                * used. */
               dec_ystride = curr_pic_width_in_mbs * 4 * 16 * pixel_width / 8;
               dec_cstride = dec_ystride;
            }
         } else {
            if (dec->dec_param.tiled_stride_enable) {
               dec_ystride =
                  NEXT_MULTIPLE(4 * curr_pic_width_in_mbs * 16 * pixel_width,
                                (ALIGN(dec->dec_param.w_align) * 8))
                  / 8;
               dec_cstride = dec_ystride;
            } else {
               dec_ystride = curr_pic_width_in_mbs * 4 * 16 * pixel_width / 8;
               dec_cstride = dec_ystride;
            }
         }
         reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_Y_STRIDE, dec_ystride);
         reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_C_STRIDE, dec_cstride);
      }
   } else {
      if (hw_feature->dec_stride_support) {

         if (dec->dec_param.tiled_stride_enable) {
            reg_buf->set_reg(
               reg_buf,
               JMDEC_DEC_OUT_Y_STRIDE,
               NEXT_MULTIPLE((curr_pic_width_in_mbs + 1) * 4 * 16,
                             ALIGN(dec->dec_param.w_align)));
            reg_buf->set_reg(
               reg_buf,
               JMDEC_DEC_OUT_C_STRIDE,
               NEXT_MULTIPLE((curr_pic_width_in_mbs + 1) * 4 * 16,
                             ALIGN(dec->dec_param.w_align)));
         } else {
            reg_buf->set_reg(reg_buf,
                             JMDEC_DEC_OUT_Y_STRIDE,
                             curr_pic_width_in_mbs * 4 * 16);
            reg_buf->set_reg(reg_buf,
                             JMDEC_DEC_OUT_C_STRIDE,
                             curr_pic_width_in_mbs * 4 * 16);
         }
      }
   }
}

#if 0
static void
printSliceType(uint8_t slice_type)
{
   if (IS_P_SLICE(slice_type)) {
      printf("debug ref Slice type : P_SLICE  , slice type value : %d  \n",
             slice_type);
   }
   if (IS_SP_SLICE(slice_type)) {
      printf("debug ref Slice type : SP_SLICE  , slice type value : %d  \n",
             slice_type);
   }
   if (IS_I_SLICE(slice_type)) {
      printf("debug ref Slice type : I_SLICE  , slice type value : %d  \n",
             slice_type);
   }
   if (IS_B_SLICE(slice_type)) {
      printf("debug ref Slice type : B_SLICE  , slice type value : %d  \n",
             slice_type);
   }
}
#endif

static int
mwv207_dec_h264_init_poc(int pic_field_poc[2],
                         int *pic_poc,
                         const struct mwv207_h264_sps *sps,
                         H264POCContext *pc,
                         int picture_structure,
                         int nal_ref_idc)
{
   const int max_frame_num = 1 << (sps->log2_max_frame_num_minus4 + 4);
   int64_t field_poc[2];

   pc->frame_num_offset = pc->prev_frame_num_offset;
   if (pc->frame_num < pc->prev_frame_num)
      pc->frame_num_offset += max_frame_num;

   if (sps->pic_order_cnt_type == 0) {
      const int max_poc_lsb = 1
         << (sps->log2_max_pic_order_cnt_lsb_minus4 + 4);

      if (pc->poc_lsb < pc->prev_poc_lsb
          && pc->prev_poc_lsb - pc->poc_lsb >= max_poc_lsb / 2)
         pc->poc_msb = pc->prev_poc_msb + max_poc_lsb;
      else if (pc->poc_lsb > pc->prev_poc_lsb
               && pc->prev_poc_lsb - pc->poc_lsb < -max_poc_lsb / 2)
         pc->poc_msb = pc->prev_poc_msb - max_poc_lsb;
      else
         pc->poc_msb = pc->prev_poc_msb;
      field_poc[0] = field_poc[1] = pc->poc_msb + pc->poc_lsb;
      if (picture_structure == PICT_FRAME)
         field_poc[1] += pc->delta_poc_bottom;
   } else if (sps->pic_order_cnt_type == 1) {
      int abs_frame_num;
      int64_t expected_delta_per_poc_cycle, expectedpoc;
      int i;

      if (sps->num_ref_frames_in_pic_order_cnt_cycle != 0)
         abs_frame_num = pc->frame_num_offset + pc->frame_num;
      else
         abs_frame_num = 0;

      if (nal_ref_idc == 0 && abs_frame_num > 0)
         abs_frame_num--;

      expected_delta_per_poc_cycle = 0;
      for (i = 0; i < sps->num_ref_frames_in_pic_order_cnt_cycle; i++)
         
         expected_delta_per_poc_cycle += sps->offset_for_ref_frame[i];

      if (abs_frame_num > 0) {
         int poc_cycle_cnt =
            (abs_frame_num - 1) / sps->num_ref_frames_in_pic_order_cnt_cycle;
         int frame_num_in_poc_cycle =
            (abs_frame_num - 1) % sps->num_ref_frames_in_pic_order_cnt_cycle;

         expectedpoc = poc_cycle_cnt * expected_delta_per_poc_cycle;
         for (i = 0; i <= frame_num_in_poc_cycle; i++)
            expectedpoc = expectedpoc + sps->offset_for_ref_frame[i];
      } else
         expectedpoc = 0;

      if (nal_ref_idc == 0)
         expectedpoc = expectedpoc + sps->offset_for_non_ref_pic;

      field_poc[0] = expectedpoc + pc->delta_poc[0];
      field_poc[1] = field_poc[0] + sps->offset_for_top_to_bottom_field;

      if (picture_structure == PICT_FRAME)
         field_poc[1] += pc->delta_poc[1];
   } else {
      int poc = 2 * (pc->frame_num_offset + pc->frame_num);

      if (!nal_ref_idc)
         poc--;

      field_poc[0] = poc;
      field_poc[1] = poc;
   }

   if (field_poc[0] != (int)field_poc[0] || field_poc[1] != (int)field_poc[1])
      return (-1);

   if (picture_structure != PICT_BOTTOM_FIELD)
      pic_field_poc[0] = field_poc[0];
   if (picture_structure != PICT_TOP_FIELD)
      pic_field_poc[1] = field_poc[1];
   *pic_poc = MIN(pic_field_poc[0], pic_field_poc[1]);

   return 0;
}

static void
generate_sliding_window_mmcos(struct mwv207_video_dec *dec,
                              struct mwv207_h264_picture_desc *pic_param);
static void
generate_sliding_window_mmcos(struct mwv207_video_dec *dec,
                              struct mwv207_h264_picture_desc *pic_param)
{
   mwv207_h264_slice_header *h = &(dec->slice_header);
   MMCO *mmco = h->mmco;
   int nb_mmco = 0;

   if (dec->short_ref_count
       && dec->long_ref_count + dec->short_ref_count
          >= pic_param->num_ref_frames
       && !((h->picture_structure != PICT_FRAME)
            && pic_param->is_reference)) {
      mmco[0].opcode = MMCO_SHORT2UNUSED;
      mmco[0].short_pic_num =
         dec->short_ref[dec->short_ref_count - 1].frame_num;
      nb_mmco = 1;
      if (h->picture_structure != PICT_FRAME) {
         mmco[0].short_pic_num *= 2;
         mmco[1].opcode = MMCO_SHORT2UNUSED;
         mmco[1].short_pic_num = mmco[0].short_pic_num + 1;
         nb_mmco = 2;
      }
   }

   h->nb_mmco = nb_mmco;
}

/**
 * Extract structure information about the picture described by pic_num in
 * the current decoding context (frame or field). Note that pic_num is
 * picture number without wrapping (so, 0<=pic_num<max_pic_num).
 * @param pic_num picture number for which to extract structure information
 * @param structure one of PICT_XXX describing structure of picture
 *                      with pic_num
 * @return frame number (short term) or long term index of picture
 *         described by pic_num
 */
static int
pic_num_extract(struct mwv207_video_dec *h, int pic_num, int *structure)
{
   *structure = h->slice_header.picture_structure;
   if (h->slice_header.picture_structure != PICT_FRAME) {
      if (!(pic_num & 1))

         *structure ^= PICT_FRAME;
      pic_num >>= 1;
   }

   return pic_num;
}

/**
 * Remove a picture from the short term reference list by its index in
 * that list.  This does no checking on the provided index; it is assumed
 * to be valid. Other list entries are shifted down.
 * @param i index into h->short_ref of picture to remove.
 */
static void
remove_short_at_index(struct mwv207_video_dec *h, int i)
{
   assert(i >= 0 && i < h->short_ref_count);
   memset(&(h->short_ref[i]), 0, sizeof(h->short_ref[i]));
   
   if (--h->short_ref_count)
      memmove(&h->short_ref[i],
              &h->short_ref[i + 1],
              (h->short_ref_count - i) * sizeof(h->short_ref[i]));
}

/**
 * Find a H264Picture in the short term reference list by frame number.
 * @param frame_num frame number to search for
 * @param idx the index into h->short_ref where returned picture is found
 *            undefined if no picture found.
 * @return pointer to the found picture, or NULL if no pic with the provided
 *                 frame number is found
 */
static struct mwv207_picture_h264 *
find_short(struct mwv207_video_dec *h, int frame_num, int *idx)
{
   int i;

   for (i = 0; i < h->short_ref_count; i++) {
      struct mwv207_picture_h264 *pic = &(h->short_ref[i]);
      
      
      
      if (pic->frame_num == frame_num) {
         *idx = i;
         return pic;
      }
   }
   return NULL;
}

/**
 * @return the removed picture or NULL if an error occurs
 */
static struct mwv207_picture_h264 *
remove_short(struct mwv207_video_dec *h, int frame_num, int ref_mask)
{
   struct mwv207_picture_h264 *pic;
   int i;

   
   
   

   pic = find_short(h, frame_num, &i);
   if (pic) {
      
      remove_short_at_index(h, i);
   }

   return pic;
}

/**
 * Remove a picture from the long term reference list by its index in
 * that list.
 * @return the removed picture or NULL if an error occurs
 */
static struct mwv207_picture_h264 *
remove_long(struct mwv207_video_dec *h, int i, int ref_mask)
{
   struct mwv207_picture_h264 *pic;

   pic = &(h->long_ref[i]);
   if (pic && pic->picture) {
      memset(&(h->long_ref[i]), 0, sizeof(h->long_ref[i]));
      h->long_ref_count--;
      
      
      
      
      
      
   }

   return pic;
}

static int mwv207_h264_execute_ref_pic_marking(
   struct mwv207_video_dec *h,
   struct mwv207_h264_picture_desc *pic_param,
   struct pipe_video_buffer *target);

static int
mwv207_h264_execute_ref_pic_marking(
   struct mwv207_video_dec *h,
   struct mwv207_h264_picture_desc *pic_param,
   struct pipe_video_buffer *target)
{
   mwv207_h264_slice_header *sh = &(h->slice_header);
   MMCO *mmco = sh->mmco;
   int mmco_count;
   int i, (j);
   int current_ref_assigned = 0;
   struct mwv207_picture_h264 *pic;

   if (!sh->explicit_ref_marking)
      generate_sliding_window_mmcos(h, pic_param);
   mmco_count = sh->nb_mmco;

   for (i = 0; i < mmco_count; i++) {
      int structure, frame_num;

      if (mmco[i].opcode == MMCO_SHORT2UNUSED
          || mmco[i].opcode == MMCO_SHORT2LONG) {
         frame_num = pic_num_extract(h, mmco[i].short_pic_num, &structure);
         pic = find_short(h, frame_num, &j);
         if (!pic) {
            if (mmco[i].opcode != MMCO_SHORT2LONG
                || !h->long_ref[mmco[i].long_arg].picture
                || h->long_ref[mmco[i].long_arg].frame_num != frame_num) {
               printf("mmco: unref short failure\n");
            }
            continue;
         }
      }

      switch (mmco[i].opcode) {
      case MMCO_SHORT2UNUSED:
         remove_short(h, frame_num, structure ^ PICT_FRAME);
         break;
      case MMCO_SHORT2LONG:
         if (h->long_ref[mmco[i].long_arg].picture != pic->picture)
            remove_long(h, mmco[i].long_arg, 0);

         remove_short_at_index(h, j);
         memcpy(&(h->long_ref[mmco[i].long_arg]), pic, sizeof(*pic));
         if (h->long_ref[mmco[i].long_arg].picture) {
            h->long_ref[mmco[i].long_arg].is_long_term = true;
            h->long_ref_count++;
         }
         break;
      case MMCO_LONG2UNUSED:
         j = pic_num_extract(h, mmco[i].long_arg, &structure);
         pic = &(h->long_ref[j]);
         if (pic->picture) {
            remove_long(h, j, structure ^ PICT_FRAME);
         } else {
            printf("mmco: unref long failure\n");
         }
         break;
      case MMCO_LONG:
         
         
         /* First field in pair is in short term list or
          * at a different long term index.
          * This is not allowed; see 7.4.3.3, notes 2 and 3.
          * Report the problem and keep the pair where it is,
          * and mark this field valid.
          */
         if (h->short_ref[0].picture == target) {
            printf("mmco: cannot assign current picture to short and long at "
                   "the same time\n");
            remove_short_at_index(h, 0);
         }

#if 0

            if (h->cur_pic_ptr->long_ref) {
                for (j = 0; j < FF_ARRAY_ELEMS(h->long_ref); j++) {
                    if (h->long_ref[j] == h->cur_pic_ptr) {
                        if (j != mmco[i].long_arg)
                            av_log(h->avctx, AV_LOG_ERROR, "mmco: cannot assign current picture to 2 long term references\n");
                        remove_long(h, j, 0);
                    }
                }
            }
#endif

         if (h->long_ref[mmco[i].long_arg].picture != target) {
            
            remove_long(h, mmco[i].long_arg, 0);

            h->long_ref[mmco[i].long_arg].picture = target;
            h->long_ref[mmco[i].long_arg].frame_num = pic_param->frame_num;
            h->long_ref[mmco[i].long_arg].is_long_term = true;
            h->long_ref[mmco[i].long_arg].top_is_reference = true;
            h->long_ref[mmco[i].long_arg].bottom_is_reference = true;
            h->long_ref[mmco[i].long_arg].top_field_order_cnt =
               pic_param->my_field_order_cnt[0];
            h->long_ref[mmco[i].long_arg].bottom_field_order_cnt =
               pic_param->my_field_order_cnt[1];
            h->long_ref_count++;
         }

         
         current_ref_assigned = 1;
         break;
      case MMCO_SET_MAX_LONG:
         assert(mmco[i].long_arg <= 16);
         
         for (j = mmco[i].long_arg; j < 16; j++) {
            remove_long(h, j, 0);
         }
         break;
      case MMCO_RESET:
         while (h->short_ref_count) {
            remove_short(h, h->short_ref[0].frame_num, 0);
         }
         for (j = 0; j < 16; j++) {
            remove_long(h, j, 0);
         }
         h->poc.frame_num = 0;
         sh->mmco_reset = 1;
         break;
      default:
         assert(0);
      }
   }

   if (!current_ref_assigned) {
      /* Second field of complementary field pair; the first field of
       * which is already referenced. If short referenced, it
       * should be first entry in short_ref. If not, it must exist
       * in long_ref; trying to put it on the short list here is an
       * error in the encoded bit stream (ref: 7.4.3.3, NOTE 2 and 3).
       */
      if (h->short_ref_count && h->short_ref[0].picture == target) {

         
      } else {
         pic = remove_short(h, sh->frame_num, 0);
         if (pic && pic->picture) {
            printf("illegal short term buffer state detected\n");
            
         }

         if (h->short_ref_count)
            memmove(&h->short_ref[1],
                    &h->short_ref[0],
                    h->short_ref_count * sizeof(h->short_ref[0]));

         h->short_ref[0].picture = target;
         h->short_ref[0].frame_num = pic_param->frame_num;
         h->short_ref[0].is_long_term = false;
         h->short_ref[0].top_is_reference = true;
         h->short_ref[0].bottom_is_reference = true;
         h->short_ref[0].top_field_order_cnt =
            pic_param->my_field_order_cnt[0];
         h->short_ref[0].bottom_field_order_cnt =
            pic_param->my_field_order_cnt[1];
         h->short_ref_count++;
      }
   }

   if (h->long_ref_count + h->short_ref_count
       > MAX(pic_param->num_ref_frames, 1)) {

      if (h->long_ref_count && !h->short_ref_count) {
         for (i = 0; i < 16; ++i)
            if (h->long_ref[i].picture)
               break;

         assert(i < 16);
         remove_long(h, i, 0);
      } else {
         pic = &(h->short_ref[h->short_ref_count - 1]);
         remove_short(h, pic->frame_num, 0);
      }
   }

   for (i = 0; i < h->short_ref_count; i++) {
      pic = &(h->short_ref[i]);
   }

   return 0;
}

static int fill_my_ref_frames(struct mwv207_video_dec *h,
                              struct mwv207_h264_picture_desc *pic_param);
static int
fill_my_ref_frames(struct mwv207_video_dec *h,
                   struct mwv207_h264_picture_desc *pic_param)
{
   int i;

   memset(
      pic_param->my_ref_frames, 0, 16 * sizeof(struct mwv207_picture_h264));
   pic_param->num_my_ref_frames = 0;

   for (i = 0; i < h->short_ref_count; i++) {
      struct mwv207_picture_h264 *pic = &(h->short_ref[i]);
      if (pic->picture) {
         pic_param->my_ref_frames[pic_param->num_my_ref_frames++] = *pic;
      }
   }

   for (i = 0; i < 16; i++) {
      struct mwv207_picture_h264 *pic = &(h->long_ref[i]);
      if (pic->picture) {
         pic_param->my_ref_frames[pic_param->num_my_ref_frames++] = *pic;
      }
   }
   return 0;
}

void mwv207_h264_remove_all_refs(struct mwv207_video_dec *h);
void
mwv207_h264_remove_all_refs(struct mwv207_video_dec *h)
{
   int i;

   for (i = 0; i < 16; i++) {
      remove_long(h, i, 0);
   }
   assert(h->long_ref_count == 0);

   
   
   
   

   for (i = 0; i < h->short_ref_count; i++) {
      memset(&(h->short_ref[i]), 0, sizeof(h->short_ref[i]));
      
      
   }
   h->short_ref_count = 0;

   
}

/**
 * instantaneous decoder refresh.
 */
static void
idr(struct mwv207_video_dec *h)
{
   mwv207_h264_remove_all_refs(h);
   h->poc.prev_frame_num = h->poc.prev_frame_num_offset = 0;
   h->poc.prev_poc_msb = 1 << 16;
   h->poc.prev_poc_lsb = 0;
}

static void
dec_h264_set_sh_regs(struct mwv207_video_decoder *decoder,
                     struct mwv207_h264_picture_desc *pic_param,
                     struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;
   int i;
   int pic_field_poc[2] = {0};
   int pic_poc = 0;
   int picture_structure = PICT_FRAME;

   pic_param->pps.sps.chroma_format_idc = 1;
   struct mwv207_strm_data rb = {0};
   uint32_t slice_size = 0;
   uint32_t strm_buff_size = 0;
   uint8_t *address = NULL;
   mwv207_h264_nal_unit nal_unit = {0};
   slice_size = decoder->streams_len[0];
   address = decoder->streams[0];
   strm_buff_size = slice_size;
   ASSERT(address);

   rb.strm_buff_start = address;
   rb.strm_curr_pos = (uint8_t *)address;
   rb.bit_pos_in_word = 0;
   rb.strm_buff_size = strm_buff_size;
   rb.strm_data_size = slice_size;
   rb.strm_buff_read_bits = 0;
   rb.remove_emul3_byte = 0;
   rb.emul_byte_count = 0;
   rb.is_rb = 0;

   mwv207_h264_bsd_decode_nal_unit(&rb, &nal_unit);
   if (nal_unit.nal_ref_idc != 0) {
      pic_param->is_reference = 1;
   } else {
      pic_param->is_reference = 0;
   }

   mwv207_h264_bsd_decode_slice_header(
      &rb, pic_param, &dec->slice_header, &nal_unit);
   if (nal_unit.nal_unit_type == NAL_CODED_SLICE_IDR) {
      pic_param->is_idr = 1;
   } else {
      pic_param->is_idr = 0;
   }
   if (pic_param->is_idr) {

      memset(pic_param->my_ref_frames,
             0,
             16 * sizeof(struct mwv207_picture_h264));
      pic_param->num_my_ref_frames = 0;

      idr(dec);
   }

   if (pic_param->pps.sps.pic_order_cnt_type == 0) {
      dec->poc.poc_lsb = dec->slice_header.pic_order_cnt_lsb;
   }
   mwv207_dec_h264_init_poc(pic_field_poc,
                            &pic_poc,
                            &(pic_param->pps.sps),
                            &(dec->poc),
                            picture_structure,
                            nal_unit.nal_ref_idc);
   
   
   
   
   pic_param->my_field_order_cnt[0] = pic_field_poc[0];
   pic_param->my_field_order_cnt[1] = pic_field_poc[1];
   
   

   reg_buf->set_reg(reg_buf,
                    JMDEC_REFPIC_MK_LEN,
                    dec->slice_header.dec_ref_pic_marking.strm_len);

   reg_buf->set_reg(reg_buf, JMDEC_IDR_PIC_E, IS_IDR_NAL_UNIT(&nal_unit));
   if (dec->dec_param.high10p_mode) {
      reg_buf->set_reg(
         reg_buf, JMDEC_IDR_PIC_ID_H10, dec->slice_header.idr_pic_id);
   } else {
      reg_buf->set_reg(
         reg_buf, JMDEC_IDR_PIC_ID, dec->slice_header.idr_pic_id);
   }
   reg_buf->set_reg(
      reg_buf, JMDEC_PPS_ID, dec->slice_header.pic_parameter_set_id);
   reg_buf->set_reg(
      reg_buf, JMDEC_POC_LENGTH, dec->slice_header.poc_length_hw);

   reg_buf->set_reg(
      reg_buf, JMDEC_PIC_TOPFIELD_E, !pic_param->bottom_field_flag);
   reg_buf->set_reg(reg_buf,
                    JMDEC_WRITE_MVS_E,
                    nal_unit.nal_ref_idc != 0
                       || (nal_unit.nal_unit_type == NAL_CODED_SLICE_EXT));

   uint32_t num_ref_idx_active_override_flag = 1;
   for (i = 0; i < decoder->streams_num; i++) {
      slice_size = decoder->streams_len[i];
      address = decoder->streams[i];
      strm_buff_size = slice_size;
      ASSERT(address);
      
      

      rb.strm_buff_start = address;
      rb.strm_curr_pos = (uint8_t *)address;
      rb.bit_pos_in_word = 0;
      rb.strm_buff_size = strm_buff_size;
      rb.strm_data_size = slice_size;
      rb.strm_buff_read_bits = 0;
      rb.remove_emul3_byte = 0;
      rb.emul_byte_count = 0;
      rb.is_rb = 0;

      mwv207_h264_bsd_decode_nal_unit(&rb, &nal_unit);
      mwv207_h264_bsd_decode_slice_header(
         &rb, pic_param, &dec->slice_header, &nal_unit);

      if (!IS_I_SLICE(dec->slice_header.slice_type)) {
         num_ref_idx_active_override_flag =
            dec->slice_header.num_ref_idx_active_override_flag;

         if ((IS_P_SLICE(dec->slice_header.slice_type)
              || IS_SP_SLICE(dec->slice_header.slice_type))
             && num_ref_idx_active_override_flag == 0) {
            reg_buf->set_reg(reg_buf,
                             JMDEC_REFIDX0_ACTIVE,
                             pic_param->num_ref_idx_l0_active_minus1 + 1);
         }
         if (IS_B_SLICE(dec->slice_header.slice_type)
             && num_ref_idx_active_override_flag == 0) {
            reg_buf->set_reg(reg_buf,
                             JMDEC_REFIDX0_ACTIVE,
                             pic_param->num_ref_idx_l0_active_minus1 + 1);
            reg_buf->set_reg(reg_buf,
                             JMDEC_REFIDX1_ACTIVE,
                             pic_param->num_ref_idx_l1_active_minus1 + 1);
         }
      }
   }
}

static void
dec_avc_calc_sur_offset(struct mwv207_video_decoder *decoder,
                        struct mwv207_h264_picture_desc *pic_param,
                        struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_buffer *buf;

   buf = (struct mwv207_video_buffer *)target;

   uint32_t curr_pic_width_in_mbs = 0, curr_pic_height_in_mbs = 0,
            curr_pic_size_in_mbs = 0;
   curr_pic_width_in_mbs = align(decoder->base.width, 16) / 16;
   curr_pic_height_in_mbs = align(decoder->base.height, 16) / 16;
   curr_pic_size_in_mbs = curr_pic_width_in_mbs * curr_pic_height_in_mbs;

   
   uint32_t out_w = 0;
   out_w =
      NEXT_MULTIPLE(4 * curr_pic_width_in_mbs * 16 * dec->dec_param.bit_depth,
                    ALIGN(dec->dec_param.w_align) * 8)
      / 8;
   uint32_t out_h = curr_pic_height_in_mbs * 4;
   uint32_t pic_buff_size =
      out_w * out_h * (dec->dec_param.mono_chrome ? 2 : 3) / 2; 

   
   pic_buff_size +=
      curr_pic_size_in_mbs * (dec->dec_param.high10p_mode ? 80 : 64);

   if (dec->dec_param.use_video_compressor) {
      uint32_t pic_width_in_cbsy, pic_height_in_cbsy;
      uint32_t pic_width_in_cbsc, pic_height_in_cbsc;
      pic_width_in_cbsy = ((curr_pic_width_in_mbs * 16 + 8 - 1) / 8);
      pic_width_in_cbsy = NEXT_MULTIPLE(pic_width_in_cbsy, 16);
      pic_width_in_cbsc = ((curr_pic_width_in_mbs * 16 + 16 - 1) / 16);
      pic_width_in_cbsc = NEXT_MULTIPLE(pic_width_in_cbsc, 16);
      pic_height_in_cbsy = (curr_pic_height_in_mbs * 16 + 8 - 1) / 8;
      pic_height_in_cbsc = (curr_pic_height_in_mbs * 16 / 2 + 4 - 1) / 4;
      buf->mwv207_offset_info.fake_tbly_size =
         NEXT_MULTIPLE(pic_width_in_cbsy * pic_height_in_cbsy, 16);
      buf->mwv207_offset_info.fake_tblc_size =
         NEXT_MULTIPLE(pic_width_in_cbsc * pic_height_in_cbsc, 16);
   } else {
      buf->mwv207_offset_info.fake_tbly_size =
         buf->mwv207_offset_info.fake_tblc_size = 0;
   }

   pic_buff_size += 32;
   buf->mwv207_offset_info.cbs_tbl_offsety = pic_buff_size;
   buf->mwv207_offset_info.cbs_tbl_offsetc =
      buf->mwv207_offset_info.cbs_tbl_offsety
      + buf->mwv207_offset_info.fake_tbly_size;
   pic_buff_size += buf->mwv207_offset_info.fake_tbly_size
      + buf->mwv207_offset_info.fake_tblc_size;

   if (dec->dec_param.high10p_mode) {
      buf->mwv207_offset_info.sync_mc_offset =
         out_w * out_h * (dec->dec_param.mono_chrome ? 2 : 3) / 2;
      buf->mwv207_offset_info.dir_mv_offset =
         buf->mwv207_offset_info.sync_mc_offset + 32;
   } else {
      buf->mwv207_offset_info.dir_mv_offset =
         out_w * out_h * (dec->dec_param.mono_chrome ? 2 : 3) / 2;
      buf->mwv207_offset_info.sync_mc_offset = pic_buff_size - 32;
   }
   buf->mwv207_offset_info.frame_width = curr_pic_width_in_mbs;
   buf->mwv207_offset_info.frame_height = curr_pic_height_in_mbs;
   buf->mwv207_offset_info.pic_size_in_mbs = curr_pic_size_in_mbs;
   buf->tiled_pic_size = pic_buff_size;
}

static int
dec_ensure_surface_res(struct mwv207_video_decoder *decoder,
                       struct mwv207_h264_picture_desc *pic_param,
                       struct pipe_video_buffer *target)
{
   struct mwv207_video_buffer *buf;
   buf = (struct mwv207_video_buffer *)target;

   if (!buf->res) {
      mwv207_video_buffer_alloc_buffer(
         decoder->base.context, buf, buf->tiled_pic_size, PIPE_USAGE_STAGING);
   }
   return 0;
}

static void
dec_h264_avc_set_pp_regs(struct mwv207_video_decoder *decoder,
                         struct mwv207_h264_picture_desc *pic_param,
                         struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;

   mwv207_pp_unit_config ppu_ext_cfg[DEC_MAX_PPU_COUNT] = {0};

   if (dec->dec_param.pp_enabled) {
      mwv207_dec_pp_params_init(
         dec, ppu_ext_cfg, dec->dec_param.bit_depth, target);

      mwv207_pp_unit_set_int_config(dec->ppu_cfg,
                                    ppu_ext_cfg,
                                    dec->dec_param.bit_depth,
                                    pic_param->pps.sps.frame_mbs_only_flag,
                                    dec->dec_param.mono_chrome);

      uint32_t in_width = align(decoder->base.width, 16);
      uint32_t in_height = align(decoder->base.height, 16);

      for (uint32_t i = 0; i < DEC_MAX_PPU_COUNT; i++) {
         if ((dec->hw_feature.pp_up_lanczos[i]
              || dec->hw_feature.pp_down_lanczos[i])
             && (dec->ppu_cfg[i].lanczos_table == NULL)) {
            uint32_t size = LANCZOS_BUFFER_SIZE * sizeof(int32_t);

            dec->ppu_cfg[i].lanczos_table = mwv207_video_buffer_create_2(
               decoder->base.context, size, PIPE_USAGE_STAGING);
         }
      }

      if (mwv207_check_pp_unit_config(
             &dec->hw_feature, in_width, in_height, 0, dec->ppu_cfg)) {
         MWV207_LOG(MWV207_LEVEL_ERROR, "!!! pp set failed \n");
      }
      mwv207_calc_each_pp_buffer_size(
         dec->ppu_cfg, 0, dec->dec_param.mono_chrome);

      mwv207_pp_unit_int_config *ppu_cfg = &dec->ppu_cfg[0];
      uint32_t bottom_flag =
         pic_param->field_pic_flag && pic_param->bottom_field_flag;
      mwv207_pp_set_regs(reg_buf,
                         &dec->hw_feature,
                         ppu_cfg,
                         dec->dec_param.mono_chrome,
                         bottom_flag);

      reg_buf->set_reg(reg_buf, JMDEC_PP_IN_FORMAT_U, 1);
   }
}

static bool
is_short_term_frame(const struct mwv207_picture_h264 *pic)
{
   if ((pic->is_long_term == false) && pic->top_is_reference
       && pic->bottom_is_reference)
      return true;
   return false;
}

static int32_t
compare_pictures(struct mwv207_h264_picture_desc *pic_param,
                 const void *ptr1,
                 const void *ptr2)
{

   const struct mwv207_picture_h264 *pic1, *pic2;
   int32_t pic_num1, pic_num2;

   ASSERT(ptr1);
   ASSERT(ptr2);

   pic1 = (struct mwv207_picture_h264 *)ptr1;
   pic2 = (struct mwv207_picture_h264 *)ptr2;

   pic_num1 = pic1->frame_num;
   pic_num2 = pic2->frame_num;

   if ((is_short_term_frame(pic1)) && (is_short_term_frame(pic2))) {
      if ((pic_num1 >= pic_param->frame_num)
          && (pic1->picture != pic_param->cur_pic))
         pic_num1 = pic_num1
            - (1 << (pic_param->pps.sps.log2_max_frame_num_minus4 + 4));

      if ((pic_num2 >= pic_param->frame_num)
          && (pic2->picture != pic_param->cur_pic))
         pic_num2 = pic_num2
            - (1 << (pic_param->pps.sps.log2_max_frame_num_minus4 + 4));

      if (pic_num1 > pic_num2)
         return (-1);
      else if (pic_num1 < pic_num2)
         return (1);
      else
         return (0);
   }

   else if (is_short_term_frame(pic1)) {
      return (-1);
   }

   else if (is_short_term_frame(pic2)) {
      return (1);
   }
   /* both are long term reference pictures -> check pic_num (contains the
    * long_term_pic_num */
   else {
      if (pic_num1 > pic_num2)
         return (1);
      else if (pic_num1 < pic_num2)
         return (-1);
      else
         return (0);
   }
}

static int32_t
compare_pictures_b(const void *ptr1, const void *ptr2, int32_t curr_poc)
{

   struct mwv207_picture_h264 *pic1, *pic2;
   int32_t poc1, poc2;
   int32_t pic_num1, pic_num2;

   ASSERT(ptr1);
   ASSERT(ptr2);

   pic1 = (struct mwv207_picture_h264 *)ptr1;
   pic2 = (struct mwv207_picture_h264 *)ptr2;

   pic_num1 = pic1->frame_num;
   pic_num2 = pic2->frame_num;

   if ((is_short_term_frame(pic1)) && (is_short_term_frame(pic2))) {
      poc1 = (pic1->top_is_reference) ? pic1->top_field_order_cnt
         : (pic1->bottom_is_reference)
         ? pic1->bottom_field_order_cnt
         : MIN(pic1->top_field_order_cnt, pic1->bottom_field_order_cnt);

      poc2 = (pic2->top_is_reference) ? pic2->top_field_order_cnt
         : (pic2->bottom_is_reference)
         ? pic2->bottom_field_order_cnt
         : MIN(pic2->top_field_order_cnt, pic2->bottom_field_order_cnt);

      if (poc1 <= curr_poc && poc2 <= curr_poc)
         return (poc1 < poc2 ? 1 : -1);
      else
         return (poc1 < poc2 ? -1 : 1);
   }

   else if (is_short_term_frame(pic1)) {
      return (-1);
   }

   else if (is_short_term_frame(pic2)) {
      return (-1);
   }
   /* both are long term reference pictures -> check pic_num (contains the
    * long_term_pic_num */
   else {
      if (pic_num1 > pic_num2)
         return (1);
      else if (pic_num1 < pic_num2)
         return (-1);
      else
         return (0);
   }
}

static void
shell_sort(struct mwv207_video_decoder *decoder,
           struct mwv207_h264_picture_desc *pic_param,
           uint32_t *list,
           uint32_t type,
           int32_t par)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   uint32_t i, j;
   uint32_t step;
   uint32_t tmp;

   struct mwv207_picture_h264 *p_pic =
      (struct mwv207_picture_h264 *)pic_param->ref_frames;

   uint32_t num = dec->num_refs;

   step = 7;
   while (step) {
      for (i = step; i < num; i++) {
         tmp = list[i];
         j = i;
         while (j >= step
                && (type ? compare_pictures_b(
                       p_pic + list[j - step], p_pic + tmp, par)
                         : compare_pictures(
                            pic_param, p_pic + list[j - step], p_pic + tmp))
                   > 0) {
            list[j] = list[j - step];
            j -= step;
         }
         list[j] = tmp;
      }
      step >>= 1;
   }
}

static void
h264_init_ref_pic_list_1(struct mwv207_video_decoder *decoder,
                         struct mwv207_h264_picture_desc *pic_param,
                         uint32_t *list0,
                         uint32_t *list1)
{
   uint32_t tmp, i;
   uint32_t idx, idx0, idx1, idx2;
   int32_t curr_poc;
   const struct mwv207_picture_h264 *pic;

   if (pic_param->field_pic_flag) {
      curr_poc = ((pic_param->bottom_field_flag))
         ? pic_param->field_order_cnt[1]
         : pic_param->field_order_cnt[0];
   } else {
      curr_poc =
         MIN(pic_param->field_order_cnt[1], pic_param->field_order_cnt[0]);
   }

   pic = (struct mwv207_picture_h264 *)pic_param->ref_frames;
   i = 0;
   while (is_short_term_frame(&pic[list0[i]])
          && MIN(pic[list0[i]].top_field_order_cnt,
                 pic[list0[i]].bottom_field_order_cnt)
             <= curr_poc) {
      i++;
   }

   idx0 = i;

   while (is_short_term_frame(&pic[list0[i]]))
      i++;

   idx1 = i;

   if (!pic_param->field_pic_flag) {
      while (pic[list0[i]].is_long_term)
         i++;

      idx2 = i;
   } else
      idx2 = 0;

   for (i = idx0, idx = 0; i < idx1; i++, idx++)
      list1[idx] = list0[i];

   for (i = 0; i < idx0; i++, idx++)
      list1[idx] = list0[i];

   for (i = idx1; idx < 16; idx++, i++)
      list1[idx] = list0[i];

   if (idx2 > 1) {
      tmp = 0;
      for (i = 0; i < idx2; i++) {
         tmp += (list0[i] != list1[i]) ? 1 : 0;
      }

      if (!tmp) {
         i = list1[0];
         list1[0] = list1[1];
         list1[1] = i;
      }
   }
}

static void
h264_init_ref_pic_list(struct mwv207_video_decoder *decoder,
                       struct mwv207_h264_picture_desc *pic_param)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   uint32_t i;
   uint32_t num_refs = 0;
   int32_t curr_poc;

   for (i = 0; i < 34; i++)
      dec->list0[i] = dec->list1[i] = dec->list_p[i] = i;

   for (i = 0; i < 16; i++) {
      if (pic_param->ref_frames[i].picture)
         num_refs++;
   }

   dec->num_refs = num_refs;
   if (pic_param->field_pic_flag) {
      curr_poc = (pic_param->bottom_field_flag)
         ? pic_param->field_order_cnt[1]
         : pic_param->field_order_cnt[0];
   } else {
      curr_poc =
         MIN(pic_param->field_order_cnt[1], pic_param->field_order_cnt[0]);
   }

   shell_sort(decoder, pic_param, dec->list0, 1, curr_poc);

   h264_init_ref_pic_list_1(decoder, pic_param, dec->list0, dec->list1);

   shell_sort(decoder, pic_param, dec->list_p, 0, 0);
}

static void
dec_h264_set_rpl_regs(struct mwv207_video_decoder *decoder,
                      struct mwv207_h264_picture_desc *pic_param)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;

   uint32_t isValid[16] = {0};
   int i, j, k;
   uint32_t List0[34] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                         12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                         24, 25, 26, 27, 28, 29, 30, 31, 32, 33};
   uint32_t List1[34] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                         12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                         24, 25, 26, 27, 28, 29, 30, 31, 32, 33};
   uint32_t Listp[34] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11,
                         12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                         24, 25, 26, 27, 28, 29, 30, 31, 32, 33};

   /* init reference picture list0/list1 */
   h264_init_ref_pic_list(decoder, pic_param);

   /* a workaround to deal with HW limitations of h264 directB,
            the buffer index of one reference frame should be always same */

   for (i = 0; i < 16; i++) {
      if (dec->ref_frames[i].picture) {
         for (j = 0; j < 16; j++) {
            if ((pic_param->ref_frames[j].picture)
                && (pic_param->ref_frames[j].picture
                    == dec->ref_frames[i].picture)) {
               
               dec->ref_frames[i] = pic_param->ref_frames[j];

               isValid[j] = 1;
               /* list0/list1/listp shold be updated accordingly */
               for (k = 0; k < 16; k++) {
                  if (dec->list0[k] == j) {
                     List0[k] = i;
                     break;
                  }
               }

               for (k = 0; k < 16; k++) {
                  if (dec->list1[k] == j) {
                     List1[k] = i;
                     break;
                  }
               }

               for (k = 0; k < 16; k++) {
                  if (dec->list_p[k] == j) {
                     Listp[k] = i;
                     break;
                  }
               }
               break;
            }
         }

         if (j == 16) {
            dec->ref_frames[i].picture = NULL;
            dec->ref_frames[i].frame_num = 0;
            dec->ref_frames[i].is_long_term = false;
            dec->ref_frames[i].top_is_reference = false;
            dec->ref_frames[i].bottom_is_reference = false;

            dec->ref_frames[i].top_field_order_cnt = 0;
            dec->ref_frames[i].bottom_field_order_cnt = 0;
         }
      }
   }
   /* insert new reference pictures to
    * private_inst->ReferenceFrames[private_inst->view] */
   for (i = 0; i < 16; i++) {
      if (pic_param->ref_frames[i].picture) {
         if (isValid[i])
            continue;
         for (j = 0; j < 16; j++) {
            if (!dec->ref_frames[j].picture) {
               
               
               dec->ref_frames[j] = pic_param->ref_frames[i];

               /* list0/list1/listp should be updated accordingly */
               for (k = 0; k < 16; k++) {
                  if (dec->list0[k] == i) {
                     List0[k] = j;
                     break;
                  }
               }

               for (k = 0; k < 16; k++) {
                  if (dec->list1[k] == i) {
                     List1[k] = j;
                     break;
                  }
               }

               for (k = 0; k < 16; k++) {
                  if (dec->list_p[k] == i) {
                     Listp[k] = j;
                     break;
                  }
               }
               break;
            }
         }
      }
   }
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   
   for (i = 0; i < 16; i++) {
      reg_buf->set_reg(reg_buf, ref_pic_list0[i], List0[i]);
      reg_buf->set_reg(reg_buf, ref_pic_list1[i], List1[i]);
      reg_buf->set_reg(reg_buf, ref_pic_list_p[i], Listp[i]);
   }
}

static void
dec_h264_set_cur_out_buffer_info(struct mwv207_video_decoder *decoder,
                                 struct mwv207_h264_picture_desc *pic_param,
                                 struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;

   struct mwv207_video_buffer *buf;
   struct mwv207_resource *buf_res_y;
   struct mwv207_resource *buf_res_uv;

   uint32_t curr_pic_width_in_mbs =
      align(align(decoder->base.width, 16), 16) / 16;
   uint32_t curr_pic_height_in_mbs = align(decoder->base.height, 16) / 16;

   if (!pic_param->pps.sps.frame_mbs_only_flag
       && dec->dec_param.allow_dpb_field_ordering) {
      dec->dec_param.dpb_mode = DEC_DPB_INTERLACED_FIELD;
   } else {
      dec->dec_param.dpb_mode = DEC_DPB_FRAME;
   }

   buf = (struct mwv207_video_buffer *)target;
   buf_res_y = mwv207_resource(buf->resources[0]); 
   buf_res_uv = mwv207_resource(buf->resources[1]); 

   reg_buf->set_addr_reg_hi(reg_buf,
                            JMDEC_DEC_OUT_BASE_MSB,
                            buf->res->bo,
                            0,
                            MWV207_BO_ACC_FLAG_WRITE);
   reg_buf->set_addr_reg(reg_buf,
                         JMDEC_DEC_OUT_BASE_LSB,
                         buf->res->bo,
                         0,
                         MWV207_BO_ACC_FLAG_WRITE);

   if (dec->dec_param.high10p_mode) {
      uint32_t pixel_width = dec->dec_param.bit_depth;
      uint32_t dec_ystride;
      if (dec->dec_param.tiled_stride_enable) {
         dec_ystride =
            NEXT_MULTIPLE((curr_pic_width_in_mbs)*4 * 16 * pixel_width,
                          (ALIGN(dec->dec_param.w_align) * 8))
            / 8;
      } else {
         dec_ystride = (curr_pic_width_in_mbs)*4 * 16 * pixel_width / 8;
      }
      buf->mwv207_offset_info.chroma_base_offset =
         NEXT_MULTIPLE(dec_ystride * curr_pic_height_in_mbs * 4,
                       MAX(16, ALIGN(dec->dec_param.w_align)));

      reg_buf->set_addr_reg_hi(reg_buf,
                               JMDEC_DEC_OUT_CBASE_MSB,
                               buf->res->bo,
                               buf->mwv207_offset_info.chroma_base_offset,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf,
                            JMDEC_DEC_OUT_CBASE_LSB,
                            buf->res->bo,
                            buf->mwv207_offset_info.chroma_base_offset,
                            MWV207_BO_ACC_FLAG_WRITE);

      reg_buf->set_addr_reg_hi(reg_buf,
                               JMDEC_DIR_MV_BASE_MSB,
                               buf->res->bo,
                               buf->mwv207_offset_info.dir_mv_offset,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf,
                            JMDEC_DIR_MV_BASE_LSB,
                            buf->res->bo,
                            buf->mwv207_offset_info.dir_mv_offset,
                            MWV207_BO_ACC_FLAG_WRITE);

      if (dec->dec_param.use_video_compressor) {

         reg_buf->set_addr_reg_hi(reg_buf,
                                  JMDEC_DEC_OUT_TYBASE_MSB,
                                  buf_res_y->bo,
                                  0,
                                  MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg(reg_buf,
                               JMDEC_DEC_OUT_TYBASE_LSB,
                               buf_res_y->bo,
                               0,
                               MWV207_BO_ACC_FLAG_WRITE);

         reg_buf->set_addr_reg_hi(reg_buf,
                                  JMDEC_DEC_OUT_TCBASE_MSB,
                                  buf_res_uv->bo,
                                  0,
                                  MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg(reg_buf,
                               JMDEC_DEC_OUT_TCBASE_LSB,
                               buf_res_uv->bo,
                               0,
                               MWV207_BO_ACC_FLAG_WRITE);

         reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_EC_BYPASS, 0);
      } else {
         reg_buf->set_reg(reg_buf, JMDEC_DEC_OUT_EC_BYPASS, 1);
      }
   }

   reg_buf->set_reg(reg_buf, JMDEC_DPB_ILACE_MODE, dec->dec_param.dpb_mode);
   reg_buf->set_reg(
      reg_buf, JMDEC_TILED_MODE_MSB, (TILED_REF_8x4 >> 1) & 0x1);
   reg_buf->set_reg(reg_buf, JMDEC_TILED_MODE_LSB, TILED_REF_8x4 & 0x1);
}

static void
dec_h264_set_ref_buffer_info(struct mwv207_video_decoder *decoder,
                             struct mwv207_h264_picture_desc *pic_param,
                             struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;

   struct mwv207_video_buffer *buf;
   struct mwv207_resource *buf_res_y;
   struct mwv207_resource *buf_res_vu;

   uint32_t long_termflags, long_term_tmp;
   uint32_t valid_flags, valid_tmp;

   long_termflags = long_term_tmp = 0;
   valid_flags = valid_tmp = 0;

   int i = 0;
   for (i = 0; i < 16; i++) {
      buf = (struct mwv207_video_buffer *)dec->ref_frames[i].picture;
      if (buf) {
         reg_buf->set_reg(
            reg_buf, ref_pic_num[i], dec->ref_frames[i].frame_num);

         long_term_tmp = pic_param->is_long_term[i];
         valid_tmp = 1;
      } else {
         buf = (struct mwv207_video_buffer *)target;
         reg_buf->set_reg(reg_buf, ref_pic_num[i], pic_param->frame_num);

         long_term_tmp = 0;
         valid_tmp = 0;
      }
      long_termflags = long_termflags << 1 | long_term_tmp;
      valid_flags = valid_flags << 1 | valid_tmp;

      if (valid_tmp) {
         buf_res_y = mwv207_resource(buf->resources[0]); 

         reg_buf->set_addr_reg_hi(reg_buf,
                                  ref_base_msb[i],
                                  buf->res->bo,
                                  0,
                                  MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg(
            reg_buf, ref_base[i], buf->res->bo, 0, MWV207_BO_ACC_FLAG_WRITE);

         reg_buf->set_addr_reg_hi(reg_buf,
                                  ref_dbase_msb[i],
                                  buf->res->bo,
                                  buf->mwv207_offset_info.dir_mv_offset,
                                  MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg(reg_buf,
                               ref_dbase[i],
                               buf->res->bo,
                               buf->mwv207_offset_info.dir_mv_offset,
                               MWV207_BO_ACC_FLAG_WRITE);

         buf_res_vu = mwv207_resource(buf->resources[1]); 

         reg_buf->set_addr_reg_hi(reg_buf,
                                  ref_cbase_msb[i],
                                  buf->res->bo,
                                  buf->mwv207_offset_info.chroma_base_offset,
                                  MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg(reg_buf,
                               ref_cbase[i],
                               buf->res->bo,
                               buf->mwv207_offset_info.chroma_base_offset,
                               MWV207_BO_ACC_FLAG_WRITE);
      }

      
      
      

      if (dec->dec_param.use_video_compressor) {

         reg_buf->set_addr_reg_hi(reg_buf,
                                  ref_tybase_msb[i],
                                  buf_res_y->bo,
                                  0,
                                  MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg(reg_buf,
                               ref_tybase[i],
                               buf_res_y->bo,
                               0,
                               MWV207_BO_ACC_FLAG_WRITE);

         reg_buf->set_addr_reg_hi(reg_buf,
                                  ref_tcbase_msb[i],
                                  buf_res_vu->bo,
                                  0,
                                  MWV207_BO_ACC_FLAG_WRITE);
         reg_buf->set_addr_reg(reg_buf,
                               ref_tcbase[i],
                               buf_res_vu->bo,
                               0,
                               MWV207_BO_ACC_FLAG_WRITE);
      } else {
         
         
         
         

         
         
         
         
      }
   }

   valid_flags <<= 16;
   long_termflags <<= 16;
   reg_buf->set_reg(reg_buf, JMDEC_REFER_LTERM_E, long_termflags);
   reg_buf->set_reg(reg_buf, JMDEC_REFER_VALID_E, valid_flags);

   reg_buf->set_reg(reg_buf, JMDEC_PICORD_COUNT_E, 1);
   reg_buf->set_reg(reg_buf, JMDEC_BITPLANE1_E, 0);
}

static void
dec_avc_set_matrix_flag(struct mwv207_video_decoder *decoder,
                        struct mwv207_h264_picture_desc *pic_param)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;
   int i, j;

   dec->dec_param.scaling_matrix_present_flag = 0;

   for (i = 0; i < 6; i++) {
      for (j = 0; j < 16; j++) {
         if (pic_param->pps.ScalingList4x4[i][j] != 16) {
            dec->dec_param.scaling_matrix_present_flag = 1;
            break;
         }
      }
      if (dec->dec_param.scaling_matrix_present_flag)
         break;
   }

   for (i = 0; i < 2; i++) {
      for (j = 0; j < 64; j++) {
         if (pic_param->pps.ScalingList8x8[i][j] != 16) {
            dec->dec_param.scaling_matrix_present_flag = 1;
            break;
         }
      }
      if (dec->dec_param.scaling_matrix_present_flag)
         break;
   }
   if (dec->dec_param.high10p_mode) {
      reg_buf->set_reg(
         reg_buf, JMDEC_BIT_DEPTH_Y_MINUS8, dec->dec_param.bit_depth - 8);
      reg_buf->set_reg(
         reg_buf, JMDEC_BIT_DEPTH_C_MINUS8, dec->dec_param.bit_depth - 8);
   }
   reg_buf->set_reg(reg_buf,
                    JMDEC_TYPE1_QUANT_E,
                    dec->dec_param.scaling_matrix_present_flag);
}

static void
dec_h264_set_cabac_data(struct mwv207_video_decoder *decoder,
                        struct mwv207_h264_picture_desc *pic_param)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;

   uint32_t cabac_len;
   int32_t curr_poc = 0;
   int32_t *poc_base = NULL;
   if (!dec->dec_param.high10p_mode) {
      cabac_len = ASIC_CABAC_INIT_BUFFER_SIZE + ASIC_SCALING_LIST_SIZE
         + ASIC_POC_BUFFER_SIZE;
   } else {
      cabac_len = ASIC_CABAC_INIT_BUFFER_SIZE + ASIC_SCALING_LIST_SIZE
         + ASIC_POC_BUFFER_SIZE_H10;
   }

   if (!dec->cabac_buffer[dec->current_cabac_buf_index]) {
      dec->cabac_buffer[dec->current_cabac_buf_index] =
         mwv207_video_buffer_create_2(
            decoder->base.context, cabac_len, PIPE_USAGE_STAGING);
      dec->cabac_buf = decoder->ws->bo_map(
         decoder->ws,
         dec->cabac_buffer[dec->current_cabac_buf_index]->res->bo);
   } else {
      dec->cabac_buf =
         dec->cabac_buffer[dec->current_cabac_buf_index]->res->bo->logical;
   }
   memset(dec->cabac_buf, 0, cabac_len);
   memcpy(dec->cabac_buf, cabac_init_values, ASIC_CABAC_INIT_BUFFER_SIZE);

   poc_base =
      (int32_t *)(dec->cabac_buf + ASIC_CABAC_INIT_BUFFER_SIZE); 

   int i, j;
   for (i = 0; i < 16; i++) { 
      *poc_base++ = dec->ref_frames[i].top_field_order_cnt; 
      *poc_base++ = dec->ref_frames[i].bottom_field_order_cnt; 
   }

   if (pic_param->field_pic_flag) {
      curr_poc = ((pic_param->bottom_field_flag))
         ? pic_param->field_order_cnt[1]
         : pic_param->field_order_cnt[0];
   } else {
      curr_poc =
         MIN(pic_param->field_order_cnt[1], pic_param->field_order_cnt[0]);
   }

   if (pic_param->field_pic_flag
       || !pic_param->pps.sps.mb_adaptive_frame_field_flag) {
      *poc_base++ = curr_poc;
      *poc_base = 0;
   } else { 
      *poc_base++ = pic_param->field_order_cnt[0];
      *poc_base++ = pic_param->field_order_cnt[1];
   }

   if (dec->dec_param.scaling_matrix_present_flag) {
      uint32_t *p = NULL;
      if (!dec->dec_param.high10p_mode) {
         p = (uint32_t *)((uint8_t *)dec->cabac_buf
                          + ASIC_CABAC_INIT_BUFFER_SIZE
                          + ASIC_POC_BUFFER_SIZE);
      } else {
         p = (uint32_t *)((uint8_t *)dec->cabac_buf
                          + ASIC_CABAC_INIT_BUFFER_SIZE
                          + ASIC_POC_BUFFER_SIZE_H10);
      }

      uint32_t tmp;
      for (i = 0; i < 6; i++) {
         for (j = 0; j < 4; j++) {
            tmp = (pic_param->pps.ScalingList4x4[i][4 * j + 0] << 24)
               | (pic_param->pps.ScalingList4x4[i][4 * j + 1] << 16)
               | (pic_param->pps.ScalingList4x4[i][4 * j + 2] << 8)
               | (pic_param->pps.ScalingList4x4[i][4 * j + 3]);
            *p++ = tmp;
         }
      }
      for (i = 0; i < 2; i++) {
         for (j = 0; j < 16; j++) {
            tmp = (pic_param->pps.ScalingList8x8[i][4 * j + 0] << 24)
               | (pic_param->pps.ScalingList8x8[i][4 * j + 1] << 16)
               | (pic_param->pps.ScalingList8x8[i][4 * j + 2] << 8)
               | (pic_param->pps.ScalingList8x8[i][4 * j + 3]);
            *p++ = tmp;
         }
      }
   }

   reg_buf->set_addr_reg_hi(
      reg_buf,
      JMDEC_QTABLE_BASE_MSB,
      dec->cabac_buffer[dec->current_cabac_buf_index]->res->bo,
      0,
      MWV207_BO_ACC_FLAG_WRITE);
   reg_buf->set_addr_reg(
      reg_buf,
      JMDEC_QTABLE_BASE_LSB,
      dec->cabac_buffer[dec->current_cabac_buf_index]->res->bo,
      0,
      MWV207_BO_ACC_FLAG_WRITE);

   dec->current_cabac_buf_index =
      (dec->current_cabac_buf_index + 1) % MAX_CABAC_BUF_COUNT;
}

static void
dec_h264_set_stream_data(struct mwv207_video_decoder *decoder,
                         struct mwv207_h264_picture_desc *pic_param)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;

   struct mwv207_video_buffer *stream_buf =
      decoder->bs_buffers[decoder->cur_buffer];
   struct mwv207_resource *stream_buf_res = stream_buf->res;

   reg_buf->set_reg(reg_buf, JMDEC_START_CODE_E, 1);
   reg_buf->set_reg(reg_buf, JMDEC_STRM_START_OFFSET, 0);
   reg_buf->set_reg(reg_buf, JMDEC_STREAM_LEN, decoder->bs_size);
   reg_buf->set_reg(reg_buf, JMDEC_STRM_BUFFER_LEN, stream_buf_res->size);

   if (dec->dec_param.high10p_mode) {
      reg_buf->set_addr_reg_hi(reg_buf,
                               JMDEC_STREAM_BASE_MSB,
                               stream_buf_res->bo,
                               0,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf,
                            JMDEC_STREAM_BASE_LSB,
                            stream_buf_res->bo,
                            0,
                            MWV207_BO_ACC_FLAG_WRITE);
   } else {
      reg_buf->set_addr_reg_hi(reg_buf,
                               JMDEC_RLC_VLC_BASE_MSB,
                               stream_buf_res->bo,
                               0,
                               MWV207_BO_ACC_FLAG_WRITE);
      reg_buf->set_addr_reg(reg_buf,
                            JMDEC_RLC_VLC_BASE_LSB,
                            stream_buf_res->bo,
                            0,
                            MWV207_BO_ACC_FLAG_WRITE);
   }
}

void dump_referece_frames(struct mwv207_video_decoder *dec,
                          struct mwv207_h264_picture_desc *pic_param);
void
dump_referece_frames(struct mwv207_video_decoder *dec,
                     struct mwv207_h264_picture_desc *pic_param)
{
   for (int i = 0; i < 16; i++) {
      void *picture = pic_param->ref_frames[i].picture;
      if (picture) {
         char file_path[128] = {0};
         sprintf(file_path, "./dump/dump_referece_frames_%02d.txt", i);
         FILE *f2 = fopen(file_path, "w");

         struct mwv207_video_buffer *buf =
            (struct mwv207_video_buffer *)picture;
         struct mwv207_resource *buf_res_y;
         struct mwv207_resource *buf_res_uv;
         buf_res_y = mwv207_resource(buf->resources[0]);
         buf_res_uv = mwv207_resource(buf->resources[1]);

         if (buf_res_y->bo->logical == NULL) {
            dec->ws->bo_map(dec->ws, buf_res_y->bo);
         }
         fwrite(buf_res_y->bo->logical, 1, 1920 * 1080, f2);
         
         if (buf_res_uv->bo->logical == NULL) {
            dec->ws->bo_map(dec->ws, buf_res_uv->bo);
         }
         fwrite(buf_res_uv->bo->logical, 1, 1920 * 1080 / 2, f2);
         fclose(f2);
      }
   }
}

void
mwv207_dec_h264_set_regs(struct mwv207_video_decoder *decoder,
                         struct pipe_video_buffer *target)
{
   struct mwv207_video_dec *dec = decoder->decoder;
   struct mwv207_video_reg_buf *reg_buf = dec->reg_buf;
   struct mwv207_h264_picture_desc *pic_param = &decoder->picture_desc.h264;

   dec->dec_param.pp_enabled = 1;
   mwv207_video_dec_get_hw_features_by_id(0x00001F76,
                                          &dec->hw_feature);
   dec->dec_param.bit_depth =
      (pic_param->pps.sps.bit_depth_luma_minus8 == 0
       && pic_param->pps.sps.bit_depth_chroma_minus8 == 0)
      ? 8
      : 10;

   
   
   dec->dec_param.w_align = 6;
   dec->dec_param.use_video_compressor = 0;
   dec->dec_param.allow_dpb_field_ordering = 1;
   pic_param->cur_pic = (struct mwv207_video_buffer *)target;

   dec_h264_set_common_regs(decoder, pic_param);
   dec_h264_set_picparam_regs(decoder, pic_param);

   /*since we need read nal unit and idr to update my ref frames,
   so call dec_h264_set_sh_regs before dec_h264_set_rpl_regs*/
   dec_h264_set_sh_regs(decoder, pic_param, target);
   if ((pic_param->ref_frames[0].picture == NULL)
       && (pic_param->num_my_ref_frames > 0)) {
      memcpy(pic_param->ref_frames,
             pic_param->my_ref_frames,
             16 * sizeof(struct mwv207_picture_h264));
   }

   pic_param->field_order_cnt[0] = pic_param->my_field_order_cnt[0];
   pic_param->field_order_cnt[1] = pic_param->my_field_order_cnt[1];

   dec_h264_set_rpl_regs(decoder, pic_param);

   dec_avc_calc_sur_offset(decoder, pic_param, target);

   dec_ensure_surface_res(decoder, pic_param, target);

   dec_h264_set_cur_out_buffer_info(decoder, pic_param, target);

   dec_h264_set_ref_buffer_info(decoder, pic_param, target);

   dec_h264_avc_set_pp_regs(decoder, pic_param, target);

   dec_avc_set_matrix_flag(decoder, pic_param);

   dec_h264_set_cabac_data(decoder, pic_param);

   dec_h264_set_stream_data(decoder, pic_param);

   reg_buf->set_reg(reg_buf, JMDEC_DEC_E, 1);
   reg_buf->dump_regs(reg_buf);

   if (pic_param->is_reference) {
      mwv207_h264_execute_ref_pic_marking(dec, pic_param, target);
      fill_my_ref_frames(dec, pic_param);
   }
}

