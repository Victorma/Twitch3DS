#include "stream.h"

#include "util.h"

Result open_stream(StreamState * ss, char * url){
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
    printf("Didn't find a audio stream\n");
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
    printf("Audio stream opened\n");
  }


  // Allocate audio frame
  ss->aFrame=av_frame_alloc();

  return 0;
}

void close_stream(StreamState * ss){

  if(ss->videoStream != -1){
    video_close_stream(ss);
  }

  if(ss->audioStream != -1){
    audio_close_stream(ss);
  }

  // Close the stream
  avformat_close_input(&ss->pFormatCtx);
}
