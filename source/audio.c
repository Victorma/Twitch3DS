/**
 *@file audio.cpp
 *@author Lectem
 *@date 14/06/2015
 */
#include <stdio.h>
#include <libavcodec/avcodec.h>
#include "sample_converter.h"
#include "stream.h"
#include "audio.h"


#define SAMPLESPERBUF 1024

//----------------------------------------------------------------------------
void fill_buffer(void *audioBuffer,size_t offset, size_t size, int frequency, StreamState * ss ) {
//----------------------------------------------------------------------------

	u32 *dest = (u32*)audioBuffer;

	for (int i=0; i<size; i++) {

		s16 sample = INT16_MAX * sin(frequency*(2*M_PI)*(offset+i)/ss->aCodecCtx->sample_rate);

		dest[i] = (sample<<16) | (sample & 0xffff);
	}


}

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

    ss->aCodecCtx = ss->pFormatCtx->streams[ss->audioStream]->codec;

    ss->aCodec = avcodec_find_decoder(ss->aCodecCtx->codec_id);

    if (!ss->aCodec)
    {
        printf("Unsupported audio codec!\n");
        return -1;
    }
    else
    {
        printf("audio decoder : %s - OK\n", ss->aCodec->name);
    }

    // Copy context
    /*ss->aCodecCtx = avcodec_alloc_context3(ss->aCodec);
    if (avcodec_copy_context(ss->aCodecCtx, ss->aCodecCtxOrig) != 0)
    {
        printf("Couldn't copy audio codec context");
        return -1; // Error copying codec context
    }*/

    if (avcodec_open2(ss->aCodecCtx, ss->aCodec, NULL) < 0)
    {
        printf("Couldn't open audio codec context");
        return -1; // Error copying codec context
    }

	  ss->aFrame=av_frame_alloc();

    switch(ss->aCodecCtx->sample_fmt){
      case AV_SAMPLE_FMT_NONE : printf("Sample_fmt is: AV_SAMPLE_FMT_NONE\n"); break;
      case AV_SAMPLE_FMT_U8   : printf("Sample_fmt is: AV_SAMPLE_FMT_U8\n"); break;
      case AV_SAMPLE_FMT_S16  : printf("Sample_fmt is: AV_SAMPLE_FMT_S16\n"); break;
      case AV_SAMPLE_FMT_S32  : printf("Sample_fmt is: AV_SAMPLE_FMT_S32\n"); break;
      case AV_SAMPLE_FMT_FLT  : printf("Sample_fmt is: AV_SAMPLE_FMT_FLT\n"); break;
      case AV_SAMPLE_FMT_DBL  : printf("Sample_fmt is: AV_SAMPLE_FMT_DBL\n"); break;
      case AV_SAMPLE_FMT_U8P  : printf("Sample_fmt is: AV_SAMPLE_FMT_U8P\n"); break;
      case AV_SAMPLE_FMT_S16P : printf("Sample_fmt is: AV_SAMPLE_FMT_S16P\n"); break;
      case AV_SAMPLE_FMT_S32P : printf("Sample_fmt is: AV_SAMPLE_FMT_S32P\n"); break;
      case AV_SAMPLE_FMT_FLTP : printf("Sample_fmt is: AV_SAMPLE_FMT_FLTP\n"); break;
      case AV_SAMPLE_FMT_DBLP : printf("Sample_fmt is: AV_SAMPLE_FMT_DBLP\n"); break;
      case AV_SAMPLE_FMT_NB   : printf("Sample_fmt is: AV_SAMPLE_FMT_NB\n"); break;
    }
    printf("Channels is: %i\n",  ss->aCodecCtx->channels);
		printf("Sample_rate is: %i\n", ss->aCodecCtx->sample_rate);

		ss->fillBlock = 0;

		//ndspChnSetEncoding(0,NDSP_ENCODING_PCM16);
		ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
  	ndspChnSetRate(0, ss->aCodecCtx->sample_rate);
  	ndspChnSetFormat(0, NDSP_FORMAT_MONO_PCM16);
    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0;
    mix[1] = 1.0;
    ndspChnSetMix(0, mix);

		memset(ss->waveBuf,0,sizeof(ss->waveBuf));


		/*

    u32 *audioBuffer = (u32*) linearAlloc(1024 * 4 * 4);
		memset(audioBuffer, 0, 1024 * 4 * 4);

  	ndspChnWaveBufAdd(0, &ss->waveBuf[0]);
  	ndspChnWaveBufAdd(0, &ss->waveBuf[1]);
  	ndspChnWaveBufAdd(0, &ss->waveBuf[2]);
  	ndspChnWaveBufAdd(0, &ss->waveBuf[3]);*/

		ss->waveBuf[0].status = NDSP_WBUF_DONE;
		ss->waveBuf[1].status = NDSP_WBUF_DONE;
		ss->waveBuf[2].status = NDSP_WBUF_DONE;
		ss->waveBuf[3].status = NDSP_WBUF_DONE;
		ss->waveBuf[4].status = NDSP_WBUF_DONE;
		ss->waveBuf[5].status = NDSP_WBUF_DONE;
		ss->waveBuf[6].status = NDSP_WBUF_DONE;
		ss->waveBuf[7].status = NDSP_WBUF_DONE;
		ss->waveBuf[8].status = NDSP_WBUF_DONE;
		ss->waveBuf[9].status = NDSP_WBUF_DONE;

		initSampleConverter(ss);

    return 0;
}

int audio_decode_frame(StreamState *ss) {
  int err, data_size = 0, frameFinished = 0;

  err = avcodec_decode_audio4(ss->aCodecCtx, ss->aFrame, &frameFinished, &ss->packet);
  if(err < 0) {
    return -1;
  }

  if (frameFinished)
  {
    data_size = sampleConvert(ss);

    //ndspChnWaveBufClear(0);

		//ndspChnWaveBufAdd(0, &ss->waveBuf[0]);
    //printf("s: %i, samp: %i, bs: %i\n", data_size,ss->aFrame->nb_samples, data_size/ss->aFrame->nb_samples) ;

    if (ss->waveBuf[ss->fillBlock].status == NDSP_WBUF_DONE) {

			//ss->waveBuf[ss->fillBlock].data_pcm16 = (s16 *) &ss->audioBuffer[ss->fillBlock * ss->aFrame->nb_samples];
      ss->waveBuf[ss->fillBlock].nsamples = ss->aFrame->nb_samples;

      //memcpy(ss->waveBuf[ss->fillBlock].data_pcm16, ss->resampled_buffer, data_size);
      DSP_FlushDataCache(ss->waveBuf[ss->fillBlock].data_pcm16,data_size);

      ndspChnWaveBufAdd(0, &ss->waveBuf[ss->fillBlock]);
      //ss->stream_offset += ss->waveBuf[ss->fillBlock].nsamples;
      ss->fillBlock = (ss->fillBlock + 1) %10;
    }else{
			printf("(LOST)\n");
		}

  }


  return 0;
}

void audio_close_stream(StreamState *ss)
{
    // Free the audio frame
    av_free(ss->aFrame);

    // Close the codec
    avcodec_close(ss->aCodecCtx);
    avcodec_close(ss->aCodecCtxOrig);

    closeSampleConverter(ss);
}
