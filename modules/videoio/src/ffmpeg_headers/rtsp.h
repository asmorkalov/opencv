/*
 * RTSP definitions
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
#ifndef AVFORMAT_RTSP_H
#define AVFORMAT_RTSP_H

#include <stdint.h>
#include "libavformat/avformat.h"
#include "rtspcodes.h"
#include "rtpdec.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include "httpauth.h"

/**
 * Network layer over which RTP/etc packet data will be transported.
 */
enum RTSPLowerTransport {
    RTSP_LOWER_TRANSPORT_UDP = 0,           /**< UDP/unicast */
    RTSP_LOWER_TRANSPORT_TCP = 1,           /**< TCP; interleaved in RTSP */
    RTSP_LOWER_TRANSPORT_UDP_MULTICAST = 2, /**< UDP/multicast */
    RTSP_LOWER_TRANSPORT_NB,
    RTSP_LOWER_TRANSPORT_HTTP = 8,          /**< HTTP tunneled - not a proper
                                                 transport mode as such,
                                                 only for use via AVOptions */
    RTSP_LOWER_TRANSPORT_CUSTOM = 16,       /**< Custom IO - not a public
                                                 option for lower_transport_mask,
                                                 but set in the SDP demuxer based
                                                 on a flag. */
};

/**
 * Packet profile of the data that we will be receiving. Real servers
 * commonly send RDT (although they can sometimes send RTP as well),
 * whereas most others will send RTP.
 */
enum RTSPTransport {
    RTSP_TRANSPORT_RTP, /**< Standards-compliant RTP */
    RTSP_TRANSPORT_RDT, /**< Realmedia Data Transport */
    RTSP_TRANSPORT_RAW, /**< Raw data (over UDP) */
    RTSP_TRANSPORT_NB
};

/**
 * Transport mode for the RTSP data. This may be plain, or
 * tunneled, which is done over HTTP.
 */
enum RTSPControlTransport {
    RTSP_MODE_PLAIN,   /**< Normal RTSP */
    RTSP_MODE_TUNNEL   /**< RTSP over HTTP (tunneling) */
};

#define RTSP_DEFAULT_PORT   554
#define RTSPS_DEFAULT_PORT  322
#define RTSP_MAX_TRANSPORTS 8
#define RTSP_TCP_MAX_PACKET_SIZE 1472
#define RTSP_DEFAULT_NB_AUDIO_CHANNELS 1
#define RTSP_DEFAULT_AUDIO_SAMPLERATE 44100
#define RTSP_RTP_PORT_MIN 5000
#define RTSP_RTP_PORT_MAX 65000

/**
 * This describes a single item in the "Transport:" line of one stream as
 * negotiated by the SETUP RTSP command. Multiple transports are comma-
 * separated ("Transport: x-read-rdt/tcp;interleaved=0-1,rtp/avp/udp;
 * client_port=1000-1001;server_port=1800-1801") and described in separate
 * RTSPTransportFields.
 */
typedef struct RTSPTransportField {
    /** interleave ids, if TCP transport; each TCP/RTSP data packet starts
     * with a '$', stream length and stream ID. If the stream ID is within
     * the range of this interleaved_min-max, then the packet belongs to
     * this stream. */
    int interleaved_min, interleaved_max;

    /** UDP multicast port range; the ports to which we should connect to
     * receive multicast UDP data. */
    int port_min, port_max;

    /** UDP client ports; these should be the local ports of the UDP RTP
     * (and RTCP) sockets over which we receive RTP/RTCP data. */
    int client_port_min, client_port_max;

    /** UDP unicast server port range; the ports to which we should connect
     * to receive unicast UDP RTP/RTCP data. */
    int server_port_min, server_port_max;

    /** time-to-live value (required for multicast); the amount of HOPs that
     * packets will be allowed to make before being discarded. */
    int ttl;

    /** transport set to record data */
    int mode_record;

    struct sockaddr_storage destination; /**< destination IP address */
    char source[INET6_ADDRSTRLEN + 1]; /**< source IP address */

    /** data/packet transport protocol; e.g. RTP or RDT */
    enum RTSPTransport transport;

    /** network layer transport protocol; e.g. TCP or UDP uni-/multicast */
    enum RTSPLowerTransport lower_transport;
} RTSPTransportField;

