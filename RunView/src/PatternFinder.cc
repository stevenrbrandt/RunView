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
PatternFinder::PatternFinder(vector<int> v, int patternLength) {
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
  vector<int> run_start(pattern_len_limit+1,0);
  vector<bool> run_overlap(pattern_len_limit+1,false);
  int max_cover = 0;
  int max_cover_pidx = -1;
  int max_cover_len = -1;
  int max_repeats = -1;

  auto submatch = [&](int idx1, int idx2, int len )
    {
      for ( int i=0; i<len; i++ )
        if ( list[idx1+i] != list[idx2+i] ) return false;
      return true;
    };

  for ( int i=1; i<patterns.size(); i++ )
    {
      const int idx1 = patterns[i];
      bool match_possible = true;
      for ( int j=1; j<=pattern_len_limit; j++ )
        {
          const int idx2 = patterns[run_start[j]];
          if ( !match_possible || !submatch(idx1, idx2, j) )
            {
              match_possible = false;
              const int repeats = i - run_start[j];
              const int cover = j * repeats;
              assert( cover > 0 );
              if ( cover > max_cover && !run_overlap[j] )
                {
                  max_cover = cover;
                  max_repeats = repeats;
                  max_cover_pidx = run_start[j];
                  max_cover_len = j;
                  if ( false )
                    printf("Max cover %d, idx %d, len %d, rpt %d\n",
                           max_cover, max_cover_pidx, max_cover_len,
                           max_repeats);
                }
              bool overlap = false;
              for ( int pl = 1; pl <= j/2 && !overlap; pl++ )
                overlap = submatch(idx1,idx1+j-pl,pl);
              run_start[j] = i;
              run_overlap[j] = overlap;
            }
        }
    }

  pattern_start_pidx = max_cover_pidx;
  pattern_repeats = max_repeats;

  instance_next.resize(list_size);
  for ( int i=0; i<list_size; i++ ) instance_next[i] = i;

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
          assert( instance_next[ curr_lidx ] == curr_lidx );
          instance_next[ curr_lidx ] = next_lidx;
        }
    }
}

vector<int>
PatternFinder::getBackPairs(size_t list_idx)
{
  vector<int> back_pairs;
  assert( list_idx < list.size() );
  while ( true )
    {
      const int idx = instance_next[list_idx];
      back_pairs.push_back( idx );
      if ( idx == list_idx ) break;
      if ( instance_next[list_idx-1] == list_idx-1 ) break;
      list_idx = idx;
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

