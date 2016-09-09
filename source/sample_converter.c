#include "sample_converter.h"
#include "3ds.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>

Result initSampleConverter(StreamState * ss){
  ss->spl_swr_ctx = swr_alloc();

  av_opt_set_int(ss->spl_swr_ctx, "in_channel_layout",  ss->aCodecCtx->channel_layout, 0);
  av_opt_set_int(ss->spl_swr_ctx, "out_channel_layout", AV_CH_LAYOUT_MONO,  0);
  av_opt_set_int(ss->spl_swr_ctx, "in_sample_rate",     ss->aCodecCtx->sample_rate, 0);
  av_opt_set_int(ss->spl_swr_ctx, "out_sample_rate",    ss->aCodecCtx->sample_rate, 0);
  av_opt_set_sample_fmt(ss->spl_swr_ctx, "in_sample_fmt",  AV_SAMPLE_FMT_FLTP, 0);
  av_opt_set_sample_fmt(ss->spl_swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16,  0);
  swr_init(ss->spl_swr_ctx);

  printf("sample rate %i\n", ss->aCodecCtx->sample_rate);

  return 0;
}

Result sampleConvert(StreamState * ss){
  int out_samples, output_data_size = 0,
    size = output_data_size = av_samples_get_buffer_size(&ss->linesize, ss->aCodecCtx->channels,ss->aFrame->nb_samples*10, AV_SAMPLE_FMT_S16, 1);
  if(ss->aFrame->nb_samples != ss->nb_samples){
    //printf()
    //ss->aFrame->nb_samples * ss->aCodecCtx->channels * 4 * 4;
    ss->audioBuffer = (u32*) linearAlloc(size);
		memset(ss->audioBuffer, 0, size);
    ss->waveBuf[0].data_vaddr = &ss->audioBuffer[0];
    ss->waveBuf[1].data_vaddr = &ss->audioBuffer[(size/40) * 1];
    ss->waveBuf[2].data_vaddr = &ss->audioBuffer[(size/40) * 2];
    ss->waveBuf[3].data_vaddr = &ss->audioBuffer[(size/40) * 3];
    ss->waveBuf[4].data_vaddr = &ss->audioBuffer[(size/40) * 4];
    ss->waveBuf[5].data_vaddr = &ss->audioBuffer[(size/40) * 5];
    ss->waveBuf[6].data_vaddr = &ss->audioBuffer[(size/40) * 6];
    ss->waveBuf[7].data_vaddr = &ss->audioBuffer[(size/40) * 7];
    ss->waveBuf[8].data_vaddr = &ss->audioBuffer[(size/40) * 8];
    ss->waveBuf[9].data_vaddr = &ss->audioBuffer[(size/40) * 9];

    //av_samples_alloc(&ss->resampled_buffer, &ss->linesize, ss->aCodecCtx->channels, ss->aFrame->nb_samples, AV_SAMPLE_FMT_S16P, 0);
    ss->nb_samples = ss->aFrame->nb_samples;
  }
  u32* dir = &ss->audioBuffer[(size / 40) * ss->fillBlock];
  out_samples = swr_convert(ss->spl_swr_ctx,
                          (uint8_t **)&dir,
                           ss->aFrame->nb_samples,
                          (const uint8_t **) ss->aFrame->extended_data,
                           ss->aFrame->nb_samples);

  // recompute output_data_size following swr_convert result (number of samples actually converted)
  output_data_size = av_samples_get_buffer_size(&ss->linesize, ss->aCodecCtx->channels,
                                          out_samples,
                                          AV_SAMPLE_FMT_S16, 1);

  //swr_convert(ss->spl_swr_ctx, &ss->resampled_buffer, ss->aFrame->nb_samples, (const uint8_t **) ss->aFrame->extended_data, ss->aFrame->nb_samples);
  return output_data_size;
}

Result closeSampleConverter(StreamState * ss){
  /*printf("liberando resampled_buffer");
  av_freep(ss->resampled_buffer);*/

  swr_close(ss->spl_swr_ctx);
  swr_free(&ss->spl_swr_ctx);

  return 0;
}
