/**
 *@file audio.h
 *@author Lectem
 *@date 14/06/2015
 */
#pragma once

#include "stream.h"

int  audio_open_stream(StreamState * ss);
int  audio_decode_frame(StreamState *ss);
void audio_close_stream(StreamState * ss);
