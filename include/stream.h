#pragma once

#include "3ds.h"

#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/frame.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

typedef enum{
    CONVERT_COL_SWSCALE =0,
    CONVERT_COL_Y2R     =1,
}CONVERT_COLOR_METHOD;

typedef struct StreamState {
    AVFormatContext   * pFormatCtx ;
    int                 videoStream,audioStream;

    // video codec
    AVCodecContext    *pCodecCtxOrig ;
    AVCodecContext    *pCodecCtx ;
    AVCodec           *pCodec ;

    // video frame
    AVFrame           *pFrame ;
    AVFrame           *outFrame;// 1024x1024 texture
    u16               out_bpp;
    AVPacket          packet;
    struct SwsContext *sws_ctx ;
    CONVERT_COLOR_METHOD convertColorMethod;
    Y2RU_ConversionParams params;
    Handle end_event;
    bool renderGpu;
} StreamState;


#include "video.h"
#include "color_converter.h"

Result openStream(StreamState * ss, char * url);
