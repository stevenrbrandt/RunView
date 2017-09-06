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
#include "ctimers.h"
#include "util.h"
#include "rv_papi.h"

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
  void insert(Strings& path, int levelp, double time, const RV_PAPI_Sample &ps)
    {
      // Add node at LEVELP for timer described by PATH with duration
      // TIME.
      //
      // Example:
      //
      //   Consider two timers: main/a and main/a/b
      //
      //   Calling root.insert( {main,a}, 0, 10.1 )
      //     Adds nodes:
      //      name: main, level: 0, dur_node_s: not set, dur_kid_s: 10.1
      //      name: a,    level: 1, dur_node_s: 10.1
      //
      //   Calling root.insert( {main,a,b}, 0, 20.2 )
      //     Adds new node:
      //      name: b,    level: 2, dur_node_s: 20.2
      //     And updates exiting node:
      //      name: a,    level: 1, dur_kid_s: 20.2

      if ( name.size() == 0 )
        {
          // This is the first time this node has been visited.
          // Initialize the level and name components.
          level = levelp;
          name = path[level-1];
        }
      else if ( level )
        {
          // This node has been visited before, make sure the names match.
          assert( name == path[level-1] );
        }

      // Check if this is the leaf node for the timer. If so,
      // record the time and end recursion.
      //
      if ( path.size() == level )
        {
          assert( !dur_node_set );
          dur_node_set = true;
          dur_node_s = time;
          papi_node = ps;
          return;
        }

      // Check if this is the parent of the leaf node of the timer.
      // If so, collect time of children.
      //
      if ( path.size() == level + 1 )
        {
          dur_kids_s += time;
          papi_kids += ps;
        }

      // Insert a node for next component.
      //
      children[path[level]].insert(path,level+1,time,ps);
    }

  double dur_node_s;  // Duration recorded by Cactus timer.
  double dur_kids_s;  // Sum of durations of immediate children.

  RV_PAPI_Sample papi_node;
  RV_PAPI_Sample papi_kids;

  double pseudo_time_start;  // Start time, set for a particular image.

  // Component of timer name. For example, for timer
  // main/Evolve/PrintTimers there would be three nodes, named main,
  // Evolve, and PrintTimers, at levels 1, 2, and 3, respectively.
  string name;
  int level;

  bool dur_node_set;  // If true, dur_node_s set to some value.
  map<string,RV_Timer_Node> children;
};

class RV_Data {
public:
  RV_Data():timer_tree("root"){};
  void init();
  int atend();
  void generate_text_tree();
  void generate_graph_simple();
  void generate_timeline_simple();
  RV_Timer_Node timer_tree;
};

RV_Data rv_data;
RV_CTimers rv_ctimers;



CCTK_INT
RunView_init()
{
  // Initialize the collection of timer event data
  //
  rv_data.init();

  return 0;
}

CCTK_INT
RunView_atend()
{

  // Print runview timer data.
  //
  return rv_data.atend();
}

void
RV_Data::init()
{
  // Initialize the collection of timer event data
  //
  rv_ctimers.init();

  // Initialize collection of CPU performance counter data.
  //
#ifdef HAVE_PAPI
  rv_ctimers.papi_init
    ( { PAPI_TOT_INS, PAPI_L3_TCM, PAPI_RES_STL,
        PAPI_STL_CCY, PAPI_FUL_CCY } );
#endif
}

