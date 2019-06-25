/*
 * RTP definitions
 * Copyright (c) 2002 Fabrice Bellard
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */
#ifndef AVFORMAT_RTP_H
#define AVFORMAT_RTP_H

#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/mathematics.h"

#define RTP_PT_PRIVATE 96
#define RTP_VERSION 2
#define RTP_MAX_SDES 256   /**< maximum text length for SDES */

/* RTCP packets use 0.5% of the bandwidth */
#define RTCP_TX_RATIO_NUM 5
#define RTCP_TX_RATIO_DEN 1000

/* An arbitrary id value for RTP Xiph streams - only relevant to indicate
 * that the configuration has changed within a stream (by changing the
 * ident value sent).
 */
#define RTP_XIPH_IDENT 0xfecdba

/* RTCP packet types */
enum RTCPType {
    RTCP_FIR    = 192,
    RTCP_NACK, // 193
    RTCP_SMPTETC,// 194
    RTCP_IJ,   // 195
    RTCP_SR     = 200,
    RTCP_RR,   // 201
    RTCP_SDES, // 202
    RTCP_BYE,  // 203
    RTCP_APP,  // 204
    RTCP_RTPFB,// 205
    RTCP_PSFB, // 206
    RTCP_XR,   // 207
    RTCP_AVB,  // 208
    RTCP_RSI,  // 209
    RTCP_TOKEN,// 210
};

#define RTP_PT_IS_RTCP(x) (((x) >= RTCP_FIR && (x) <= RTCP_IJ) || \
                           ((x) >= RTCP_SR  && (x) <= RTCP_TOKEN))

#define NTP_TO_RTP_FORMAT(x) av_rescale((x), INT64_C(1) << 32, 1000000)

#endif /* AVFORMAT_RTP_H */
