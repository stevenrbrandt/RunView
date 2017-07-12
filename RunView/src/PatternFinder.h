// Anna Neshyba, this is the header file for PatternFinder, c++

#include <vector>
#include <string>

using namespace std; 

class PatternFinder {
public:
  PatternFinder(vector<int>); 
  vector<int> list;
  vector<vector<int>*> patterns; 
  vector<vector<int>*> getPatterns(); 
  void buildVector();
  void sort(); 

};