/**
 * This describes the server response to each RTSP command.
 */
typedef struct RTSPMessageHeader {
    /** length of the data following this header */
    int content_length;

    enum RTSPStatusCode status_code; /**< response code from server */

    /** number of items in the 'transports' variable below */
    int nb_transports;

    /** Time range of the streams that the server will stream. In
     * AV_TIME_BASE unit, AV_NOPTS_VALUE if not used */
    int64_t range_start, range_end;

    /** describes the complete "Transport:" line of the server in response
     * to a SETUP RTSP command by the client */
    RTSPTransportField transports[RTSP_MAX_TRANSPORTS];

    int seq;                         /**< sequence number */

    /** the "Session:" field. This value is initially set by the server and
     * should be re-transmitted by the client in every RTSP command. */
    char session_id[512];

    /** the "Location:" field. This value is used to handle redirection.
     */
    char location[4096];

    /** the "RealChallenge1:" field from the server */
    char real_challenge[64];

    /** the "Server: field, which can be used to identify some special-case
     * servers that are not 100% standards-compliant. We use this to identify
     * Windows Media Server, which has a value "WMServer/v.e.r.sion", where
     * version is a sequence of digits (e.g. 9.0.0.3372). Helix/Real servers
     * use something like "Helix [..] Server Version v.e.r.sion (platform)
     * (RealServer compatible)" or "RealServer Version v.e.r.sion (platform)",
     * where platform is the output of $uname -msr | sed 's/ /-/g'. */
    char server[64];

    /** The "timeout" comes as part of the server response to the "SETUP"
     * command, in the "Session: <xyz>[;timeout=<value>]" line. It is the
     * time, in seconds, that the server will go without traffic over the
     * RTSP/TCP connection before it closes the connection. To prevent
     * this, sent dummy requests (e.g. OPTIONS) with intervals smaller
     * than this value. */
    int timeout;

    /** The "Notice" or "X-Notice" field value. See
     * http://tools.ietf.org/html/draft-stiemerling-rtsp-announce-00
     * for a complete list of supported values. */
    int notice;

    /** The "reason" is meant to specify better the meaning of the error code
     * returned
     */
    char reason[256];

    /**
     * Content type header
     */
    char content_type[64];
} RTSPMessageHeader;

/**
 * Client state, i.e. whether we are currently receiving data (PLAYING) or
 * setup-but-not-receiving (PAUSED). State can be changed in applications
 * by calling av_read_play/pause().
 */
enum RTSPClientState {
    RTSP_STATE_IDLE,    /**< not initialized */
    RTSP_STATE_STREAMING, /**< initialized and sending/receiving data */
    RTSP_STATE_PAUSED,  /**< initialized, but not receiving data */
    RTSP_STATE_SEEKING, /**< initialized, requesting a seek */
};

/**
 * Identify particular servers that require special handling, such as
 * standards-incompliant "Transport:" lines in the SETUP request.
 */
enum RTSPServerType {
    RTSP_SERVER_RTP,  /**< Standards-compliant RTP-server */
    RTSP_SERVER_REAL, /**< Realmedia-style server */
    RTSP_SERVER_WMS,  /**< Windows Media server */
    RTSP_SERVER_NB
};

/**
 * Private data for the RTSP demuxer.
 *
 * @todo Use AVIOContext instead of URLContext
 */
