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
#include "urlcode.h"

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x1000000

u32 __stacksize__ = 0x40000;

static u32 *SOC_buffer = NULL;
s32 sock = -1, csock = -1;

FILE * lf;

void waitForStart();

Result http_request(const char *url, u8 ** output, int * output_size)
{
    Result ret=0;
    httpcContext context;
    char *newurl=NULL;
    u32 statuscode=0;
    u32 contentsize=0;
    u8 *buf;

    printf("Downloading %s\n",url);
    gfxFlushBuffers();

    do {
        ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);
        printf("return from httpcOpenContext: %"PRId32"\n",ret);
        gfxFlushBuffers();

        // This disables SSL cert verification, so https:// will be usable
        ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
        printf("return from httpcSetSSLOpt: %"PRId32"\n",ret);
        gfxFlushBuffers();

        // Enable Keep-Alive connections (on by default, pending ctrulib merge)
        // ret = httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
        // printf("return from httpcSetKeepAlive: %"PRId32"\n",ret);
        // gfxFlushBuffers();

        // Set a User-Agent header so websites can identify your application
        ret = httpcAddRequestHeaderField(&context, "User-Agent", "httpc-example/1.0.0");
        printf("return from httpcAddRequestHeaderField: %"PRId32"\n",ret);
        gfxFlushBuffers();

        // Tell the server we can support Keep-Alive connections.
        // This will delay connection teardown momentarily (typically 5s)
        // in case there is another request made to the same server.
        ret = httpcAddRequestHeaderField(&context, "Connection", "Keep-Alive");
        printf("return from httpcAddRequestHeaderField: %"PRId32"\n",ret);
        gfxFlushBuffers();

        ret = httpcBeginRequest(&context);
        if(ret!=0){
            httpcCloseContext(&context);
            if(newurl!=NULL) free(newurl);
            return ret;
        }

        ret = httpcGetResponseStatusCode(&context, &statuscode);
        if(ret!=0){
            httpcCloseContext(&context);
            if(newurl!=NULL) free(newurl);
            return ret;
        }

        if ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308)) {
            if(newurl==NULL) newurl = malloc(0x1000); // One 4K page for new URL
            if (newurl==NULL){
                httpcCloseContext(&context);
                return -1;
            }
            ret = httpcGetResponseHeader(&context, "Location", newurl, 0x1000);
            url = newurl; // Change pointer to the url that we just learned
            printf("redirecting to url: %s\n",url);
            httpcCloseContext(&context); // Close this context before we try the next
        }
    } while ((statuscode >= 301 && statuscode <= 303) || (statuscode >= 307 && statuscode <= 308));

    if(statuscode!=200){
        printf("URL returned status: %"PRId32"\n", statuscode);
        httpcCloseContext(&context);
        if(newurl!=NULL) free(newurl);
        return -2;
    }

    // This relies on an optional Content-Length header and may be 0
    ret=httpcGetDownloadSizeState(&context, NULL, &contentsize);
    if(ret!=0){
        httpcCloseContext(&context);
        if(newurl!=NULL) free(newurl);
        return ret;
    }

    printf("reported size: %"PRId32"\n",contentsize);
    gfxFlushBuffers();

    // Start with a single page buffer
    buf = (u8*)malloc(contentsize);
    if(buf==NULL){
        httpcCloseContext(&context);
        if(newurl!=NULL) free(newurl);
        return -1;
    }

    // This download loop resizes the buffer as data is read.
    ret = httpcReceiveData(&context, buf, contentsize);
    if(ret!=0){
        httpcCloseContext(&context);
        if(newurl!=NULL) free(newurl);
        free(buf);
        return -1;
    }

    printf("downloaded size: %"PRId32"\n",contentsize);
    gfxFlushBuffers();

    httpcCloseContext(&context);
    if (newurl!=NULL) free(newurl);

    *output = buf;
    *output_size = contentsize;

    return 0;
}

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
  lf = fopen("./twitch3ds.log","w+");
  consoleInit(GFX_BOTTOM, NULL, lf);

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

  printf("Initializing the GPU...\n");
  printf("Done.\n");
}

void exitServices()
{

  fclose(lf);
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

int nextLine(char ** s, char ** l)
{
  char * nextLine = NULL;

  if(*s == NULL || **s == '\0')
    return -1;

  *l = *s;

  nextLine = strchr(*s, '\n');
  if (nextLine){
    *nextLine = '\0';
    *s = nextLine+1;
  }else{
    nextLine = strchr(*s, '\0');
    *s = nextLine;
  }

  return 1;
}


int main(int argc, char *argv[])
{
  char token[] = "http://api.twitch.tv/api/channels/%s/access_token";
  char m3u8[] = "http://usher.twitch.tv/api/channel/hls/%s.m3u8?player=twitchweb&token=%s&sig=%s";
  char streamname[] = "yoda";

  char *url, *ptr, *line;
  char *urlencoded;
  char *nothing = "nothing";
  char **output = &nothing;
  int output_size = 0;
  char filename[] = "";
  AVFormatContext *pFormatCtx = NULL;
  int             i, videoStream;
  AVCodecContext  *pCodecCtx = NULL;
  AVCodec         *pCodec = NULL;
  AVFrame         *pFrame = NULL;
  AVPacket        packet;
  int             frameFinished;
  int             numBytes;
  json_value *    json;

  AVDictionary    *optionsDict = NULL;

  initServices();

  // Register all formats and codecs
  av_log_set_level(AV_LOG_DEBUG);

  printf("Press start to look for streaming\n");
  waitForStart();

  // generate url for token
  asprintf(&url, token, streamname);
  //get token response
  printf("http_download_result: %ld\n", http_request(url, (u8 **)output, &output_size));
  json = json_parse(*output, output_size);
  free(*output);
  printf("\nParsed!\n");

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

  // Open video file
  if(avformat_open_input(&pFormatCtx, line, NULL, NULL)!=0){
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
