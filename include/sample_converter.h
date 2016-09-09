#pragma once

#include <3ds.h>
#include "stream.h"

Result initSampleConverter(StreamState * ss);
Result sampleConvert(StreamState * ss);
Result closeSampleConverter(StreamState * ss);
