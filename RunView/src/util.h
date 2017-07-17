#ifndef RUNVIEW_UTIL_H
#define RUNVIEW_UTIL_H
#include <stdio.h>
#include <vector>
#include <string>
#include <time.h>

template<typename T> bool
set_min(T& lhs, T rhs)
{ if ( rhs < lhs ) { lhs = rhs; return true; } return false; }
template<typename T1, typename T2> bool
set_max(T1& lhs, T2 rhs)
{ if ( rhs > lhs ) { lhs = rhs; return true; } return false; }

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

inline int64_t
time_wall_ns()
{
  struct timespec now;
  clock_gettime(CLOCK_REALTIME,&now);
  return 1000000000 * int64_t(now.tv_sec) + now.tv_nsec;
}

#endif
