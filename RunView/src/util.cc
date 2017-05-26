#include <util.h>

using namespace std;

Strings
split(const char *cs, char sep)
{
  vector<string> rv;
  rv.push_back(string(""));
  while ( const char c = *cs++ )
    if ( c == sep ) rv.push_back(string("")); else rv.back() += c;

  return rv;
}
