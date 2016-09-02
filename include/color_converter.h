/**
 *@file color_converter.h
 *@author Lectem
 *@date 18/06/2015
 */
#pragma once

#include "video.h"
#include <3ds.h>

int initColorConverter(StreamState* ss);

int colorConvert(StreamState* ss);

int exitColorConvert(StreamState* ss);
