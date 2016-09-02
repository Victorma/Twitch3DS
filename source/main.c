#include <3ds.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

#include <fcntl.h>

#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x512000

u32 __stacksize__ = 0x40000;

static u32 *SOC_buffer = NULL;
s32 sock = -1, csock = -1;

void waitForStart()
{
  while (aptMainLoop())
  {
    gspWaitForVBlank();
    hidScanInput();
    u32 kDown = hidKeysDown();
    if (kDown & KEY_START)break;
  }
}
//---------------------------------------------------------------------------------
void socShutdown() {
  //---------------------------------------------------------------------------------
  printf("waiting for socExit...\n");
  socExit();

}


//---------------------------------------------------------------------------------
__attribute__((format(printf,1,2)))
void failExit(const char *fmt, ...);
void failExit(const char *fmt, ...) {
  //---------------------------------------------------------------------------------

  if(sock>0) close(sock);
  if(csock>0) close(csock);

  va_list ap;

  printf(CONSOLE_RED);
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  printf(CONSOLE_RESET);
  printf("\nPress B to exit\n");

  while (aptMainLoop()) {
    gspWaitForVBlank();
    hidScanInput();

    u32 kDown = hidKeysDown();
    if (kDown & KEY_B) exit(0);
  }
}


void initServices()
{   // Initialize services
  int ret;

  hidInit();
  srvInit();
  aptInit();
  sdmcInit();
  gfxInitDefault();
  consoleInit(GFX_BOTTOM, NULL);

  // allocate buffer for SOC service
  SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);

  if(SOC_buffer == NULL) {
    failExit("memalign: failed to allocate\n");
  }

  // Now intialise soc:u service
  if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
    failExit("socInit: 0x%08X\n", (unsigned int)ret);
  }

  // register socShutdown to run at exit
  // atexit functions execute in reverse order so this runs before gfxExit
  atexit(socShutdown);

  printf("Initializing the GPU...\n");
  printf("Done.\n");
}

void exitServices()
{
  // Cleanup SOC
  socExit();

  gfxExit();
  sdmcExit();
  hidExit();
  aptExit();
  srvExit();
}

void waitForStartAndExit()
{
  printf("Press start to exit\n");
  waitForStart();
  exitServices();
}


int main(int argc, char *argv[])
{
  char filename[] = "http://video-edge-2c8ae4.iad02.hls.ttvnw.net/hls-833f94/lirik_23039948432_508662609/mobile/index-live.m3u8?token=id=2016752315297032689,bid=23039948432,exp=1472670236,node=video-edge-2c8ae4.iad02,nname=video-edge-2c8ae4.iad02,fmt=mobile&sig=b1bf82baa99b495e2c2f4532156149cf88b15863";
  AVFormatContext *pFormatCtx = NULL;
  int             i, videoStream;
  AVCodecContext  *pCodecCtx = NULL;
  AVCodec         *pCodec = NULL;
  AVFrame         *pFrame = NULL;
  AVPacket        packet;
  int             frameFinished;
  int             numBytes;

  AVDictionary    *optionsDict = NULL;

  initServices();

  // Register all formats and codecs
  av_log_set_level(AV_LOG_DEBUG);

  printf("Press start to open the file\n");
  waitForStart();
  // Register all formats and codecs
  av_register_all();
  avformat_network_init();

  // Open video file
  if(avformat_open_input(&pFormatCtx, filename, NULL, NULL)!=0){
    printf("Couldn't open stream\n");
    waitForStartAndExit();
    return 0; // Couldn't open file
  }

  // Retrieve stream information
  if(avformat_find_stream_info(pFormatCtx, NULL)<0){
    printf("Couldn't find stream information\n");
    waitForStartAndExit();
    return 0; // Couldn't open file
  }

  // Dump information about file onto standard error
  av_dump_format(pFormatCtx, 0, argv[1], 0);

  // Find the first video stream
  videoStream=-1;
  for(i=0; i<pFormatCtx->nb_streams; i++)
  if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
    videoStream=i;
    break;
  }
  if(videoStream==-1)
    return -1; // Didn't find a video stream

  // Get a pointer to the codec context for the video stream
  pCodecCtx=pFormatCtx->streams[videoStream]->codec;

  // Find the decoder for the video stream
  pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
  if(pCodec==NULL) {
    fprintf(stderr, "Unsupported codec!\n");
    return -1; // Codec not found
  }
  // Open codec
  if(avcodec_open2(pCodecCtx, pCodec, &optionsDict)<0)
    return -1; // Could not open codec

  // Allocate video frame
  pFrame=av_frame_alloc();

  // Read frames and save first five frames to disk
  i=0;
  while(av_read_frame(pFormatCtx, &packet)>=0) {
    // Is this a packet from the video stream?
    if(packet.stream_index==videoStream) {
      // Decode video frame
      avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished,&packet);

      // Did we get a video frame?
      if(frameFinished) {
        printf("frame!\n");
      }
    }

    // Free the packet that was allocated by av_read_frame
    av_free_packet(&packet);
  }

  // Free the YUV frame
  av_free(pFrame);

  // Close the codec
  avcodec_close(pCodecCtx);

  // Close the video file
  avformat_close_input(&pFormatCtx);

  waitForStartAndExit();
  return 0;
}