int
RV_Data::atend()
{
  /// Print Timer Data

  DECLARE_CCTK_PARAMETERS;

  // If true, show lots of data.  Intended for debugging and
  // familiarization.
  //
  const bool show_all_timers = true;

  string file_path = string(out_dir) + "/run-view-all-timers.txt";
  FILE* const fh = show_all_timers ? fopen(file_path.c_str(),"w") : NULL;

  const int ntimers = CCTK_NumTimers();
  cTimerData* const td = CCTK_TimerCreateData();

  // Note: Each timer has multiple clocks. In most cases a clock
  // measures time and so only one would be necessary.
  const int nclocks = CCTK_NumClocks();

  if ( show_all_timers )
    {
      fprintf(fh,"Found %d timers.\n", ntimers);
      fprintf(fh,"Found %zd events.\n", rv_ctimers.events.size());
      fprintf(fh,"Allocated %d papi structures.\n",rv_ctimers.n_papi_samples);
      for ( int i=0; i<nclocks; i++ )
        fprintf(fh,"Clock %d: %s\n",i,CCTK_ClockName(i));
    }

  // Organize timer data into a tree, timer_tree.
  //
  for ( int i=0; i<ntimers; i++ )
    {
      const char *name = CCTK_TimerName(i);

      // Get RunView-collected data for timer i.
      //
      const RV_CTimer& t = rv_ctimers.get(i);

      if ( show_all_timers )
        {
          // Print out information about each timer.
          // Intended for debugging and familiarization.
          //

          // Copy info for timer i into td.
          CCTK_TimerI(i, td);

          // Extract the timer value for our preferred clock, "cycle",
          // or clock zero if cycle isn't found.
          //
          const cTimerVal* const tv =
            CCTK_GetClockValue("cycle", td) ?: CCTK_GetClockValueI(0, td);
          const double timer_secs = CCTK_TimerClockSeconds(tv);

          // Compare duration of the RunView clock with the clock used
          // above.
          const double delta = fabs(timer_secs - t.duration_s) * 1e6;
          fprintf(fh,"%10.6f %3.0f µs %s %3d %10lld %s\n",
                  timer_secs, delta, tv->heading, t.missequence_count,
                  t.papi_sample[PAPI_TOT_INS],
                  name);
        }

      //
      // Check whether timer is a tree timer. A timer is considered a
      // tree timer if its name is of the form
      // main/PARTA/PARTB/.../LEAFPAR, where PARTA, etc., consist of
      // alphanumeric characters.
      //
      // For example:
      //
      //  Timer main/CarpetStartup/CheckRegions is a tree timer, its
      //   leaf name is CheckRegions.
      //
      //  Timer [0017] SymBase: SymBase_Startup in CCTK_STARTUP is not
      //  a tree timer.
      //
      // This code assumes that the start and stop events for tree
      // timers properly nest and do so as suggested by their
      // names. For example, consider timers main/a, main/a/b, and
      // main/a/c. The following is a proper nesting: start-main/a,
      // start-main/a/b stop-main/a/b, start-main/a/c, stop-main/a/c,
      // stop-main/a. The following *is not* a proper nesting:
      // start-main/a, start-main/a/b stop-main/a stop-main/a/b,
      // start-main/a/c, stop-main/a/c; it's improper because
      // stop-main/a occurred while main-a/b was still running.
      //
      Strings pieces = split(name,'/');
      if ( pieces[0] != "main" ) continue;
      timer_tree.insert(pieces,0,t.duration_s,t.papi_sample);
    }

  CCTK_TimerDestroyData(td);

  if ( fh ) fclose(fh);

  // Write out text and graphical representations of timer trees.
  //
  generate_text_tree();
  generate_graph_simple();
  generate_timeline_simple();

  return 0;
}


void
RV_Data::generate_text_tree()
{
  DECLARE_CCTK_PARAMETERS;

  // Write out a text representation of timer tree.

  string file_path = string(out_dir) + "/run-view-timer-tree.txt";
  FILE* const fh = fopen(file_path.c_str(),"w");
  assert(fh);

  // Put root of tree on stack.
  vector<RV_Timer_Node*> stack = { &timer_tree };

  while ( stack.size() )
    {
      RV_Timer_Node* const nd = stack.back();  stack.pop_back();

      fprintf
        (fh, "%10.6f %10.6f %10lld %s %s\n",
         nd->dur_node_s, nd->dur_kids_s, nd->papi_node[PAPI_TOT_INS],
         string(nd->level*2,' ').c_str(), nd->name.c_str());

      for ( auto& ch: nd->children ) stack.push_back(&ch.second);
    }

  fclose(fh);
}

