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

string
escapeForXML(const string&& s)
{
  // This function based on a namesake in carpet/Timers/src/TimerTree.cc.
  // See that file for copyright and license.
  string rv;
  for ( char c: s )
    switch ( c ) {
    case '\'': rv += "&apos;"; break;
    case '"':  rv += "&quot;"; break;
    case '&':  rv += "&amp;"; break;
    case '<':  rv += "&lt;"; break;
    case '>':  rv += "&gt;"; break;
    default:   rv += c; break; }
  return rv;
}
