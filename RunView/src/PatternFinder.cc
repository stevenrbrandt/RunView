// Anna Neshyba, cc file for PatternFinder class, c++

#include "PatternFinder.h"
#include <iostream>
#include <string.h>
#include <sstream>
#include <stdio.h>
#include <assert.h>
#include <vector>
#include <stdlib.h>
#include <ctype.h>
#include <regex>
#include <algorithm>

using namespace std; 

// methods 
PatternFinder::PatternFinder
(vector<int> v, vector<RV_Timer_Event>& eventsp, int patternLength):
  events(eventsp)
{
  pattern_len_limit = patternLength;
  list = v; 
  list_size = v.size();
  for ( int i=0; i<patternLength; i++ ) list.push_back(1<<30);
  buildVector(); 
  sort(); 
  findPatterns(); 
}

vector<int> PatternFinder::getPatterns() {
  return patterns; 
}

vector<int> PatternFinder::getPatternTracker() {
  return patternTracker;
}

vector<int> PatternFinder::getRepeatNums() {
  return repeats;
}

void PatternFinder::findRepeatNums() {
  int n = patternTracker.size(); 
  if (patternTracker[0] == 0) {repeats.push_back(1); }
  for (int i = 1; i < n; i++) {
    repeats.push_back(patternTracker[i] - patternTracker[i-1]); 
  }
  
  
  printf("\n repeats:  \n" ); 
  for (int i = 0; i < repeats.size(); i++) {
    printf("%d | %d \n", i, repeats[i]); 
    } 
  
  printf("\n Repeats size: %zd!", repeats.size()); 
}

void PatternFinder::findPatterns()
{
  double max_cover = 0;
  int max_cover_pidx = -1;
  int max_cover_len = -1;
  int max_repeats = -1;

  auto submatch = [&](int idx1, int idx2, int len )
    {
      for ( int i=0; i<len; i++ )
        if ( list[idx1+i] != list[idx2+i] ) return false;
      return true;
    };

  struct Run_Start {
    Run_Start(int pidxp, int lenp, bool overlapp):
      pidx(pidxp),len(lenp),overlap(overlapp){};
    int pidx, len;
    bool overlap;
  };

  vector<Run_Start> run_start;

  const int levmask = ( 1 << pattern_finder_tshift ) - 1;

  auto get_level = [&](int a) { return ( a & levmask ) >> 1 ; };
  auto is_start = [&](int a) { return ! ( a & 1 ); };
  auto get_duration = [&](int pidx, int plen)
    {
      const int idx = patterns[pidx];
      return events[idx+plen].etime_get() - events[idx].etime_get();
    };

  const double duration = events.back().etime_get() - events[0].etime_get();

  for ( int i=1; i<patterns.size(); i++ )
    {
      const int idx0 = patterns[i-1];
      const int idx1 = patterns[i];
      int prefix = 0;

      const int level_start = get_level(list[idx1]);

      if ( is_start(list[idx1] ) )
        for ( int j=0; j<pattern_len_limit; j++ )
          {
            const int elt0 = list[idx0+j];
            if ( elt0 != list[idx1+j] ) break;
            if ( is_start(elt0) ) continue;
            const int level = get_level(elt0);
            if ( level < level_start ) break;
            if ( level > level_start ) continue;
            prefix = j+1;
          }

      int nearest_pidx = i;

      while ( !run_start.empty() && run_start.back().len > prefix )
        {
          Run_Start done_run = run_start.back();
          run_start.pop_back();
          assert( done_run.pidx < nearest_pidx );
          nearest_pidx = done_run.pidx;

          const double duration_n = get_duration(done_run.pidx,done_run.len);
          const int repeats = i - done_run.pidx;
          const double cover = duration_n * repeats;

          assert( cover > 0 );
          if ( cover > max_cover && !done_run.overlap )
            {
              max_cover = cover;
              max_repeats = repeats;
              max_cover_pidx = done_run.pidx;
              max_cover_len = done_run.len;
              if ( true )
                printf("Max cover %7.3f%%, idx %d, len %d, rpt %d\n",
                       100.0 * max_cover / duration,
                       max_cover_pidx, max_cover_len,
                       max_repeats);
            }
        }

      if ( prefix > 0
           && ( run_start.empty() || run_start.back().len < prefix ) )
        {
          bool overlap = false;
          for ( int pl = 1; pl <= prefix/2 && !overlap; pl++ )
            overlap = submatch(idx1,idx1+prefix-pl,pl);
          run_start.emplace_back( nearest_pidx, prefix, overlap );
        }
    }

  pattern_start_pidx = max_cover_pidx;
  pattern_repeats = max_repeats;

  instance_next.resize(list_size,-1);

  vector<int> pidx_sorted;
  for ( int i=0; i<pattern_repeats; i++ )
    pidx_sorted.push_back(pattern_start_pidx+i);
  ::sort(pidx_sorted.begin(),pidx_sorted.end(),
       [&](int a, int b){ return patterns[a] < patterns[b]; } );

  for ( int i=0; i<pattern_repeats-1; i++ )
    {
      const int curr_st_lidx = patterns[pidx_sorted[i]];
      const int next_st_lidx = patterns[pidx_sorted[i+1]];
      assert( submatch( curr_st_lidx, next_st_lidx, max_cover_len ) );
      assert( next_st_lidx > curr_st_lidx );
      for ( int j=0; j<max_cover_len; j++ )
        {
          const int curr_lidx = curr_st_lidx + j;
          const int next_lidx = next_st_lidx + j;
          assert( curr_lidx < list_size );
          assert( next_lidx < list_size );
          assert( instance_next[ curr_lidx ] == -1 );
          instance_next[ curr_lidx ] = next_lidx;
        }
    }
}