void
RV_Data::generate_graph_simple()
{
  // Write out SVG image showing timer values.

  // Declare Cactus parameters. (Needed for out_dir.)
  DECLARE_CCTK_PARAMETERS;

  string svg_file_path = string(out_dir) + "/run-view.svg";
  //
  // Note: out_dir is directory in which to write output for this
  // Cactus run.

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
  //
  // Note: This is the correct way to print messages in Cactus.

  const double font_size = 10;  // All units in points.

  // Dimension of entire SVG graphic.
  //
  const double image_wpt = 800;
  const double image_hpt = 600;

  // Compute dimensions of plot area.
  //
  const double plot_area_left_xpt = 5;
  const double plot_area_top_ypt = 5;
  const double plot_area_wpt = image_wpt - 2 * plot_area_left_xpt;
  const double plot_area_hpt = image_hpt - 2 * plot_area_top_ypt;

  // Write SVG Header
  //
  fprintf(fh,"%s",R"~~(<?xml version="1.0" standalone="no"?>
<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN"
  "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">
)~~");

  // Set SVG so that one user unit is one point. This is assuming that
  // user has adjusted font rendering so that a ten-point font is the
  // smallest size that's comfortably readable for substantial amounts
  // of text.
  //
  fprintf(fh,R"~~(<svg width="%.3fpt" height="%.3fpt"
          viewBox="0 0 %.3f %.3f"
          version="1.1" xmlns="http://www.w3.org/2000/svg">
)~~",
          image_wpt, image_hpt, image_wpt, image_hpt);

  fprintf(fh,"%s\n","<desc>Created by RunView</desc>");
  fprintf(fh,R"--(<g font-size="%.3f" font-family="sans-serif">
)--",
          font_size);

  // Convenience function for emitting SVG rectangles.
  //
  auto rect = [&](double x, double y, double w, double h)
    { fprintf(fh, R"--(<rect fill="none" stroke="black"
              x="%.3f" y="%.3f" width="%.3f" height="%.3f" />)--",
              x, y, w, h); };

  // Emit a box around the entire plot area.
  //
  rect( plot_area_left_xpt, plot_area_top_ypt, plot_area_wpt, plot_area_hpt);

  double max_time = 0;
  int max_level = 0;


  //
  // Determine y-axis position, pseudo_time_start, for each node in
  // timer tree. The value for pseudo_time_start is computed assuming
  // that each timer has one start/stop pair with dur_node_s the
  // duration from the start to the stop.  Also determine the height
  // of the tree.
  //

  vector<RV_Timer_Node*> stack = { &timer_tree };
  timer_tree.pseudo_time_start = 0;

  while ( stack.size() )
    {
      RV_Timer_Node* const nd = stack.back();  stack.pop_back();

      // Get this node's pseudo_time_start, which was set by its parent.
      //
      double pseudo_time_start = nd->pseudo_time_start;

      if ( false )
        printf
          ("%10.6f %10.6f %10.6f %s %s\n",
           nd->dur_node_s, nd->dur_kids_s, pseudo_time_start,
           string(nd->level*2,' ').c_str(), nd->name.c_str());

      // Set the pseudo_time_start members of this node's children.
      //
      for ( auto& pair: nd->children )
        {
          RV_Timer_Node* const ch = &pair.second;
          ch->pseudo_time_start = pseudo_time_start;

          // Push ch on the stack so pseudo_time_start can be set for
          // its children.
          //
          stack.push_back(ch);

          // Get time for the next sibling of ch.
          //
          pseudo_time_start += ch->dur_node_s;
        }

      set_max( max_time, pseudo_time_start );
      set_max( max_level, nd->level );
    }

  // Compute scale factors
  //
  // Seconds to points.
  const double s_to_pt = plot_area_hpt / max_time;
  //
  // Level to points.
  const double level_to_pt = plot_area_wpt / ( max_level + 1 );

  const int width_char = 1.5 * level_to_pt / font_size;  // Approximate width.

  // Traverse tree again, this time emit a rectangle for each tree node.
  //
  stack.push_back(&timer_tree);
  while ( stack.size() )
    {
      RV_Timer_Node* const nd = stack.back();  stack.pop_back();

      const double ht = nd->dur_node_s * s_to_pt;
      const double top_ypt = nd->pseudo_time_start * s_to_pt;

      rect( nd->level * level_to_pt, top_ypt, level_to_pt, ht );
      string name = escapeForXML( nd->name.substr(0,width_char) );

      const double baselineskip_ypt = font_size * 1.2;
      const double text_limit_ypt = top_ypt + ht - baselineskip_ypt;
      const double text_xpt = font_size + nd->level * level_to_pt;
      double curr_text_ypt = nd->pseudo_time_start * s_to_pt;

      if ( curr_text_ypt < text_limit_ypt )
        fprintf(fh, "<text x=\"%.3f\" y=\"%.3f\">%s</text>\n",
                text_xpt, curr_text_ypt += baselineskip_ypt,
                name.c_str());

#     ifdef HAVE_PAPI
      if ( nd->papi_node.filled() )
        {
          const papi_long n_insn =
            max(papi_long(1),nd->papi_node[PAPI_TOT_INS]);
          const papi_long n_cyc = max(papi_long(1),nd->papi_node.cyc);
          const double mpki = 1000.0 * nd->papi_node[PAPI_L3_TCM] / n_insn;

          const papi_long n_cyc_stall = nd->papi_node[PAPI_STL_CCY];
          const papi_long n_cyc_stall_r = nd->papi_node[PAPI_RES_STL];
          const papi_long n_cyc_full = nd->papi_node[PAPI_FUL_CCY];

          if ( curr_text_ypt < text_limit_ypt )
            fprintf(fh, "<text x=\"%.3f\" y=\"%.3f\">L3 %.3f MPKI</text>\n",
                    text_xpt, curr_text_ypt += baselineskip_ypt,
                    mpki );
          if ( curr_text_ypt < text_limit_ypt )
            fprintf(fh, "<text x=\"%.3f\" y=\"%.3f\">%.1f IPC</text>\n",
                    text_xpt, curr_text_ypt += baselineskip_ypt,
                    double(n_insn) / n_cyc );
          if ( curr_text_ypt < text_limit_ypt )
            fprintf(fh, "<text x=\"%.3f\" y=\"%.3f\">Stallr %.1f%%</text>\n",
                    text_xpt, curr_text_ypt += baselineskip_ypt,
                    100.0 * double(n_cyc_stall_r) / n_cyc );
          if ( curr_text_ypt < text_limit_ypt )
            fprintf(fh, "<text x=\"%.3f\" y=\"%.3f\">Stall %.1f%%</text>\n",
                    text_xpt, curr_text_ypt += baselineskip_ypt,
                    100.0 * double(n_cyc_stall) / n_cyc );
          if ( curr_text_ypt < text_limit_ypt )
            fprintf
              (fh, "<text x=\"%.3f\" y=\"%.3f\">Full %.1f%%</text>\n",
               text_xpt, curr_text_ypt += baselineskip_ypt,
               100.0 * n_cyc_full / n_cyc );
          if ( curr_text_ypt < text_limit_ypt )
            fprintf
              (fh, "<text x=\"%.3f\" y=\"%.3f\">Fullns %.1f%%</text>\n",
               text_xpt, curr_text_ypt += baselineskip_ypt,
               100.0 * n_cyc_full / max(papi_long(1),n_cyc-n_cyc_stall) );
        }
#     endif

      for ( auto& pair: nd->children ) stack.push_back(&pair.second);
    }

  fprintf(fh,"%s","</g></svg>\n");
  fclose(fh);
}

