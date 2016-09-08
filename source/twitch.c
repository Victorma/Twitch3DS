#include "twitch.h"

char gamesTop[] = "https://api.twitch.tv/kraken/games/top";
char gameStreams[] = "https://api.twitch.tv/kraken/search/streams?q=%s";
char token[] = "http://api.twitch.tv/api/channels/%s/access_token";
char m3u8[] = "http://usher.twitch.tv/api/channel/hls/%s.m3u8?player=twitchweb&token=%s&sig=%s";

Result getGameList(game_page * gp, int page){
  char **output = malloc(sizeof (char*));
  int output_size = 0;
  json_value *    json;
  int i = 0;
  Result res;

  res = http_request(gamesTop, (u8 **)output, &output_size);
  if(res != 0){
    printf("Error getting game list");
    return -1; // Couldn't open file
  }

  json = json_parse(*output, output_size);
  // gamearray = response["top"=2]
  json_value * gamearray = json->u.object.values[2].value;
  json_value * gameobject;
  for(i = 0; i < gamearray->u.array.length; i++){
    // gameobject = gamearray[i]
    gameobject = gamearray->u.array.values[i]->u.object.values[0].value;
    // name = gameobject["name"=0]
    strcpy(gp->g[i].name, gameobject->u.object.values[0].value->u.string.ptr);
  }

  json_value_free(json);
  free(*output);
  free(output);

  return 0;
}

Result getGameStreams(game_stream_page * gsp, char * name){
  char * url, *ptr, * urlencoded;
  char ** output = malloc(sizeof (char*));
  int output_size = 0;
  json_value * json;
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
    return -1; // Couldn't open file
  }

  json = json_parse(*output, output_size);
  // streamarray = response["top"=2]
  json_value * streamsarray = json_object_find_value(json, "streams");
  if(streamsarray != NULL){
    json_value * gameobject, *channelobject, *name;
      for(i = 0; i < streamsarray->u.array.length; i++){
        // gameobject = streamarray[i]
        gameobject = streamsarray->u.array.values[i];
        if(gameobject != NULL){
          channelobject = json_object_find_value(gameobject, "channel");
          if(channelobject != NULL){
          // name = gameobject["name"=0]
            name = json_object_find_value(channelobject, "name");
            if(name != NULL) strcpy(gsp->s[i].name, name->u.string.ptr);
            else return -1;
          }else return -1;
        }else return -1;
      }
  }else return -1;

  json_value_free(json);

  free(*output);
  free(output);

  return 0;
}

Result getStreamSources(stream_sources * ss, char * stream_name){
  char * url, *p, *urlencoded, *line;
  char **output = malloc(sizeof (char*));
  int output_size = 0;
  json_value *    json;
  Result res;

  // generate url for token
  asprintf(&url, token, stream_name);
  //get token response
  res = http_request(url, (u8 **)output, &output_size);
  free(url);
  //printf("http_download_result: %ld\n", );
  if(res != 0){
    printf("Error getting security token\n");
    return -1; // Couldn't open file
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
    return -1; // Couldn't open file
  }

  p = *output;
  while(nextLine(&p, &line) != -1){
    if (strstr(line, "http://") != NULL){
      if(strstr(line, "source"))       strcpy(ss->source,line);
      else if(strstr(line, "high"))    strcpy(ss->high,line);
      else if(strstr(line, "medium"))  strcpy(ss->medium,line);
      else if(strstr(line, "low"))     strcpy(ss->low,line);
      else if(strstr(line, "mobile"))  strcpy(ss->mobile,line);
    }
  }

  free(*output);
  free(output);

  return 0;
}
