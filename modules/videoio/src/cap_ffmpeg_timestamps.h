#ifndef _CAP_FFMPEG_TIMESTAMPS_H_
#define _CAP_FFMPEG_TIMESTAMPS_H_

double get_rtp_reception_time(AVFormatContext * ic);
double get_rtp_ntp_time(AVFormatContext * ic);
double get_rtcp_time(AVFormatContext * ic);
double get_rtp_time(AVFormatContext * ic);

#endif