void
RV_Data::generate_timeline_simple()
{
  DECLARE_CCTK_PARAMETERS;

  string svg_file_path = string(out_dir) + "/run-view-timeline.svg";
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
     "Writing RunView timeline plot to file \"%s\"\n", svg_file_path.c_str());

  const int ntimers = CCTK_NumTimers();

  //
  // Identify timer-tree timers (participants) and extract their leaf names.
  //
  // For example:
  //
  //  Timer main/CarpetStartup/CheckRegions is a tree timer, its
  //   leaf name is CheckRegions.
  //
  //  Timer [0017] SymBase: SymBase_Startup in CCTK_STARTUP is not a tree timer.
  //
  vector<bool> participant(ntimers);
  vector<string> leaf_name(ntimers);
  for ( int i=0; i<ntimers; i++ )
    {
      Strings pieces = split(CCTK_TimerName(i),'/');
      leaf_name[i] = pieces.back();
      if ( pieces[0] == "main" ) participant[i] = true;
    }

  //
  // Structure and container for holding information on segments to plot.
  //
  // There is a segment for each time a timer is stopped. It will be
  // plotted as a rectangle with the left side based on the timer
  // start time and the right side based on the timer stop time.
  //
  struct Seg_Info {
    Seg_Info(int ti, int l, double ss, double es):
      timer_idx(ti), level(l), start_s(ss), end_s(es){};
    int timer_idx, level;
    double start_s, end_s;
    RV_PAPI_Sample papi;
  };
  vector<Seg_Info> seg_info;

  // Maximum depth of tree.
  int max_level = 0;

  // List of start events for currently active timers.
  //
  vector<RV_Timer_Event*> timer_stack = { NULL };

  for ( auto& ev: rv_ctimers.events )
    {
      const int idx = ev.timer_index;
      if ( ! participant[idx] ) continue;

      RV_Timer_Event* const ev_last = timer_stack.back();

      switch ( ev.event_type ) {

      case RET_Timer_Start:
        // Push timer on to list of active timers.
        timer_stack.push_back(&ev);
        set_max(max_level,timer_stack.size()); // Keep track of max # timers.
        break;

      case RET_Timer_Stop:
        // Pop timer from list of active timers and add new segment to list.
        assert( ev_last->timer_index == idx );
        timer_stack.pop_back();
        seg_info.emplace_back
          (idx,timer_stack.size(),ev_last->etime_s_get(),ev.etime_s_get());
        if ( ev_last->have_papi_sample )
          seg_info.back().papi.accumulate(ev.papi_sample,ev_last->papi_sample);
        break;

      case RET_Timer_Reset: break;

      default: assert( false );
      }
    }


  //
  /// Write SVG Image of Segments
  //

  const double font_size = 10;  // All units in points.
  const double baselineskip_pt = font_size * 1.5;

  // Plot Area (area holding segments) Width
  //
  const double plot_area_wpt = 1000;

  // Compute scale factors.
  //
  // Seconds to Points
  const double s_to_pt = plot_area_wpt / seg_info.back().end_s;
  //
  // Level to Point
  const double level_ht_lines = 5.7;
  const double level_to_pt = level_ht_lines * baselineskip_pt;

  // Height of a Segment.
  const double seg_hpt = level_to_pt;

  // Plot Area (area holding segments) Height
  //
  const double plot_area_hpt = max_level * level_to_pt;

  // Note: Currently image only contains plot. Later, add at least an
  // x-axis scale showing time.
  //
  const double image_wpt = plot_area_wpt;
  const double image_hpt = plot_area_hpt;

  fprintf(fh,"%s",R"~~(<?xml version="1.0" standalone="no"?>
<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN"
  "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">
)~~");

  // Set SVG so that one user unit is one point. This is assuming that
  // user has adjusted font rendering so that a ten-point font is the
  // smallest size that's comfortably readable for substantial amounts
  // of text.
  //
  fprintf(fh,R"~~(<svg width="%.3fpt" height="%.3fpt"
          viewBox="0 0 %.3f %.3f"
          version="1.1" xmlns="http://www.w3.org/2000/svg">
)~~",
          image_wpt, image_hpt, image_wpt, image_hpt);

  fprintf(fh,"%s\n","<desc>Created by RunView</desc>");

  // Make background of plot area gray.
  //
  fprintf
    (fh,
     R"~~(<rect x="%.3f" y="%.3f" width="%.3f" height="%.3f"
     fill="#bbb" stroke="none"/>)~~",
     0.0, 0.0, plot_area_wpt, plot_area_hpt);

  fprintf(fh,R"~~(<g font-size="%.3f" font-family="sans-serif"
          stroke-width="0.2">)~~", font_size);

  // Generate SVG for individual segments.
  //
  for ( auto& s: seg_info )
    {
      const double xpt = s.start_s * s_to_pt;
      const double ypt = s.level * level_to_pt;
      const double wpt = ( s.end_s - s.start_s ) * s_to_pt;

      fprintf(fh, R"~~(<rect fill="white" stroke="black"
              x="%.3f" y="%.3f" width="%.3f" height="%.3f" />)~~",
              xpt, ypt, wpt, seg_hpt);

      // Estimate width assuming that character width is font_size/1.2.
      const int width_char = 1.2 * wpt / font_size;

      if ( width_char < 2 ) continue;

      string name = escapeForXML( leaf_name[s.timer_idx].substr(0,width_char) );
      fprintf(fh, R"--(<text x="%.3f" y="%.3f">%s</text>)--",
              xpt + 0.5*font_size, ypt + font_size, name.c_str() );

