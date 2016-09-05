#include "stream.h"

#include "util.h"

Result openStream(StreamState * ss, char * url){
  int i = 0;

  //streaming init
  memset(ss, 0, sizeof(*ss));
  ss->videoStream = -1;
  ss->audioStream = -1;
  ss->renderGpu = false;
  ss->convertColorMethod = CONVERT_COL_Y2R; // TODO : check if the format is supported
  ss->pFormatCtx = avformat_alloc_context();

  // Open video file
  if(avformat_open_input(&ss->pFormatCtx, url, NULL, NULL)!=0){
    printf("Couldn't open stream\n");
    return -1; // Couldn't open file
  }

  // Retrieve stream information
  if(avformat_find_stream_info(ss->pFormatCtx, NULL)<0){
    printf("Couldn't find stream information\n");
    return -1; // Couldn't open file
  }

  // Dump information about file onto standard error
  av_dump_format(ss->pFormatCtx, 0, url, 0);

  // Find the first video stream
  ss->videoStream=-1;
  for(i=0; i<ss->pFormatCtx->nb_streams; i++){
    if(ss->pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)ss->videoStream=i;
    else if(ss->pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO) ss->audioStream=i;
  }

  if(ss->videoStream==-1){
    printf("Didn't find a video stream\n");
    return -1; // Couldn't open file
  }
  if(ss->audioStream==-1){
    printf("Didn't find a video stream\n");
    return -1; // Couldn't open file
  }

  if (video_open_stream(ss)){
    printf("Couldn't open video stream\n");
    return -1; // Couldn't open file
  }else{
    printf("Video stream opened\n");
  }

  if (audio_open_stream(ss)){
    printf("Couldn't open audio stream\n");
    return -1; // Couldn't open file
  }else{
    printf("Video stream opened\n");
  }

  // Allocate video frame
  ss->pFrame=av_frame_alloc();
  // Allocate audio frame
  ss->aFrame=av_frame_alloc();
  // Allocate an AVFrame structure
  ss->outFrame = av_frame_alloc();

  ss->outFrame->width = next_pow2(ss->pCodecCtx->width);
  ss->outFrame->height = next_pow2(ss->pCodecCtx->height);

  printf("Width: %i OutW: %i\n", ss->pCodecCtx->width,ss->outFrame->width);
  printf("Height: %i OutH: %i\n", ss->pCodecCtx->height,ss->outFrame->height);
  ss->renderGpu = true;

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
