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
PatternFinder::PatternFinder(vector<int> v) {
  list = v; 
  buildVector(); 
  sort(); 
}

vector<int> PatternFinder::getPatterns() {
  return patterns; 
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

  int vec1  = patterns[n-1];
  int vec2 = patterns[n-2];
  vector<int> all = {vec2, vec1, patterns[n-3], patterns[n-5]}; 

  // vec1 and vec2 size = 0?!

  printf("--------BEFORE SORT-------- \n"); 

  // this prints out nothing...
  for (int i = 0; i < all.size(); i++) {
    for (int j = all.at(i); j < n; j++) {
      printf(" %d ", list[j] );
    }
    printf("\n"); 
  }

  printf("\n"); 
  std::sort(all.begin(), all.end(), compare);
 
  printf("\n--------AFTER SORT-------- \n"); 
  
  // this prints out nothing...
  for (int i = 0; i < all.size(); i++) {
    for (int j = all.at(i); j < n; j++) {
      printf(" %d ", list[j] );
    }
    printf("\n"); 
  }

  // std::sort(patterns.begin(), patterns.end(), compare);


}
