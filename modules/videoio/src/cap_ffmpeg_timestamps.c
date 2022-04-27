// FFmpeg internals to get timestamps
#include "libavformat/rtpdec.h"
#include "libavformat/rtsp.h"
#include "cap_ffmpeg_timestamps.h"

static RTPDemuxContext* get_RTCP_context(AVFormatContext * ic)
{
    RTSPState* rtsp_state = (RTSPState*) ic->priv_data;
    if(!rtsp_state)
        return NULL;

    RTSPStream* rtsp_stream = rtsp_state->rtsp_streams[0];
    if(!rtsp_stream)
        return NULL;

    RTPDemuxContext* rtp_demux_context = (RTPDemuxContext*) rtsp_stream->transport_priv;

    return rtp_demux_context;
}

double get_rtp_reception_time(AVFormatContext * ic)
{
    RTPDemuxContext* rtp_demux_context = get_RTCP_context(ic);
    if(!rtp_demux_context)
        return 0;

    uint64_t time = rtp_demux_context->last_rtcp_reception_time;
    uint32_t seconds = (uint32_t) ((time >> 32) & 0xffffffff);
    uint32_t fraction  = (uint32_t) (time & 0xffffffff);
    return seconds + ((double) fraction / 0xffffffff);
}

double get_rtp_ntp_time(AVFormatContext * ic)
{
    RTPDemuxContext* rtp_demux_context = get_RTCP_context(ic);
    if(!rtp_demux_context)
        return 0;

    uint64_t time = rtp_demux_context->last_rtcp_ntp_time;
    uint32_t seconds = (uint32_t) ((time >> 32) & 0xffffffff);
    uint32_t fraction  = (uint32_t) (time & 0xffffffff);
    return seconds + ((double) fraction / 0xffffffff);
}

double get_rtcp_time(AVFormatContext * ic)
{
    RTPDemuxContext* rtp_demux_context = get_RTCP_context(ic);
    if(!rtp_demux_context)
        return 0;

    return rtp_demux_context->last_rtcp_timestamp;
}

double get_rtp_time(AVFormatContext * ic)
{
    RTPDemuxContext* rtp_demux_context = get_RTCP_context(ic);
    if(!rtp_demux_context)
        return 0;

    return rtp_demux_context->timestamp;
}
