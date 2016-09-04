#pragma once

#include "video.h"

void gpuInit();

void gpuExit();

void gpuRenderFrame(StreamState *ss);
