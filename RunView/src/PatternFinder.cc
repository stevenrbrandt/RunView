// Anna Neshyba, cc file for PatternFinder class, c++

#include<vector>
#include "PatternFinder.h"
#include <iostream>
#include <string.h>
#include <sstream>
#include <stdio.h>
#include <vector>
#include <stdlib.h>
#include <ctype.h>
#include <regex>
#include <algorithm>

using namespace std; 

// methods 
PatternFinder::PatternFinder(vector<int> v, int patternLength) {
  list = v; 
  buildVector(); 
  sort(); 
  findPatterns(patternLength); 
  findRepeatNums(); 
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
  
  printf("\n Repeats size: %d!", repeats.size()); 
}

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

void PatternFinder::buildVector() {
  for ( int i=0; i<list.size(); i++ ) patterns.push_back(i);


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

  const int n = (int)list.size();

  auto compare = [&](const int a, const int b)
    {
      if ( a == b ) return true;

      if ( a < b ) {

        for (int i = b; i < n; i++) {
          const int aidx = i - b + a;
          if ( list[aidx] == list[i] ) continue;
          return list[aidx] < list[i];
        }
        return true;

      } else {

        for (int i = a; i < n; i++) {
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
  printf("\n Patterns size: %d", patterns.size()); 
  
}

