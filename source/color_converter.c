/**
 *@file color_converter.c
 *@author Lectem
 *@date 18/06/2015
 */
#include <libswscale/swscale.h>
#include "color_converter.h"
#include <3ds.h>

#include <assert.h>
#include <3ds/services/y2r.h>

static inline double u64_to_double(u64 value)
{
    return (((double) (u32) (value >> 32)) * 0x100000000ULL + (u32) value);
}

int initColorConverterSwscale(StreamState *ss)
{
    enum AVPixelFormat out_fmt = AV_PIX_FMT_BGR24;
    if (gfxGetScreenFormat(GFX_TOP) == GSP_RGBA8_OES) out_fmt = AV_PIX_FMT_BGRA;
    // initialize SWS context for software scaling
    ss->sws_ctx = sws_getContext(ss->pCodecCtx->width, ss->pCodecCtx->height, ss->pCodecCtx->pix_fmt,
                                  400, 240, out_fmt,
                                  0,//SWS_BILINEAR,//TODO perf comparison
                                  NULL,
                                  NULL,
                                  NULL
    );


    if (ss->sws_ctx == NULL)
    {
        printf("Couldnt initialize sws\n");
        return -1;
    }
    return 0;
}


int exitColorConvertSwscale(StreamState *ss)
{
    sws_freeContext(ss->sws_ctx);
    return 0;
}


int colorConvertSwscale(StreamState *ss)
{
    // Convert the image from its native format to RGB
    sws_scale(ss->sws_ctx, (uint8_t const *const *) ss->pFrame->data,
              ss->pFrame->linesize, 0, ss->pCodecCtx->height,
              ss->outFrame->data, ss->outFrame->linesize);
    return 0;
}

#define CHECKRES() do{if(res != 0){printf("error %lx L%d %s\n",(u32)res,__LINE__,osStrError(res));return -1;}}while(0);

int ffmpegPixelFormatToY2R(enum AVPixelFormat pix_fmt)
{
    switch (pix_fmt)
    {
        case AV_PIX_FMT_YUV420P     :
            return INPUT_YUV420_INDIV_8;
        case AV_PIX_FMT_YUV422P     :
            return INPUT_YUV422_INDIV_8;
        case AV_PIX_FMT_YUV420P16LE :
            return INPUT_YUV420_INDIV_16;
        case AV_PIX_FMT_YUV422P16LE :
            return INPUT_YUV422_INDIV_16;
        case AV_PIX_FMT_YUYV422     :
            return INPUT_YUV422_BATCH;
        default:
            printf("unknown format %d, using INPUT_YUV420_INDIV_8\n", pix_fmt);
            return INPUT_YUV420_INDIV_8;
    }
}

int initColorConverterY2r(StreamState *ss)
{
    Result res = 0;
    res = y2rInit();
    CHECKRES();
    res = Y2RU_StopConversion();
    CHECKRES();
    bool is_busy = 0;
    int tries = 0;
    do
    {
        svcSleepThread(100000ull);
        res = Y2RU_StopConversion();
        CHECKRES();
        res = Y2RU_IsBusyConversion(&is_busy);
        CHECKRES();
        tries += 1;
//        printf("is_busy %d\n",is_busy);
    } while (is_busy && tries < 100);

    ss->params.input_format = ffmpegPixelFormatToY2R(ss->pCodecCtx->pix_fmt);
    if (ss->out_bpp == 3) ss->params.output_format = OUTPUT_RGB_24;
    else if (ss->out_bpp == 4) ss->params.output_format = OUTPUT_RGB_32;
    ss->params.rotation = ROTATION_NONE;
    ss->params.block_alignment = BLOCK_8_BY_8;
    ss->params.input_line_width = ss->pCodecCtx->width;
    ss->params.input_lines = ss->pCodecCtx->height;
    if (ss->params.input_lines % 8)
    {
        ss->params.input_lines += 8 - ss->params.input_lines % 8;
        printf("Height not multiple of 8, cropping to %dpx\n", ss->params.input_lines);
    }
    ss->params.standard_coefficient = COEFFICIENT_ITU_R_BT_601;//TODO : detect
    ss->params.unused = 0;
    ss->params.alpha = 0xFF;

    res = Y2RU_SetConversionParams(&ss->params);
    CHECKRES();

    res = Y2RU_SetTransferEndInterrupt(true);
    CHECKRES();
    ss->end_event = 0;
    res = Y2RU_GetTransferEndEvent(&ss->end_event);
    CHECKRES();


    return 0;
}


