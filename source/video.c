/**
 *@file video.c
 *@author Lectem
 *@date 14/06/2015
 */
#include <3ds.h>
#include <3ds/gfx.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#include "video.h"
#include "gpu.h"


const bool USE_GPU = true;

int video_open_stream(StreamState *ss)
{
    if (ss->videoStream == -1)
        return -1;

    // Get a pointer to the codec context for the video stream
    ss->pCodecCtxOrig = ss->pFormatCtx->streams[ss->videoStream]->codec;
    // Find the decoder for the video stream
    ss->pCodec = avcodec_find_decoder(ss->pCodecCtxOrig->codec_id);
    if (ss->pCodec == NULL)
    {
        fprintf(stderr, "Unsupported video codec!\n");
        return -1; // Codec not found
    }

    // Copy context
    ss->pCodecCtx = avcodec_alloc_context3(ss->pCodec);
    if (avcodec_copy_context(ss->pCodecCtx, ss->pCodecCtxOrig) != 0)
    {
        fprintf(stderr, "Couldn't copy video codec context");
        return -1; // Error copying codec context
    }
    // Open codec
    if (avcodec_open2(ss->pCodecCtx, ss->pCodec, NULL) < 0)
    {
        printf("Could not open codec\n");
        return -1;
    }

    // Allocate video frame
    ss->pFrame=av_frame_alloc();

    // Allocate an AVFrame structure
    ss->outFrame = av_frame_alloc();
    ss->outFrame->width = next_pow2(ss->pCodecCtx->width);
    ss->outFrame->height = next_pow2(ss->pCodecCtx->height);

    printf("Width: %i OutW: %i\n", ss->pCodecCtx->width,ss->outFrame->width);
    printf("Height: %i OutH: %i\n", ss->pCodecCtx->height,ss->outFrame->height);

    ss->renderGpu = USE_GPU;

    // bpp config
    if (ss->renderGpu)ss->out_bpp = 4;
    else if (gfxGetScreenFormat(GFX_TOP) == GSP_BGR8_OES) ss->out_bpp = 3;
    else if (gfxGetScreenFormat(GFX_TOP) == GSP_RGBA8_OES)ss->out_bpp = 4;
    ss->outFrame->linesize[0] = ss->outFrame->width * ss->out_bpp;
    ss->outFrame->data[0] = linearMemAlign(ss->outFrame->width * ss->outFrame->height * ss->out_bpp, 0x80);//RGBA next_pow2(width) x 1024 texture

    printf("Conversion input prepared...\n");

    if (initColorConverter(ss) < 0)
    {
        printf("Couldn't init color converter\n");
        exitColorConvert(ss);
        return -1; // Couldn't open file
    }

    return 0;
}

void video_close_stream(StreamState *ss){
		// Free the YUV frame
		av_free(ss->pFrame);
		av_free(ss->outFrame);

		// Close the codec
		avcodec_close(ss->pCodecCtx);
		avcodec_close(ss->pCodecCtxOrig);

		exitColorConvert(ss);
}

int video_decode_frame(StreamState * ss){
  int frameFinished, err;
  // Decode video frame
  err = avcodec_decode_video2(ss->pCodecCtx, ss->pFrame, &frameFinished, &ss->packet);
  if (err <= 0)printf("decode error\n");

  // Did we get a video frame?
  if(frameFinished) {
    err = av_frame_get_decode_error_flags(ss->pFrame);
    if (err)
    {
        char buf[100];
        av_strerror(err, buf, 100);
        return -1;
    }
    /*******************************
     * Conversion of decoded frame
     *******************************/
    colorConvert(ss);

    /***********************
     * Display of the frame
     ***********************/
     if (ss->renderGpu)
     {
         gpuRenderFrame(ss);
     }
     else
     {
       display(ss->outFrame);
       gfxSwapBuffers();
     }
  }

  return 0;
}

void display(AVFrame *pFrame)
{
//    gspWaitForVBlank();
//    gfxSwapBuffers();

    int i, j, c;
    const int width  = 400 <= pFrame->width ? 400  : pFrame->width;
    const int height = 240 <= pFrame->height ? 240 : pFrame->height;
    const int fwidth = pFrame->width;
    if (gfxGetScreenFormat(GFX_TOP) == GSP_BGR8_OES)
    {
        u8 *const src = pFrame->data[0];
        u8 *const fbuffer = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, 0, 0);

        for (i = 0; i < width; ++i)
        {
            for (j = 0; j < height; ++j)
            {
                for (c = 0; c < 3; ++c)
                {
                    fbuffer[3 * 240 * i + (239 - j) * 3 + c] = src[fwidth * 3 * j + i * 3 + c];
                }
            }
        }
    }
    else if (gfxGetScreenFormat(GFX_TOP) == GSP_RGBA8_OES)
    {

        u32 *const src     = (u32 *) pFrame->data[0];
        u32 *const fbuffer = (u32 *) gfxGetFramebuffer(GFX_TOP, GFX_LEFT, 0, 0);
        for (i = 0; i < width; ++i)
        {
            for (j = 0; j < height; ++j)
            {
                fbuffer[239 * i + (239 - j)] = src[fwidth * j + i];
            }
        }
    }
    else
    {
        printf("format not supported\n");
    }
}
