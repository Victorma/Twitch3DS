/**
 *@file video.h
 *@author Lectem
 *@date 14/06/2015
 */
#pragma once

#include "3ds.h"

#include "stream.h"

int video_open_stream(StreamState *ss);
int video_decode_frame(StreamState * ss);

void display(AVFrame *pFrame);
