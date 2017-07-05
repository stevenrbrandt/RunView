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

using namespace std; 

// methods 
PatternFinder::PatternFinder(string str) {
  els = str; 
  idx = 0; 
}

string PatternFinder::getNext() {
  std::ostringstream num;

  while (!(isdigit(els.at(idx)))) {
    idx++;
  }

  while (els.at(idx) != ';') {
    num << els.at(idx); 
    idx++; 
  }
  
  idx = idx + 2; 

  return num.str(); 
}

void PatternFinder::clean() {
  string s,c; 

  for (int i = 0; i < patterns.size(); i++ ) {
    s = patterns.at(i); 
    for (int j = i+1; j < patterns.size(); j++) {
      c = patterns.at(j);
      if (s.size() > c.size()) {
	std::size_t found = s.find(c);
	if (found != string::npos) {
	  patterns.erase(patterns.begin() + j);
	}
      } else if (s.size() < c.size()) {
	std:: size_t found = c.find(s);
	if (found != string::npos) {
	  patterns.erase(patterns.begin() + i);
	}
      } else {
	if (s == c) {
	  patterns.erase(patterns.begin() + i); 
	}
      }
    }
  }
}

void PatternFinder::findPatterns() {
  std::stringstream currPat;  
  string check; 
  currPat << ";;";
  check = currPat.str(); 
  int counter, sidx; 
  vector<string> pats;
  sidx = 0;

  while (true) {
    // vector<size_t> positions;
    currPat << getNext() + ";;"; 
    check = currPat.str(); 
    
    std:: size_t pos = els.find(currPat.str(), sidx);
    // printf("%s : %d \n", currPat.str().c_str(), pos); 
    
    if ((pos == string::npos) || (idx == pos || idx > pos)) {
      bool exists = false; 
      for (auto& s : pats) { 
	if (currPat.str().compare(s) == 0) {
	  exists = true;
	} 
      } 

      if (!exists) {pats.push_back(currPat.str()); }
 
      currPat.str(""); 
      currPat << ";;";  
      sidx = idx; 
    }

    if (idx == els.length()) {break;}
  }   

  patterns = pats; 
  clean(); 

  for (auto& s : patterns) {
    printf("%s \n", s.c_str()); 
  }

} 


