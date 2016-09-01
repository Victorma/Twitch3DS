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

#include "movie.h"
#include "video.h"
#include "color_converter.h"

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x512000

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



//    gfxInit(GSP_RGBA8_OES,GSP_BGR8_OES,false);
//    gfxInit(GSP_BGR8_OES,GSP_BGR8_OES,false);

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
//    char filename[]="/test400x240-mpeg4-witch.mp4";
//    char filename[]="/test400x240-witch.mp4";
//    char filename[]="/test800x400-witch-900kbps.mp4";
//    char filename[]="/test800x400-witch-1pass.mp4";
//    char filename[]="/test800x400-witch.mp4";
//    char filename[]="/test800x480-witch-mpeg4.mp4";
//    char filename[]="/test320x176-karanokyoukai.mp4";
    char filename[] = "http://video-edge-2c8ae4.iad02.hls.ttvnw.net/hls-833f94/lirik_23039948432_508662609/mobile/index-live.m3u8?token=id=2016752315297032689,bid=23039948432,exp=1472670236,node=video-edge-2c8ae4.iad02,nname=video-edge-2c8ae4.iad02,fmt=mobile&sig=b1bf82baa99b495e2c2f4532156149cf88b15863";
    MovieState mvS;

    initServices();

    // Register all formats and codecs
    av_register_all();
    av_log_set_level(AV_LOG_DEBUG);

    printf("Press start to open the file\n");
    waitForStart();
    int ret = setup(&mvS, filename);
    if (ret)
    {
        waitForStartAndExit();
        return -1;
    }

    printf("Press start to decompress\n");
    waitForStart();
    // Read frames and save first five frames to disk
    int i = 0;
    int frameFinished;

    u64 timeBeginning, timeEnd;
    u64 timeBefore, timeAfter;
    u64 timeDecodeTotal = 0, timeScaleTotal = 0, timeDisplayTotal = 0;

    timeBefore = osGetTime();
    timeBeginning = timeBefore;
    bool stop = false;

    while (av_read_frame(mvS.pFormatCtx, &mvS.packet) >= 0 && !stop)
    {
        // Is this a packet from the video stream?
        if (mvS.packet.stream_index == mvS.videoStream)
        {

            /*********************
             * Decode video frame
             *********************/

            int err = avcodec_decode_video2(mvS.pCodecCtx, mvS.pFrame, &frameFinished, &mvS.packet);
            if (err <= 0)printf("decode error\n");
            // Did we get a video frame?
            if (frameFinished)
            {
                err = av_frame_get_decode_error_flags(mvS.pFrame);
                if (err)
                {
                    char buf[100];
                    av_strerror(err, buf, 100);
                }
                timeAfter = osGetTime();
                timeDecodeTotal += timeAfter - timeBefore;

                /*******************************
                 * Conversion of decoded frame
                 *******************************/
                timeBefore = osGetTime();
                colorConvert(&mvS);
                timeAfter = osGetTime();

                /***********************
                 * Display of the frame
                 ***********************/
                timeScaleTotal += timeAfter - timeBefore;
                timeBefore = osGetTime();

                if (mvS.renderGpu)
                {
                    //gpuRenderFrame(&mvS);
                    //gpuEndFrame();
                }
                else display(mvS.outFrame);

                timeAfter = osGetTime();
                timeDisplayTotal += timeAfter - timeBefore;

                ++i;//New frame

                hidScanInput();
                u32 kDown = hidKeysDown();
                if (kDown & KEY_START)
                    stop = true; // break in order to return to hbmenu
                if (i % 50 == 0)printf("frame %d\n", i);
                timeBefore = osGetTime();
            }

        }

        // Free the packet that was allocated by av_read_frame
        av_free_packet(&mvS.packet);
    }
    timeEnd = timeBefore;

    tearup(&mvS);

    printf("Played %d frames in %f s (%f fps)\n",
           i, (timeEnd - timeBeginning) / 1000.0,
           i / ((timeEnd - timeBeginning) / 1000.0));
    printf("\tdecode:\t%llu\t%f perframe"
           "\n\tscale:\t%llu\t%f perframe"
           "\n\tdisplay:\t%llu\t%f perframe\n",
           timeDecodeTotal, timeDecodeTotal / (double) i,
           timeScaleTotal, timeScaleTotal / (double) i,
           timeDisplayTotal, timeDisplayTotal / (double) i);

    waitForStartAndExit();
    return 0;
}