#ifdef HAVE_PAPI
      if ( ! s.papi.filled() ) continue;

      const double text_xpt = xpt + 0.5 * font_size;
      const double baselineskip_ypt = font_size * 1.2;
      const double text_limit_ypt = ypt + level_to_pt;
      double curr_text_ypt = ypt + font_size;

      const papi_long n_insn =
        max(papi_long(1),s.papi[PAPI_TOT_INS]);
      const papi_long n_cyc = max(papi_long(1),s.papi.cyc);
      const double mpki = 1000.0 * s.papi[PAPI_L3_TCM] / n_insn;

      const papi_long n_cyc_stall = s.papi[PAPI_STL_CCY];
      const papi_long n_cyc_stall_r = s.papi[PAPI_RES_STL];
      const papi_long n_cyc_full = s.papi[PAPI_FUL_CCY];

      if ( curr_text_ypt < text_limit_ypt )
        fprintf(fh, R"--(<text x="%.3f" y="%.3f">L3 %.3f MPKI</text>)--",
                text_xpt, curr_text_ypt += baselineskip_ypt,
                mpki );
      if ( curr_text_ypt < text_limit_ypt )
        fprintf(fh, R"--(<text x="%.3f" y="%.3f">%.1f IPC</text>)--",
                text_xpt, curr_text_ypt += baselineskip_ypt,
                double(n_insn) / n_cyc );
      if ( curr_text_ypt < text_limit_ypt )
        fprintf(fh, R"--(<text x="%.3f" y="%.3f">Stallr %.1f%%</text>)--",
                text_xpt, curr_text_ypt += baselineskip_ypt,
                100.0 * double(n_cyc_stall_r) / n_cyc );
      if ( curr_text_ypt < text_limit_ypt )
        fprintf(fh, R"--(<text x="%.3f" y="%.3f">Stall %.1f%%</text>)--",
                text_xpt, curr_text_ypt += baselineskip_ypt,
                100.0 * double(n_cyc_stall) / n_cyc );
      if ( curr_text_ypt < text_limit_ypt )
        fprintf
          (fh, R"--(<text x="%.3f" y="%.3f">Full %.1f%%</text>)--",
           text_xpt, curr_text_ypt += baselineskip_ypt,
           100.0 * n_cyc_full / n_cyc );
      if ( curr_text_ypt < text_limit_ypt )
        fprintf
          (fh, R"--(<text x="%.3f" y="%.3f">Fullns %.1f%%</text>)--",
           text_xpt, curr_text_ypt += baselineskip_ypt,
           100.0 * n_cyc_full / max(papi_long(1),n_cyc-n_cyc_stall) );
#endif
    }

  fprintf(fh,"%s","</g></svg>\n");
  fclose(fh);
}
