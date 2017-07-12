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

vector<vector<int>*> PatternFinder::getPatterns() {
  return patterns; 
}

void PatternFinder::buildVector() {
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
}

void PatternFinder::sort() {
  auto compare = [&](const vector<int>* a, const vector<int>* b)
    {
      int comp = 1; 
      if (a->size() == b->size()) {
	for (int i = 0; i < a->size(); i++) {
	  if (a->at(i) < b->at(i)) {
	    comp = 1; 
	    break; 
	  } else if (b->at(i) < a->at(i)) {
	    comp = 0;
	    break; 
	  }
	}
      } else {
	comp = 1; 
	if (a->size() < b->size()) {
	  for (int i = 0; i < a->size(); i++) {
	    if (a->at(i) < b->at(i)) {
	      break; 
	    } else if (b->at(i) < a->at(i)) {
	      comp = 0;
	      break; 
	    } else if (++i == a->size()) {
	      comp = 1; 
	    }
	  } 
	} else {
	  for (int i = 0; i < b->size(); i++) {
	    if (a->at(i) < b->at(i)) {
	      break; 
	    } else if (b->at(i) < a->at(i)) {
	      comp = 0;
	      break; 
	    } else if (++i == b->size()) {
	      comp = 0; 
	    }
	  }
	}
      }
      return comp;
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

  vector<int>* vec1  = patterns.back(); 
  patterns.pop_back(); 
  vector<int>* vec2 = patterns.back();
  vector<vector<int>*> all = {vec2, vec1}; 

  // vec1 and vec2 size = 0?!

  printf("--------BEFORE SORT-------- \n"); 

  // this prints out nothing...
  for (int i = 0; i < all.size(); i++) {
    for (int j = 0; j < all.at(i)->size(); j++) {
      printf(" %d ", all.at(i)->at(j));
    }
    printf("\n"); 
  }

  printf("\n"); 
  std::sort(all.begin(), all.end(), compare);
 
  printf("\n--------AFTER SORT-------- \n"); 
  
  // this prints out nothing...
  for (int i = 0; i < all.size(); i++) {
    for (int j = 0; j < all.at(i)->size(); j++) {
      printf(" %d ", all.at(i)->at(j));
    }
    printf("\n"); 
  }
  
  // std::sort(patterns.begin(), patterns.end(), compare);
  

}
