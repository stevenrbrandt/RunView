#include <stdio.h>
#include <cctk.h>
#include <cctk_Arguments.h>
#include <cctk_Parameters.h>
#include <cctk_Schedule.h>
#include <assert.h>
#include <vector>
#include <map>
#include <string>

using namespace std;

class RV_Timer_Node;

typedef vector<string> Strings;

class RV_Timer_Node {
public:
  RV_Timer_Node(string node_name)
  {
    time_desc_s = 0;
    time_node_set = true;
    name = node_name;
    level = 0;
  }
  RV_Timer_Node()
  {
    level = -1;
    time_desc_s = 0;
    time_node_set = false;
  }
  void insert(Strings& path, int levelp, double time)
    {
      if ( name.size() == 0 ) { level = levelp; name = path[level-1]; }
      else if ( level ) assert( name == path[level-1] );

      if ( path.size() == level )
        {
          assert( !time_node_set );
          time_node_set = true;
          time_node_s = time;
          return;
        }

      if ( path.size() == level + 1 ) time_desc_s += time;
      children[path[level]].insert(path,level+1,time);
    }
  double time_node_s;
  double time_desc_s;
  string name;
  int level;
  bool time_node_set;
  map<string,RV_Timer_Node> children;
};

vector<string>
split(const char *cs, char sep)
{
  vector<string> rv;
  rv.push_back(string(""));
  while ( const char c = *cs++ )
    {
      if ( c == sep ) { rv.push_back(string(""));  continue; }
      rv.back() += c;
    }
  return rv;
}


CCTK_INT
RunView_init()
{
  DECLARE_CCTK_PARAMETERS;
  printf("Hello from init.\n");

  return 0;
}

CCTK_INT
RunView_atend()
{
  DECLARE_CCTK_PARAMETERS;

  const bool show_all_timers = false;

  const int ntimers = CCTK_NumTimers();
  cTimerData* const td = CCTK_TimerCreateData();

  if ( show_all_timers )
    printf("Found %d timers.\n", ntimers);

  RV_Timer_Node timer_tree("root");

  for ( int i=0; i<ntimers; i++ )
    {
      CCTK_TimerI(i, td);
      const char *name = CCTK_TimerName(i);
      assert ( td );
      const cTimerVal* const tv = CCTK_GetClockValueI(0, td);
      const double timer_secs = CCTK_TimerClockSeconds(tv);
      if ( show_all_timers ) printf("%10.6f %s\n",timer_secs,name);

      Strings pieces = split(name,'/');
      if ( pieces[0] != "main" ) continue;
      timer_tree.insert(pieces,0,timer_secs);
    }

  CCTK_TimerDestroyData(td);

  vector<RV_Timer_Node*> stack;
  stack.push_back(&timer_tree);
  while ( stack.size() )
    {
      RV_Timer_Node* const nd = stack.back();  stack.pop_back();

      printf
        ("%10.6f %10.6f %s %s\n",
         nd->time_node_s, nd->time_desc_s,
         string(nd->level*2,' ').c_str(), nd->name.c_str());

      for ( auto& ch: nd->children ) stack.push_back(&ch.second);
    }

  return 0;
}
