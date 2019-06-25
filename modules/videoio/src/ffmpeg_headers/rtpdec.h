/*
 * RTP demuxer definitions
 * Copyright (c) 2002 Fabrice Bellard
 * Copyright (c) 2006 Ryan Martell <rdm4@martellventures.com>
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

#ifndef AVFORMAT_RTPDEC_H
#define AVFORMAT_RTPDEC_H

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "rtp.h"
#include "srtp.h"

typedef struct PayloadContext PayloadContext;
typedef struct RTPDynamicProtocolHandler RTPDynamicProtocolHandler;

#define RTP_MIN_PACKET_LENGTH 12
#define RTP_MAX_PACKET_LENGTH 8192

#define RTP_REORDER_QUEUE_DEFAULT_SIZE 10

#define RTP_NOTS_VALUE ((uint32_t)-1)

// these statistics are used for rtcp receiver reports...
typedef struct RTPStatistics {
    uint16_t max_seq;           ///< highest sequence number seen
    uint32_t cycles;            ///< shifted count of sequence number cycles
    uint32_t base_seq;          ///< base sequence number
    uint32_t bad_seq;           ///< last bad sequence number + 1
    int probation;              ///< sequence packets till source is valid
    uint32_t received;          ///< packets received
    uint32_t expected_prior;    ///< packets expected in last interval
    uint32_t received_prior;    ///< packets received in last interval
    uint32_t transit;           ///< relative transit time for previous packet
    uint32_t jitter;            ///< estimated jitter.
} RTPStatistics;

#define RTP_FLAG_KEY    0x1 ///< RTP packet contains a keyframe
#define RTP_FLAG_MARKER 0x2 ///< RTP marker bit was set for this packet
/**
 * Packet parsing for "private" payloads in the RTP specs.
 *
 * @param ctx RTSP demuxer context
 * @param s stream context
 * @param st stream that this packet belongs to
 * @param pkt packet in which to write the parsed data
 * @param timestamp pointer to the RTP timestamp of the input data, can be
 *                  updated by the function if returning older, buffered data
 * @param buf pointer to raw RTP packet data
 * @param len length of buf
 * @param seq RTP sequence number of the packet
 * @param flags flags from the RTP packet header (RTP_FLAG_*)
 */
typedef int (*DynamicPayloadPacketHandlerProc)(AVFormatContext *ctx,
                                               PayloadContext *s,
                                               AVStream *st, AVPacket *pkt,
                                               uint32_t *timestamp,
                                               const uint8_t * buf,
                                               int len, uint16_t seq, int flags);

struct RTPDynamicProtocolHandler {
    const char *enc_name;
    enum AVMediaType codec_type;
    enum AVCodecID codec_id;
    enum AVStreamParseType need_parsing;
    int static_payload_id; /* 0 means no payload id is set. 0 is a valid
                            * payload ID (PCMU), too, but that format doesn't
                            * require any custom depacketization code. */
    int priv_data_size;

    /** Initialize dynamic protocol handler, called after the full rtpmap line is parsed, may be null */
    int (*init)(AVFormatContext *s, int st_index, PayloadContext *priv_data);
    /** Parse the a= line from the sdp field */
    int (*parse_sdp_a_line)(AVFormatContext *s, int st_index,
                            PayloadContext *priv_data, const char *line);
    /** Free any data needed by the rtp parsing for this dynamic data.
      * Don't free the protocol_data pointer itself, that is freed by the
      * caller. This is called even if the init method failed. */
    void (*close)(PayloadContext *protocol_data);
    /** Parse handler for this dynamic packet */
    DynamicPayloadPacketHandlerProc parse_packet;
    int (*need_keyframe)(PayloadContext *context);

    struct RTPDynamicProtocolHandler *next;
};

typedef struct RTPPacket {
    uint16_t seq;
    uint8_t *buf;
    int len;
    int64_t recvtime;
    struct RTPPacket *next;
} RTPPacket;

struct RTPDemuxContext {
    AVFormatContext *ic;
    AVStream *st;
    int payload_type;
    uint32_t ssrc;
    uint16_t seq;
    uint32_t timestamp;
    uint32_t base_timestamp;
    uint32_t cur_timestamp;
    int64_t  unwrapped_timestamp;
    int64_t  range_start_offset;
    int max_payload_size;
    /* used to send back RTCP RR */
    char hostname[256];

    int srtp_enabled;
    struct SRTPContext srtp;

    /** Statistics for this stream (used by RTCP receiver reports) */
    RTPStatistics statistics;

    /** Fields for packet reordering @{ */
    int prev_ret;     ///< The return value of the actual parsing of the previous packet
    RTPPacket* queue; ///< A sorted queue of buffered packets not yet returned
    int queue_len;    ///< The number of packets in queue
    int queue_size;   ///< The size of queue, or 0 if reordering is disabled
    /*@}*/

    /* rtcp sender statistics receive */
    uint64_t last_rtcp_ntp_time;
    int64_t last_rtcp_reception_time;
    uint64_t first_rtcp_ntp_time;
    uint32_t last_rtcp_timestamp;
    int64_t rtcp_ts_offset;

    /* rtcp sender statistics */
    unsigned int packet_count;
    unsigned int octet_count;
    unsigned int last_octet_count;
    int64_t last_feedback_time;

    /* dynamic payload stuff */
    const RTPDynamicProtocolHandler *handler;
    PayloadContext *dynamic_protocol_context;
};

#endif /* AVFORMAT_RTPDEC_H */
