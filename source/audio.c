/**
 *@file audio.cpp
 *@author Lectem
 *@date 14/06/2015
 */
#include <stdio.h>
#include <libavcodec/avcodec.h>
#include "stream.h"
#include "audio.h"

/**
 * open an audio stream
 *
 * mv->audioStream must be a valid audio stream
 *
 * @return 0 if opened
 * @return -1 on failure
 */
int audio_open_stream(StreamState *ss)
{
    if (ss->audioStream == -1)
        return -1;

    ss->aCodecCtxOrig = ss->pFormatCtx->streams[ss->audioStream]->codec;

    ss->aCodec = avcodec_find_decoder(ss->aCodecCtxOrig->codec_id);

    if (!ss->aCodec)
    {
        fprintf(stderr, "Unsupported audio codec!\n");
        return -1;
    }
    else
    {
        printf("audio decoder : %s - OK\n", ss->aCodec->name);
    }

    // Copy context
    ss->aCodecCtx = avcodec_alloc_context3(ss->aCodec);
    if (avcodec_copy_context(ss->aCodecCtx, ss->aCodecCtxOrig) != 0)
    {
        fprintf(stderr, "Couldn't copy audio codec context");
        return -1; // Error copying codec context
    }

    if (avcodec_open2(ss->aCodecCtx, ss->aCodec, NULL) < 0)
    {
        fprintf(stderr, "Couldn't open audio codec context");
        return -1; // Error copying codec context
    }

    return 0;
}

int audio_decode_frame(StreamState *ss) {
  int err, data_size = 0, frameFinished = 0;
  AVPacket *pkt = &ss->packet;

  err = avcodec_decode_audio4(ss->aCodecCtx, ss->aFrame, &frameFinished, pkt);
  if(err < 0) {
    return -1;
  }

  if (frameFinished)
  {
      data_size =
        av_samples_get_buffer_size
        (
            NULL,
            ss->aCodecCtx->channels,
            ss->aFrame->nb_samples,
            ss->aCodecCtx->sample_fmt,
            1
        );
      printf("Got audio!");
      //memcpy(is->audio_buf, is->audio_frame.data[0], data_size);
  }
/*  is->audio_pkt_data += len1;
  is->audio_pkt_size -= len1;*/
  if(data_size <= 0) {
	   /* No data yet, get more frames */
	   return 0;
  }
  /* We have data, return it and come back for more later */
  return data_size;
}

void audio_close_stream(StreamState *ss)
{
    avcodec_free_context(&ss->aCodecCtx);
    avcodec_close(ss->aCodecCtxOrig);
}