int colorConvertY2R(StreamState *ss)
{
    Result res;

    const u16 img_w = ss->params.input_line_width;
    const u16 img_h = ss->params.input_lines;
    const u32 img_size = img_w * img_h;

    size_t src_Y_size = 0;
    size_t src_UV_size = 0;
    switch (ss->params.input_format)
    {
        case INPUT_YUV422_INDIV_8:
            src_Y_size = img_size;
            src_UV_size = img_size / 2;
            break;
        case INPUT_YUV420_INDIV_8:
            src_Y_size = img_size;
            src_UV_size = img_size / 4;
            break;
        case INPUT_YUV422_INDIV_16:
            src_Y_size = img_size * 2;
            src_UV_size = img_size / 2 * 2;
            break;
        case INPUT_YUV420_INDIV_16:
            src_Y_size = img_size * 2;
            src_UV_size = img_size / 4 * 2;
            break;
        case INPUT_YUV422_BATCH:
            src_Y_size = img_size * 2;
            src_UV_size = img_size * 2;
            break;
    }
    if (ss->params.input_format == INPUT_YUV422_BATCH)
    {
        //TODO : test it
        assert(ss->pFrame->linesize[0] >= src_Y_size);
        res = Y2RU_SetSendingYUYV(ss->pFrame->data[0], src_Y_size, img_w, ss->pFrame->linesize[0] - src_Y_size);
    }
    else
    {
        const u8 *src_Y = ss->pFrame->data[0];
        const u8 *src_U = ss->pFrame->data[1];
        const u8 *src_V = ss->pFrame->data[2];
        const u16 src_Y_padding = ss->pFrame->linesize[0] - img_w;
        const u16 src_UV_padding = ss->pFrame->linesize[1] - (img_w >> 1);

        res = Y2RU_SetSendingY(src_Y, src_Y_size, img_w, src_Y_padding);
        CHECKRES();
        res = Y2RU_SetSendingU(src_U, src_UV_size, img_w >> 1, src_UV_padding);
        CHECKRES();
        res = Y2RU_SetSendingV(src_V, src_UV_size, img_w >> 1, src_UV_padding);
        CHECKRES();
    }

    const u16 out_bpp = ss->out_bpp;
    size_t rgb_size = img_size * out_bpp;
    s16 gap = (ss->outFrame->width - img_w) * 8 * out_bpp;
    if (ss->outFrame->width * 8 * out_bpp >= 0x8000)
    {
        printf("This image is too wide for y2r.\n");
        return 1;
        //TODO : check at setup
    }
    res = Y2RU_SetReceiving(ss->outFrame->data[0], rgb_size, img_w * 8 * out_bpp, gap);
    CHECKRES();
    res = Y2RU_StartConversion();
    CHECKRES();
    u64 beforeTick = svcGetSystemTick();
    res = svcWaitSynchronization(ss->end_event, 1000 * 1000 * 10);
    u64 afterTick = svcGetSystemTick();

#define TICKS_PER_USEC 268.123480
#define TICKS_PER_MSEC 268123.480
    if (res)
    {
        printf("outdim %d unitsize %d gap %d\n", ss->outFrame->width * 8 * out_bpp, img_w * 8 * out_bpp, gap);
        printf("svcWaitSynchronization %lx\n", res);
    }
//    else printf("waited %lf\n",u64_to_double(afterTick-beforeTick)/TICKS_PER_USEC);

    return 0;
}

int exitColorConvertY2R(StreamState *ss)
{
    Result res = 0;
    bool is_busy = 0;

    Y2RU_StopConversion();
    Y2RU_IsBusyConversion(&is_busy);
    y2rExit();
    return 0;
}


int initColorConverter(StreamState *ss)
{
    switch (ss->convertColorMethod)
    {
        default:
        case CONVERT_COL_SWSCALE:
            return initColorConverterSwscale(ss);
        case CONVERT_COL_Y2R:
            return initColorConverterY2r(ss);
    }
}


int colorConvert(StreamState *ss)
{
    switch (ss->convertColorMethod)
    {
        default:
        case CONVERT_COL_SWSCALE:
            return colorConvertSwscale(ss);
        case CONVERT_COL_Y2R:
            return colorConvertY2R(ss);
    }
}

int exitColorConvert(StreamState *ss)
{
    switch (ss->convertColorMethod)
    {
        default:
        case CONVERT_COL_SWSCALE:
            return exitColorConvertSwscale(ss);
        case CONVERT_COL_Y2R:
            return exitColorConvertY2R(ss);
    }
}