typedef struct RTSPState {
    const AVClass *clazz;             /**< Class for private options. */
    URLContext *rtsp_hd; /* RTSP TCP connection handle */

    /** number of items in the 'rtsp_streams' variable */
    int nb_rtsp_streams;

    struct RTSPStream **rtsp_streams; /**< streams in this session */

    /** indicator of whether we are currently receiving data from the
     * server. Basically this isn't more than a simple cache of the
     * last PLAY/PAUSE command sent to the server, to make sure we don't
     * send 2x the same unexpectedly or commands in the wrong state. */
    enum RTSPClientState state;

    /** the seek value requested when calling av_seek_frame(). This value
     * is subsequently used as part of the "Range" parameter when emitting
     * the RTSP PLAY command. If we are currently playing, this command is
     * called instantly. If we are currently paused, this command is called
     * whenever we resume playback. Either way, the value is only used once,
     * see rtsp_read_play() and rtsp_read_seek(). */
    int64_t seek_timestamp;

    int seq;                          /**< RTSP command sequence number */

    /** copy of RTSPMessageHeader->session_id, i.e. the server-provided session
     * identifier that the client should re-transmit in each RTSP command */
    char session_id[512];

    /** copy of RTSPMessageHeader->timeout, i.e. the time (in seconds) that
     * the server will go without traffic on the RTSP/TCP line before it
     * closes the connection. */
    int timeout;

    /** timestamp of the last RTSP command that we sent to the RTSP server.
     * This is used to calculate when to send dummy commands to keep the
     * connection alive, in conjunction with timeout. */
    int64_t last_cmd_time;

    /** the negotiated data/packet transport protocol; e.g. RTP or RDT */
    enum RTSPTransport transport;

    /** the negotiated network layer transport protocol; e.g. TCP or UDP
     * uni-/multicast */
    enum RTSPLowerTransport lower_transport;

    /** brand of server that we're talking to; e.g. WMS, REAL or other.
     * Detected based on the value of RTSPMessageHeader->server or the presence
     * of RTSPMessageHeader->real_challenge */
    enum RTSPServerType server_type;

    /** the "RealChallenge1:" field from the server */
    char real_challenge[64];

    /** plaintext authorization line (username:password) */
    char auth[128];

    /** authentication state */
    HTTPAuthState auth_state;

    /** The last reply of the server to a RTSP command */
    char last_reply[2048]; /* XXX: allocate ? */

    /** RTSPStream->transport_priv of the last stream that we read a
     * packet from */
    void *cur_transport_priv;

    /** The following are used for Real stream selection */
    //@{
    /** whether we need to send a "SET_PARAMETER Subscribe:" command */
    int need_subscription;

    /** stream setup during the last frame read. This is used to detect if
     * we need to subscribe or unsubscribe to any new streams. */
    enum AVDiscard *real_setup_cache;

    /** current stream setup. This is a temporary buffer used to compare
     * current setup to previous frame setup. */
    enum AVDiscard *real_setup;

    /** the last value of the "SET_PARAMETER Subscribe:" RTSP command.
     * this is used to send the same "Unsubscribe:" if stream setup changed,
     * before sending a new "Subscribe:" command. */
    char last_subscription[1024];
    //@}

    /** The following are used for RTP/ASF streams */
    //@{
    /** ASF demuxer context for the embedded ASF stream from WMS servers */
    AVFormatContext *asf_ctx;

    /** cache for position of the asf demuxer, since we load a new
     * data packet in the bytecontext for each incoming RTSP packet. */
    uint64_t asf_pb_pos;
    //@}

    /** some MS RTSP streams contain a URL in the SDP that we need to use
     * for all subsequent RTSP requests, rather than the input URI; in
     * other cases, this is a copy of AVFormatContext->filename. */
    char control_uri[1024];

    /** The following are used for parsing raw mpegts in udp */
    //@{
    struct MpegTSContext *ts;
    int recvbuf_pos;
    int recvbuf_len;
    //@}

    /** Additional output handle, used when input and output are done
     * separately, eg for HTTP tunneling. */
    URLContext *rtsp_hd_out;

    /** RTSP transport mode, such as plain or tunneled. */
    enum RTSPControlTransport control_transport;

    /* Number of RTCP BYE packets the RTSP session has received.
     * An EOF is propagated back if nb_byes == nb_streams.
     * This is reset after a seek. */
    int nb_byes;

    /** Reusable buffer for receiving packets */
    uint8_t* recvbuf;

    /**
     * A mask with all requested transport methods
     */
    int lower_transport_mask;

    /**
     * The number of returned packets
     */
    uint64_t packets;

    /**
     * Polling array for udp
     */
    struct pollfd *p;

    /**
     * Whether the server supports the GET_PARAMETER method.
     */
    int get_parameter_supported;

    /**
     * Do not begin to play the stream immediately.
     */
    int initial_pause;

    /**
     * Option flags for the chained RTP muxer.
     */
    int rtp_muxer_flags;

    /** Whether the server accepts the x-Dynamic-Rate header */
    int accept_dynamic_rate;

    /**
     * Various option flags for the RTSP muxer/demuxer.
     */
    int rtsp_flags;

    /**
     * Mask of all requested media types
     */
    int media_type_mask;

    /**
     * Minimum and maximum local UDP ports.
     */
    int rtp_port_min, rtp_port_max;

    /**
     * Timeout to wait for incoming connections.
     */
    int initial_timeout;

    /**
     * timeout of socket i/o operations.
     */
    int stimeout;

    /**
     * Size of RTP packet reordering queue.
     */
    int reordering_queue_size;

    /**
     * User-Agent string
     */
    char *user_agent;

    char default_lang[4];
    int buffer_size;
} RTSPState;

