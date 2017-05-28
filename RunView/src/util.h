#ifndef RUNVIEW_UTIL_H
#define RUNVIEW_UTIL_H
#include <stdio.h>
#include <vector>
#include <string>
#include <time.h>

typedef std::vector<std::string> Strings;

Strings split(const char *cs, char sep);

std::string escapeForXML(const std::string&& s);

inline double
time_wall_fp()
{
  struct timespec now;
  clock_gettime(CLOCK_REALTIME,&now);
  return now.tv_sec + ((double)now.tv_nsec) * 1e-9;
}

#endif
