#include "http.h"

#include <stdio.h>
#include <stdlib.h>

Result http_request(const char *url, u8 ** output, int * output_size)
{
    Result ret=0;
    httpcContext context;
    char *newurl=NULL;
    u32 statuscode=0;
    u32 contentsize=0;
    u8 *buf;

    do {
        ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);

        // This disables SSL cert verification, so https:// will be usable
        ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);

        // Enable Keep-Alive connections (on by default, pending ctrulib merge)
        // ret = httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
        // printf("return from httpcSetKeepAlive: %"PRId32"\n",ret);
        // gfxFlushBuffers();

        // Set a User-Agent header so websites can identify your application
        ret = httpcAddRequestHeaderField(&context, "User-Agent", "httpc-example/1.0.0");

        // Tell the server we can support Keep-Alive connections.
        // This will delay connection teardown momentarily (typically 5s)
        // in case there is another request made to the same server.
        ret = httpcAddRequestHeaderField(&context, "Connection", "Keep-Alive");

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
        printf("URL returned status: %ld\n", statuscode);
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

    httpcCloseContext(&context);
    if (newurl!=NULL) free(newurl);

    *output = buf;
    *output_size = contentsize;

    return 0;
}
