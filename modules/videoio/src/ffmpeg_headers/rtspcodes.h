/*
 * RTSP definitions
 * copyright (c) 2002 Fabrice Bellard
 * copyright (c) 2014 Samsung Electronics. All rights reserved.
 *     @Author: Reynaldo H. Verdejo Pinochet <r.verdejo@sisa.samsung.com>
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

#ifndef AVFORMAT_RTSPCODES_H
#define AVFORMAT_RTSPCODES_H

/** RTSP handling */
enum RTSPStatusCode {
RTSP_STATUS_CONTINUE             =100,
RTSP_STATUS_OK                   =200,
RTSP_STATUS_CREATED              =201,
RTSP_STATUS_LOW_ON_STORAGE_SPACE =250,
RTSP_STATUS_MULTIPLE_CHOICES     =300,
RTSP_STATUS_MOVED_PERMANENTLY    =301,
RTSP_STATUS_MOVED_TEMPORARILY    =302,
RTSP_STATUS_SEE_OTHER            =303,
RTSP_STATUS_NOT_MODIFIED         =304,
RTSP_STATUS_USE_PROXY            =305,
RTSP_STATUS_BAD_REQUEST          =400,
RTSP_STATUS_UNAUTHORIZED         =401,
RTSP_STATUS_PAYMENT_REQUIRED     =402,
RTSP_STATUS_FORBIDDEN            =403,
RTSP_STATUS_NOT_FOUND            =404,
RTSP_STATUS_METHOD               =405,
RTSP_STATUS_NOT_ACCEPTABLE       =406,
RTSP_STATUS_PROXY_AUTH_REQUIRED  =407,
RTSP_STATUS_REQ_TIME_OUT         =408,
RTSP_STATUS_GONE                 =410,
RTSP_STATUS_LENGTH_REQUIRED      =411,
RTSP_STATUS_PRECONDITION_FAILED  =412,
RTSP_STATUS_REQ_ENTITY_2LARGE    =413,
RTSP_STATUS_REQ_URI_2LARGE       =414,
RTSP_STATUS_UNSUPPORTED_MTYPE    =415,
RTSP_STATUS_PARAM_NOT_UNDERSTOOD =451,
RTSP_STATUS_CONFERENCE_NOT_FOUND =452,
RTSP_STATUS_BANDWIDTH            =453,
RTSP_STATUS_SESSION              =454,
RTSP_STATUS_STATE                =455,
RTSP_STATUS_INVALID_HEADER_FIELD =456,
RTSP_STATUS_INVALID_RANGE        =457,
RTSP_STATUS_RONLY_PARAMETER      =458,
RTSP_STATUS_AGGREGATE            =459,
RTSP_STATUS_ONLY_AGGREGATE       =460,
RTSP_STATUS_TRANSPORT            =461,
RTSP_STATUS_UNREACHABLE          =462,
RTSP_STATUS_INTERNAL             =500,
RTSP_STATUS_NOT_IMPLEMENTED      =501,
RTSP_STATUS_BAD_GATEWAY          =502,
RTSP_STATUS_SERVICE              =503,
RTSP_STATUS_GATEWAY_TIME_OUT     =504,
RTSP_STATUS_VERSION              =505,
RTSP_STATUS_UNSUPPORTED_OPTION   =551,
};

#define RTSP_STATUS_CODE2STRING(x) (\
x >= 100 && x < FF_ARRAY_ELEMS(rtsp_status_strings) && rtsp_status_strings[x] \
)? rtsp_status_strings[x] : NULL

enum RTSPMethod {
    DESCRIBE,
    ANNOUNCE,
    OPTIONS,
    SETUP,
    PLAY,
    PAUSE,
    TEARDOWN,
    GET_PARAMETER,
    SET_PARAMETER,
    REDIRECT,
    RECORD,
    UNKNOWN = -1,
};

#endif /* AVFORMAT_RTSPCODES_H */
