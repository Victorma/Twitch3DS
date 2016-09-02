#pragma once

#include <3ds.h>

Result http_request(const char *url, u8 ** output, int * output_size);
