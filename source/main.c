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

#include "gpu.h"
#include "twitch.h"
#include "stream.h"

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x1000000

u32 __stacksize__ = 0x40000;

typedef enum
{
	STATE_NONE,
  STATE_GAME_SELECTING,
  STATE_CHANNEL_SELECTING,
	STATE_PLAYING
}state_t;

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
	ndspInit();
	ndspSetOutputMode(NDSP_OUTPUT_STEREO);

  // Register all formats and codecs
  av_register_all();
  avformat_network_init();
  av_log_set_level(AV_LOG_ERROR);

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
	ndspExit();
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
  int i;

  Result res                = -1;

  StreamState ss;
  game_page gp;
  game_stream_page gsp;
  stream_sources gss;

  state_t state             = STATE_NONE;

  initServices();


	// Main loop
	while (aptMainLoop())
	{
		//Scan all the inputs. This should be done once for each frame
		hidScanInput();

		//hidKeysDown returns information about which buttons have been just pressed (and they weren't in the previous frame)
		u32 kDown = hidKeysDown();

		printf("\x1b[0;0H");
		printf("----------------------                 \n");
		printf("|      Twitch3ds     |                 \n");
		printf("----------------------                 \n");
		printf("                                       \n");
		printf("                                       \n");

		switch(state)
		{
			case STATE_NONE:
				{
					printf("  Press A to see game list         \n");
  				printf("  Press START to exit              \n");
					printf("                                   \n");
					if(kDown & KEY_A){
            printf(" => Adquiring games list...         ");
	          if(getGameList(&gp, 0) != 0){
	            printf(" ERROR! \n");
						}else{
	            printf(" OK! \n");
	            i = 0;
	            state = STATE_GAME_SELECTING;
						}
          }
				}
				break;
			case STATE_GAME_SELECTING:
				{
					printf("  Select game and use A to select:  \n");
					printf("   -> %s                            \n", gp.g[i].name);
					printf("                                    \n");

          if(kDown & KEY_DOWN){
            i = (i+1) % 10;
          }else if(kDown & KEY_UP){
            i = (i+9) % 10;
          }else if(kDown & KEY_A){
            printf(" => Adquiring channels list... ");
            if(getGameStreams(&gsp, gp.g[i].name) != 0){
	            printf(" ERROR! \n");
						}else{
	            printf(" OK! \n");
	            i = 0;
	            state = STATE_CHANNEL_SELECTING;
						}
          }else{
						printf("                                                 \n");
					}
				}
				break;
			case STATE_CHANNEL_SELECTING:
				{
					printf("  Select channel and A to select:  \n");
					printf("   -> %s                           \n", gsp.s[i].name);
					printf("                                   \n");

          if(kDown & KEY_DOWN){
            i = (i+1) % 10;
          }else if(kDown & KEY_UP){
            i = (i+9) % 10;
          }else if(kDown & KEY_A){
            printf(" => Opening stream... ");
            if(getStreamSources(&gss, gsp.s[i].name) != 0){
	            printf(" ERROR! \n");
						}else{
	            printf(" OK! \n");
	            i = 0;
	            state = STATE_PLAYING;
						}
          }else if(kDown & KEY_B){
            state = STATE_GAME_SELECTING;
          }else{
						printf("                                                 \n");
					}
				}
				break;
			case STATE_PLAYING:
				{
					printf("  Opening streaming                    \n");
					printf("                                       \n");
					printf("                                       \n");
					printf("                                       \n");

					res = open_stream(&ss, gss.mobile);
					if(res != 0){
						printf("  Failed opening streaming             \n");
						state = STATE_CHANNEL_SELECTING;
						break;
					}

					// Read frames and save first five frames to disk
					printf(" Start decoding...                    \n");
				  bool stop = false;
				  while(av_read_frame(ss.pFormatCtx, &ss.packet)>=0 && !stop) {
				    // Is this a packet from the video stream?
				    if(ss.packet.stream_index==ss.videoStream) {
							video_decode_frame(&ss);
				    }/*else if(ss.packet.stream_index==ss.audioStream) {
							audio_decode_frame(&ss);
				    }*/

				    hidScanInput();
				    u32 kDown = hidKeysDown();
				    if (kDown & KEY_B)
				        stop = true; // break in order to return to hbmenu

						// Free the packet that was allocated by av_read_frame
						av_free_packet(&ss.packet);
					}

					close_stream(&ss);

					state = STATE_CHANNEL_SELECTING;
					printf("                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       ");
					printf("                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       ");
					printf("                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       ");
			  }
				break;

		}

		if (kDown & KEY_START) break;

		gspWaitForVBlank();
	}


  waitForStartAndExit();
  return 0;
}
