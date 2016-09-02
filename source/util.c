#include "util.h"

#include <string.h>

int nextLine(char ** s, char ** l)
{
  char * nextLine = NULL;

  if(*s == NULL || **s == '\0')
    return -1;

  *l = *s;

  nextLine = strchr(*s, '\n');
  if (nextLine){
    *nextLine = '\0';
    *s = nextLine+1;
  }else{
    nextLine = strchr(*s, '\0');
    *s = nextLine;
  }

  return 1;
}

unsigned int next_pow2(unsigned int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    return v + 1;
}
