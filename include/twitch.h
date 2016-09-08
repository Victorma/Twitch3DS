#pragma once

typedef struct {
	char name[1000];
} game;

typedef struct{
	char next[1000];
	game g[10];
} game_page;

typedef struct {
	char name[1000];
} game_stream;

typedef struct {
	char next[1000];
	game_stream s[10];
} game_stream_page;

typedef struct {
	char source[1000], high[1000], medium[1000], low[1000], mobile[1000];
} stream_sources;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "json.h"
#include "http.h"
#include "urlcode.h"
#include "util.h"

/*
const int limit = 10;
const int page_offset = 10;*/


Result getGameList(game_page * gp, int page);
Result getGameStreams(game_stream_page * gsp, char * name);
Result getStreamSources(stream_sources * ss, char * stream_name);