vector<int>
PatternFinder::getBackPairs(size_t list_idx)
{
  int idx = list_idx;
  vector<int> back_pairs;
  assert( idx < list.size() );
  while ( idx >= 0 )
    {
      back_pairs.push_back( idx );
      idx = instance_next[idx];
    }
  return move(back_pairs);
}

#if 0
void PatternFinder::findPatterns(int patternLength) {
  int n = patterns.size(); 
  int tracker = 0; 
  int counter = 0;

  while (true) {
    int start = patterns[tracker]; 
    vector<int> currPat; 
    for (int i = start; i < start + patternLength; i++) {
      if (i > list.size() - 1) {break;}
      currPat.push_back(list[i]); 
    }

    while (true) {
      vector<int> comPat;
      for (int i = patterns[tracker+1]; i < patterns[tracker+1]+patternLength; i++) {
	if (i > list.size() - 1) {break;}
	comPat.push_back(list[i]); 
      }
      
      if (currPat == comPat) {
	tracker++; 
	if (tracker > n-1) {patternTracker.push_back(tracker); break;}
      } else { 
	patternTracker.push_back(tracker);
	tracker++;
	break; 
      }
    }
    if (tracker > n-1) break; 
  }
  
  
  printf("\n Pattern Tracker:  \n" ); 
  for (int i = 0; i < patternTracker.size(); i++) {
    printf("%d | %d \n", i, patternTracker[i]); 
  } 
  
  printf("\n Pattern Tracker size: %d \n", patternTracker.size()); 
  
    
}
#endif

void PatternFinder::buildVector() {
  for ( int i=0; i<list_size; i++ ) patterns.push_back(i);


#if 0

  // DMK: The code below doesn't work because "patterns" contains
  // *pointers* to list, so when the loop below ends the pointers all
  // point to the same thing, an empty list.  I've put another version
  // of the loop below that stores copies of the list, and so would work,
  // but would be very inefficient.
  vector<int>* elm = &list; 
  while (!elm->empty()) {
    patterns.push_back(elm); 
    // Checking to see if elm is being pushed correctly
    // It seems to be!
    /* 
    for (int i = 0; i < patterns.back()->size(); i++) {
      printf(" %d", patterns.back()->at(i)); 
    }
    printf("\n"); 
    */
    elm->pop_back(); 
  } 

  // DMK: This loop stores copies of "list" in "patterns" and so would
  // work correctly. But it would be slow because of the amount of copying
  // that needs to be done.  For this code to work patterns must
  // be declared type vector<vector<int> >.
  vector<int> elm = list;
  while (!elm.empty()) {
    patterns.push_back(elm); 
    elm.pop_back();
  }
  //
  // DMK: It would work except for one thing: To find repeating
  // patterns we need to sort the suffixes of list, but the code
  // immediately above stores prefixes.


#endif
}

void PatternFinder::sort() {

  auto compare = [&](const int a, const int b)
    {
      if ( a == b ) return true;

      if ( a < b ) {

        for (int i = b; i < b+pattern_len_limit; i++) {
          const int aidx = i - b + a;
          if ( list[aidx] == list[i] ) continue;
          return list[aidx] < list[i];
        }
        return true;

      } else {

        for (int i = a; i < a+pattern_len_limit; i++) {
          const int bidx = i - a + b;
          if ( list[i] < list[bidx] ) {
            return true;
          } else if ( list[bidx] < list[i] ) {
            return false;
          }
        }
        return false;

      }
      return false; // Unreachable
    };

  // Testing 
  
  /*  vector<int> c = {1,2,3,5};
  vector<int> e = {1,2,3};
  vector<int> d = {3,4,5}; 
  vector<int> f = {5, 1, 7}; 
  vector<int> g = {2,3,10};
  vector<int> h = {2,3,11}; 
  vector<vector<int>> all = {d,h,e,c,f,g}; 
  
  for (int i = 0; i < all.size(); i++) {
    for (int j = i+1; j < all.size(); j++) {
      int com = compare(all.at(i), all.at(j));
      printf("\n"); 
    }
  }

  */
  /*
  printf("--------BEFORE SORT-------- \n"); 
 
  for (int i = 0; i < patterns.size(); i++) {
    printf("%d | ", i); 
    for (int j = patterns.at(i); j < n; j++) {
      printf(" %d ", list[j] );
    }
    printf("\n"); 
  }
  */
  printf("\n"); 
  std::sort(patterns.begin(), patterns.end(), compare);
 
  /*
  printf("\n--------AFTER SORT-------- \n"); 
  
  for (int i = 0; i < patterns.size(); i++) {
    printf("%d | ", i); 
    for (int j = patterns.at(i); j < n; j++) {
      printf(" %d ", list[j] );
    }
    printf("\n"); 
  }
  */
  printf("\n Patterns size: %zd\n", patterns.size()); 
  
}

