#include "twitch.h"

char gamesTop[] = "https://api.twitch.tv/kraken/games/top";
char gameStreams[] = "https://api.twitch.tv/kraken/search/streams?q=%s";
char token[] = "http://api.twitch.tv/api/channels/%s/access_token";
char m3u8[] = "http://usher.twitch.tv/api/channel/hls/%s.m3u8?player=twitchweb&token=%s&sig=%s";

game_page getGameList(int page){
  char **output = malloc(sizeof (char*));
  int output_size = 0;
  json_value *    json;
  game_page gp;
  int i = 0;
  Result res;

  res = http_request(gamesTop, (u8 **)output, &output_size);
  if(res != 0){
    printf("Error getting game list");
    return gp; // Couldn't open file
  }

  json = json_parse(*output, output_size);
  // gamearray = response["top"=2]
  json_value * gamearray = json->u.object.values[2].value;
  json_value * gameobject;
  for(i = 0; i < gamearray->u.array.length; i++){
    // gameobject = gamearray[i]
    gameobject = gamearray->u.array.values[i]->u.object.values[0].value;
    // name = gameobject["name"=0]
    strcpy(gp.g[i].name, gameobject->u.object.values[0].value->u.string.ptr);
  }

  json_value_free(json);
  free(*output);
  free(output);

  return gp;
}

game_stream_page getGameStreams(char * name){
  char * url;
  char * urlencoded;
  char ** output = malloc(sizeof (char*));
  int output_size = 0;
  json_value * json;
  game_stream_page gsp;
  Result res;
  int i = 0;

  urlencoded = url_encode(name);
  asprintf(&url, gameStreams, urlencoded);
  free(urlencoded);

  res = http_request(url, (u8 **)output, &output_size);
  free(url);
  //printf("http_download_result: %ld\n", );
  if(res != 0){
    printf("Error getting channel list\n");
    return gsp; // Couldn't open file
  }

  json = json_parse(*output, output_size);
  // streamarray = response["top"=2]
  json_value * streamsarray = json_object_find_value(json, "streams");
  json_value * gameobject, *channelobject;
  for(i = 0; i < streamsarray->u.array.length; i++){
    // gameobject = streamarray[i]
    gameobject = streamsarray->u.array.values[i];
    channelobject = json_object_find_value(gameobject, "channel");
    // name = gameobject["name"=0]
    strcpy(gsp.s[i].name, json_object_find_value(channelobject, "name")->u.string.ptr);
  }

  json_value_free(json);

  free(*output);
  free(output);

  return gsp;
}

stream_sources getStreamSources(char * stream_name){
  char * url, *p, *urlencoded, *line;
  char **output = malloc(sizeof (char*));
  int output_size = 0;
  json_value *    json;
  stream_sources sr;
  Result res;

  // generate url for token
  asprintf(&url, token, stream_name);
  //get token response
  res = http_request(url, (u8 **)output, &output_size);
  free(url);
  //printf("http_download_result: %ld\n", );
  if(res != 0){
    printf("Error getting security token\n");
    return sr; // Couldn't open file
  }

  json = json_parse(*output, output_size);
  free(*output);

  for (p = stream_name; *p != '\0'; ++p) *p = tolower(*p);
  urlencoded = url_encode(json->u.object.values[0].value->u.string.ptr);
  asprintf(&url, m3u8, stream_name, urlencoded, json->u.object.values[1].value->u.string.ptr);
  free(urlencoded);

  json_value_free(json);

  res = http_request(url, (u8 **)output, &output_size);
  free(url);
  //printf("http_download_result: %ld\n", );
  if(res != 0){
    printf("Error getting m3u8 playlist\n");
    return sr; // Couldn't open file
  }

  p = *output;
  while(nextLine(&p, &line) != -1){
    if (strstr(line, "http://") != NULL){
      if(strstr(line, "source"))       strcpy(sr.source,line);
      else if(strstr(line, "high"))    strcpy(sr.high,line);
      else if(strstr(line, "medium"))  strcpy(sr.medium,line);
      else if(strstr(line, "low"))     strcpy(sr.low,line);
      else if(strstr(line, "mobile"))  strcpy(sr.mobile,line);
    }
  }

  free(*output);
  free(output);

  return sr;
}
