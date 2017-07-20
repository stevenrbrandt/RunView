// Anna Neshyba, this is the header file for PatternFinder, c++

#include <vector>
#include <string>

using namespace std; 

class PatternFinder {
public:
  PatternFinder(vector<int>, int patternLength); 
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
