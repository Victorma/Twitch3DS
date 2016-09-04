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

#include "json.h"
#include "color_converter.h"
#include "urlcode.h"
#include "video.h"
#include "http.h"
#include "util.h"
#include "gpu.h"

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x1000000

u32 __stacksize__ = 0x40000;

static u32 *SOC_buffer = NULL;
s32 sock = -1, csock = -1;

FILE * lf;

// -----------------------------------------------------------------------------
void waitForStart()
// -----------------------------------------------------------------------------
{
  while (aptMainLoop())
  {
    gspWaitForVBlank();
    hidScanInput();
    u32 kDown = hidKeysDown();
    if (kDown & KEY_START)break;
  }
}
//------------------------------------------------------------------------------
void socShutdown() {
//------------------------------------------------------------------------------
  printf("waiting for socExit...\n");
  socExit();

}


//------------------------------------------------------------------------------
__attribute__((format(printf,1,2)))
void failExit(const char *fmt, ...);
void failExit(const char *fmt, ...) {
//------------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------
void initServices()
// -----------------------------------------------------------------------------
{   // Initialize services
  int ret;

  gfxInitDefault();
  printf("Initializing the GPU...\n");
  gpuInit();
  printf("Done.\n");
  //gfxSet3D(false);//We will not be using the 3D mode in this example

  printf("Initializing the console...\n");
  lf = fopen("./twitch3ds.log","w+");
  consoleInit(GFX_BOTTOM, NULL, lf);

  printf("Initializing the network...\n");
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

	httpcInit(0); // Buffer size when POST/PUT.

}

// ---------------------------------------------------------------------------
void exitServices()
// ---------------------------------------------------------------------------
{

  fclose(lf);
  socExit();
  gpuExit();
  gfxExit();
}
// ---------------------------------------------------------------------------
void waitForStartAndExit()
// ---------------------------------------------------------------------------
{
  printf("Press start to exit\n");
  waitForStart();
  exitServices();
}

// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
// ---------------------------------------------------------------------------
{
  char token[] = "http://api.twitch.tv/api/channels/%s/access_token";
  char m3u8[] = "http://usher.twitch.tv/api/channel/hls/%s.m3u8?player=twitchweb&token=%s&sig=%s";
  char streamname[] = "ESL_TeaTime";

  char *url, *ptr, *line, *p;
  char *urlencoded;
  char *nothing = "nothing";
  char **output = &nothing;
  int output_size = 0;
  int             i, videoStream, err;
  int             frameFinished;
  json_value *    json;

  StreamState  ss;

  AVDictionary    *optionsDict = NULL;

  initServices();

  // Register all formats and codecs
  av_log_set_level(AV_LOG_ERROR);

  printf("Press start to look for streaming\n");
  waitForStart();

  // generate url for token
  asprintf(&url, token, streamname);
  //get token response
  printf("http_download_result: %ld\n", http_request(url, (u8 **)output, &output_size));
  json = json_parse(*output, output_size);
  free(*output);
  printf("\nParsed!\n");

  for (p = streamname; *p != '\0'; ++p) *p = tolower(*p);
  urlencoded = url_encode(json->u.object.values[0].value->u.string.ptr);
  asprintf(&url, m3u8, streamname, urlencoded, json->u.object.values[1].value->u.string.ptr);
  free(urlencoded);

  json_value_free(json);

  printf("http_download_result: %ld\n", http_request(url, (u8 **)output, &output_size));

  ptr = *output;
  while(nextLine(&ptr, &line) != -1){}


  printf("mobile streaming: %s \n Press start to continue\n", line);
  waitForStart();

  // Register all formats and codecs
  av_register_all();
  avformat_network_init();

  //streaming init

  memset(&ss, 0, sizeof(ss));
  ss.videoStream = -1;
  ss.audioStream = -1;
  ss.renderGpu = false;
  ss.convertColorMethod = CONVERT_COL_Y2R; // TODO : check if the format is supported
  ss.pFormatCtx = avformat_alloc_context();

  // Open video file
  if(avformat_open_input(&ss.pFormatCtx, line, NULL, NULL)!=0){
    printf("Couldn't open stream\n");
    waitForStartAndExit();
    return 0; // Couldn't open file
  }

  // Retrieve stream information
  if(avformat_find_stream_info(ss.pFormatCtx, NULL)<0){
    printf("Couldn't find stream information\n");
    waitForStartAndExit();
    return 0; // Couldn't open file
  }

  // Dump information about file onto standard error
  av_dump_format(ss.pFormatCtx, 0, line, 0);

  // Find the first video stream
  ss.videoStream=-1;
  for(i=0; i<ss.pFormatCtx->nb_streams; i++)
  if(ss.pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO) {
    ss.videoStream=i;
    break;
  }
  if(ss.videoStream==-1){
    printf("Didn't find a video stream\n");
    waitForStartAndExit();
    return 0; // Couldn't open file
  }

  if (video_open_stream(&ss)){
    printf("Couldn't open video stream\n");
    waitForStartAndExit();
    return 0; // Couldn't open file
  }else{
    printf("Video stream opened\n");
  }

  // Allocate video frame
  ss.pFrame=av_frame_alloc();
  // Allocate an AVFrame structure
  ss.outFrame = av_frame_alloc();

  ss.outFrame->width = next_pow2(ss.pCodecCtx->width);
  ss.outFrame->height = next_pow2(ss.pCodecCtx->height);

  printf("Width: %i OutW: %i\n", ss.pCodecCtx->width,ss.outFrame->width);
  printf("Height: %i OutH: %i\n", ss.pCodecCtx->height,ss.outFrame->height);
  ss.renderGpu = true;

  if (ss.renderGpu)ss.out_bpp = 4;
  else if (gfxGetScreenFormat(GFX_TOP) == GSP_BGR8_OES) ss.out_bpp = 3;
  else if (gfxGetScreenFormat(GFX_TOP) == GSP_RGBA8_OES)ss.out_bpp = 4;
  ss.outFrame->linesize[0] = ss.outFrame->width * ss.out_bpp;
  ss.outFrame->data[0] = linearMemAlign(ss.outFrame->width * ss.outFrame->height * ss.out_bpp, 0x80);//RGBA next_pow2(width) x 1024 texture

  printf("Conversion input prepared...\n");

  if (initColorConverter(&ss) < 0)
  {
      printf("Couldn't init color converter\n");
      exitColorConvert(&ss);
      waitForStartAndExit();
      return 0; // Couldn't open file
  }

  // Read frames and save first five frames to disk
  i=0;
  bool stop = false;

  printf("Start decoding...\n");

  while(av_read_frame(ss.pFormatCtx, &ss.packet)>=0 && !stop) {
    // Is this a packet from the video stream?
    if(ss.packet.stream_index==ss.videoStream) {
      // Decode video frame
      err = avcodec_decode_video2(ss.pCodecCtx, ss.pFrame, &frameFinished, &ss.packet);
      if (err <= 0)printf("decode error\n");

      // Did we get a video frame?
      if(frameFinished) {
        err = av_frame_get_decode_error_flags(ss.pFrame);
        if (err)
        {
            char buf[100];
            av_strerror(err, buf, 100);
            continue;
        }

        /*******************************
         * Conversion of decoded frame
         *******************************/
        colorConvert(&ss);

        /***********************
         * Display of the frame
         ***********************/

         if (ss.renderGpu)
         {
             gpuRenderFrame(&ss);
         }
         else
         {
           display(ss.outFrame);
           gfxSwapBuffers();
         }

        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_START)
            stop = true; // break in order to return to hbmenu
        if (i % 50 == 0)printf("frame %d\n", i);
        i++;
      }
    }

    // Free the packet that was allocated by av_read_frame
    av_free_packet(&ss.packet);
  }

  // Free the YUV frame
  av_free(ss.pFrame);
  av_free(ss.outFrame);

  // Close the codec
  avcodec_close(ss.pCodecCtx);
  avcodec_close(ss.pCodecCtxOrig);

  // Close the video file
  avformat_close_input(&ss.pFormatCtx);
  exitColorConvert(&ss);

  waitForStartAndExit();
  return 0;
}
