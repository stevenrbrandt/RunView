// Anna Neshyba, this is the header file for PatternFinder, c++

#ifndef PATTERNFINER_H
#define PATTERNFINER_H

#include <assert.h>
#include <vector>
#include <string>
#include "ctimers.h"

using namespace std; 

class PatternFinder {
public:
  PatternFinder
  (vector<int> event_indices, vector<RV_Timer_Event>& events,
   int patternLength);
  vector<RV_Timer_Event>& events;
  vector<int> list;
  int list_size;
  vector<int> patterns; 
  vector<int> patternTracker; 
  vector<int> repeats; 
  vector<int> instance_next;
  int pattern_len_limit;
  int pattern_start_pidx;
  int pattern_repeats;
  void findPatterns();
  void findRepeatNums(); 
  void buildVector();
  void sort(); 
  vector<int> getPatterns();
  vector<int> getPatternTracker(); 
  vector<int> getRepeatNums(); 
  vector<int> getBackPairs(size_t list_idx);

};

const int pattern_finder_tshift = 5;


inline int PatternFinderEncodeStart(int timer_idx, int level)
{
  assert( level < (1 << pattern_finder_tshift - 1 ) );
  return ( timer_idx << pattern_finder_tshift ) + ( level << 1 );
}
inline int PatternFinderEncodeStop(int timer_idx, int level)
{
  return ( timer_idx << pattern_finder_tshift ) + ( level << 1 ) + 1;
}

#endif
