#include <stdio.h>
#include <cctk.h>
#include <cctk_Arguments.h>
#include <cctk_Parameters.h>
#include <cctk_Schedule.h>
#include <assert.h>
#include <vector>
#include <map>
#include <string>
#include <errno.h>
#include <string.h>
#include "util.h"

using namespace std;

class RV_Timer_Node {
public:
  RV_Timer_Node(string node_name)
  {
    dur_node_s = dur_kids_s = 0;
    dur_node_set = true;
    name = node_name;
    level = 0;
  }
  RV_Timer_Node()
  {
    dur_kids_s = 0;
    dur_node_set = false;
    level = -1;
  }
  void insert(Strings& path, int levelp, double time)
    {
      if ( name.size() == 0 ) { level = levelp; name = path[level-1]; }
      else if ( level ) assert( name == path[level-1] );

      if ( path.size() == level )
        {
          assert( !dur_node_set );
          dur_node_set = true;
          dur_node_s = time;
          return;
        }

      if ( path.size() == level + 1 ) dur_kids_s += time;
      children[path[level]].insert(path,level+1,time);
    }

  double dur_node_s;  // Duration recorded by Cactus timer.
  double dur_kids_s;  // Sum of durations of immediate children.

  double pseudo_time_start;  // Start time, set for a particular image.

  string name;
  int level;
  bool dur_node_set;
  map<string,RV_Timer_Node> children;
};

class RV_Data {
public:
  RV_Data():timer_tree("root"){};
  int atend();
  void generate_text_tree();
  void generate_graph_simple();
  RV_Timer_Node timer_tree;
};

RV_Data rv_data;




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
  return rv_data.atend();
}



int
RV_Data::atend()
{
  const bool show_all_timers = false;

  const int ntimers = CCTK_NumTimers();
  cTimerData* const td = CCTK_TimerCreateData();

  if ( show_all_timers )
    printf("Found %d timers.\n", ntimers);

  for ( int i=0; i<ntimers; i++ )
    {
      const char *name = CCTK_TimerName(i);
      CCTK_TimerI(i, td);
      const cTimerVal* const tv = CCTK_GetClockValueI(0, td);
      const double timer_secs = CCTK_TimerClockSeconds(tv);
      if ( show_all_timers ) printf("%10.6f %s\n",timer_secs,name);

      Strings pieces = split(name,'/');
      if ( pieces[0] != "main" ) continue;
      timer_tree.insert(pieces,0,timer_secs);
    }

  CCTK_TimerDestroyData(td);

  generate_text_tree();
  generate_graph_simple();

  return 0;
}


void
RV_Data::generate_text_tree()
{
  vector<RV_Timer_Node*> stack;
  stack.push_back(&timer_tree);
  while ( stack.size() )
    {
      RV_Timer_Node* const nd = stack.back();  stack.pop_back();

      printf
        ("%10.6f %10.6f %s %s\n",
         nd->dur_node_s, nd->dur_kids_s,
         string(nd->level*2,' ').c_str(), nd->name.c_str());

      for ( auto& ch: nd->children ) stack.push_back(&ch.second);
    }
}

void
RV_Data::generate_graph_simple()
{
  DECLARE_CCTK_PARAMETERS;
  string svg_file_path = string(out_dir) + "/run-view.svg";
  FILE* const fh = fopen(svg_file_path.c_str(), "w");

  if ( !fh )
    {
      CCTK_VWarn
        (CCTK_WARN_ALERT, __LINE__,__FILE__, CCTK_THORNSTRING,
         "Could not open RunView output file \"%s\" for output: %s\n",
         svg_file_path.c_str(), strerror(errno));
      return;
    }

  CCTK_VInfo
    (CCTK_THORNSTRING,
     "Writing RunView plot to file \"%s\"\n", svg_file_path.c_str());

  const double font_size = 10;  // All units in points.
  const double image_wpt = 800;
  const double image_hpt = 600;

  const double plot_area_left_xpt = 5;
  const double plot_area_top_ypt = 5;
  const double plot_area_wpt = image_wpt - 2 * plot_area_left_xpt;
  const double plot_area_hpt = image_hpt - 2 * plot_area_top_ypt;

  fprintf(fh,"%s",
          "<?xml version=\"1.0\" standalone=\"no\"?>\n"
          "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n"
          "  \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
  fprintf(fh,"<svg width=\"%.3fpt\" height=\"%.3fpt\"\n"
          "viewBox=\"0 0 %.3f %.3f\"\n"
          "version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\n",
          image_wpt, image_hpt, image_wpt, image_hpt);

  fprintf(fh,"%s\n","<desc>Created by RunView</desc>");
  fprintf(fh,"<g font-size=\"%.3f\" font-family=\"sans-serif\">\n",
          font_size);

  auto rect = [&](double x, double y, double w, double h)
    {
      fprintf(fh, "<rect fill=\"none\" stroke=\"black\" "
              "x=\"%.3f\" y=\"%.3f\" width=\"%.3f\" height=\"%.3f\" />\n",
              x, y, w, h);
    };

  rect( plot_area_left_xpt, plot_area_top_ypt, plot_area_wpt, plot_area_hpt);

  double max_time = 0;
  int max_level = 0;

  vector<RV_Timer_Node*> stack;
  timer_tree.pseudo_time_start = 0;
  stack.push_back(&timer_tree);
  while ( stack.size() )
    {
      RV_Timer_Node* const nd = stack.back();  stack.pop_back();

      double pseudo_time_start = nd->pseudo_time_start;

      printf
        ("%10.6f %10.6f %10.6f %s %s\n",
         nd->dur_node_s, nd->dur_kids_s, pseudo_time_start,
         string(nd->level*2,' ').c_str(), nd->name.c_str());

      for ( auto& pair: nd->children )
        {
          RV_Timer_Node* const ch = &pair.second;
          ch->pseudo_time_start = pseudo_time_start;
          pseudo_time_start += ch->dur_node_s;
          stack.push_back(ch);
        }
      if ( pseudo_time_start > max_time ) max_time = pseudo_time_start;
      if ( nd->level > max_level ) max_level = nd->level;
    }

  const double scale_y = plot_area_hpt / max_time;
  const double scale_x = plot_area_wpt / ( max_level + 1 );
  const int width_char = scale_x / font_size;  // Approximate width.

  stack.push_back(&timer_tree);
  while ( stack.size() )
    {
      RV_Timer_Node* const nd = stack.back();  stack.pop_back();

      const double ht = nd->dur_node_s * scale_y;

      rect( nd->level * scale_x, nd->pseudo_time_start * scale_y, scale_x, ht );

      // WARNING: Text in nd->name should be escaped before (for <> and
      // possibly other characters).  Also, the condition below is too
      // strict, if the name is too long then a shortened version can
      // be prepared. Or it can be rotated.
      if ( nd->name.size() <= width_char && ht >= font_size * 1.2 )
        fprintf(fh, "<text x=\"%.3f\" y=\"%.3f\">%s</text>\n",
                font_size + nd->level * scale_x,
                font_size + nd->pseudo_time_start * scale_y,
                nd->name.c_str());

      for ( auto& pair: nd->children ) stack.push_back(&pair.second);
    }

  fprintf(fh,"%s","</g></svg>\n");
  fclose(fh);
}