#define RTSP_FLAG_FILTER_SRC  0x1    /**< Filter incoming UDP packets -
                                          receive packets only from the right
                                          source address and port. */
#define RTSP_FLAG_LISTEN      0x2    /**< Wait for incoming connections. */
#define RTSP_FLAG_CUSTOM_IO   0x4    /**< Do all IO via the AVIOContext. */
#define RTSP_FLAG_RTCP_TO_SOURCE 0x8 /**< Send RTCP packets to the source
                                          address of received packets. */
#define RTSP_FLAG_PREFER_TCP  0x10   /**< Try RTP via TCP first if possible. */

typedef struct RTSPSource {
    char addr[128]; /**< Source-specific multicast include source IP address (from SDP content) */
} RTSPSource;

/**
 * Describe a single stream, as identified by a single m= line block in the
 * SDP content. In the case of RDT, one RTSPStream can represent multiple
 * AVStreams. In this case, each AVStream in this set has similar content
 * (but different codec/bitrate).
 */
typedef struct RTSPStream {
    URLContext *rtp_handle;   /**< RTP stream handle (if UDP) */
    void *transport_priv; /**< RTP/RDT parse context if input, RTP AVFormatContext if output */

    /** corresponding stream index, if any. -1 if none (MPEG2TS case) */
    int stream_index;

    /** interleave IDs; copies of RTSPTransportField->interleaved_min/max
     * for the selected transport. Only used for TCP. */
    int interleaved_min, interleaved_max;

    char control_url[1024];   /**< url for this stream (from SDP) */

    /** The following are used only in SDP, not RTSP */
    //@{
    int sdp_port;             /**< port (from SDP content) */
    struct sockaddr_storage sdp_ip; /**< IP address (from SDP content) */
    int nb_include_source_addrs; /**< Number of source-specific multicast include source IP addresses (from SDP content) */
    struct RTSPSource **include_source_addrs; /**< Source-specific multicast include source IP addresses (from SDP content) */
    int nb_exclude_source_addrs; /**< Number of source-specific multicast exclude source IP addresses (from SDP content) */
    struct RTSPSource **exclude_source_addrs; /**< Source-specific multicast exclude source IP addresses (from SDP content) */
    int sdp_ttl;              /**< IP Time-To-Live (from SDP content) */
    int sdp_payload_type;     /**< payload type */
    //@}

    /** The following are used for dynamic protocols (rtpdec_*.c/rdt.c) */
    //@{
    /** handler structure */
    RTPDynamicProtocolHandler *dynamic_handler;

    /** private data associated with the dynamic protocol */
    PayloadContext *dynamic_protocol_context;
    //@}

    /** Enable sending RTCP feedback messages according to RFC 4585 */
    int feedback;

    char crypto_suite[40];
    char crypto_params[100];
} RTSPStream;

#endif /* AVFORMAT_RTSP_H */
