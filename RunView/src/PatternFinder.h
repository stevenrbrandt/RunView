// Anna Neshyba, this is the header file for PatternFinder, c++

#include <vector>
#include <string>

using namespace std; 

class PatternFinder {
public:
  PatternFinder(vector<int>, int patternLength); 
  vector<int> list;
  vector<int> patterns; 
  vector<int> patternTracker; 
  vector<int> repeats; 
  void findPatterns(int patternLength); 
  void findRepeatNums(); 
  void buildVector();
  void sort(); 
  vector<int> getPatterns();
  vector<int> getPatternTracker(); 
  vector<int> getRepeatNums(); 

};
