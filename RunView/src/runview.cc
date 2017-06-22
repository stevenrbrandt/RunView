#include <stdio.h>
#include <cctk.h>
#include <cctk_Arguments.h>
#include <cctk_Parameters.h>
#include <cctk_Schedule.h>
#include <assert.h>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <errno.h>
#include <string.h>
#include "ctimers.h"
#include "util.h"
#include <time.h>

using namespace std;

class RV_Timer_Node {
public:
  RV_Timer_Node(string node_name)
  {
    dur_node_s = dur_kids_s = 0;
    dur_node_set = true;
    name = node_name;
    level = 0;
    id = rand()%10000000;
  }
  RV_Timer_Node()
  {
    dur_kids_s = 0;
    dur_node_set = false;
    level = -1;
    id = rand()%10000000;
  }
  void insert(Strings& path, int levelp, double time)
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
          return;
        }

      // Check if this is the parent of the leaf node of the timer.
      // If so, collect time of children.
      //
      if ( path.size() == level + 1 ) dur_kids_s += time;

      // Insert a node for next component.
      //
      children[path[level]].insert(path,level+1,time);
    }

  double dur_node_s;  // Duration recorded by Cactus timer.
  double dur_kids_s;  // Sum of durations of immediate children.
  double percent_op;  // Percent of parent, set by immediate parent
  double percent_pp;  // percent of the program runtime that is dedicated to this node 

  double pseudo_time_start;  // Start time, set for a particular image.

  // Component of timer name. For example, for timer
  // main/Evolve/PrintTimers there would be three nodes, named main,
  // Evolve, and PrintTimers, at levels 1, 2, and 3, respectively.
  string name;
  int id; 
  int level;

  bool dur_node_set;  // If true, dur_node_s set to some value.
  map<string,RV_Timer_Node> children;
};

class RV_Data {
public:
  RV_Data():timer_tree("root"){};
  int atend();
  double generate_rect(double x, double y, double w, double scaler, double Psize, RV_Timer_Node* curr, FILE* fh);
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
  DECLARE_CCTK_PARAMETERS;

  // A message providing reassurance to a beginner becoming familiar with
  // the code. Delete the message when no longer necessary.
  //
  printf("Hello from init.\n");

  // Initialize the collection of timer event data
  //
  rv_ctimers.init();

  return 0;
}

CCTK_INT
RunView_atend()
{

  // Print runview timer data.
  //
  return rv_data.atend();
}



int
RV_Data::atend()
{
  /// Print Timer Data

  // If true, show lots of data.  Intended for debugging and
  // familiarization.
  //
  const bool show_all_timers = false;

  const int ntimers = CCTK_NumTimers();
  cTimerData* const td = CCTK_TimerCreateData();

  // Note: Each timer has multiple clocks. In most cases a clock
  // measures time and so only one would be necessary.
  const int nclocks = CCTK_NumClocks();

  if ( show_all_timers )
    {
      printf("Found %d timers.\n", ntimers);
      printf("Found %zd events.\n", rv_ctimers.events.size());
      for ( int i=0; i<nclocks; i++ )
        printf("Clock %d: %s\n",i,CCTK_ClockName(i));
    }

  // Organize timer data into a tree, timer_tree.
  //
  srand(time(NULL)); 

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
          printf("%10.6f %3.0f µs %s %3d %s\n",
                 timer_secs, delta, tv->heading, t.missequence_count, name);
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
      timer_tree.insert(pieces,0,t.duration_s);
    }

  CCTK_TimerDestroyData(td);

  // Write out text and graphical representations of timer trees.
  //
  generate_text_tree();
  generate_graph_simple();
  generate_timeline_simple();

  return 0;
}

double 
RV_Data::generate_rect(double x, double y, double w, double scaler, double Psize, RV_Timer_Node* curr, FILE* fh) 
{ 
  double h = Psize *( curr->percent_op/100); 
  
  // Random number for tracking
  int name = rand()%10000000; 

  std::ostringstream dNames;
  dNames << name << "D";             // name of decendant group
  std::ostringstream tName;
  tName << name << "TXT";            // text of name
  std::string pName = curr->name;    // print version of name
  if (pName.length() > 13) {         // shortens print name if larger than 13 characters
    pName = pName.substr(0,10);
  } 

  // Color based on runtime
  const char* const fill_color=
    curr->percent_pp < .3? "rgb(179, 225, 255)":
    curr->percent_pp < 10? "rgb(53,133,223)":"rgb(255,125,37)";

  // Draw Rectangle 
  fprintf(fh, "<rect class=\"myrect\" id=\"%d\" fill=\"%s\" stroke=\"black\" x=\"%.3f\" y=\"%.3f\""
	  "width=\"%.3f\" height=\"%.3f\" />\n", name,fill_color, x, y, w, h); 

  // printing the text
  if ( h >= 20 ) {
    fprintf(fh, "<text id=\"%s\" class=\"text\" y=\"%.3f\" font-size=\"10\" > \n"
	    "<tspan class=\"textEl\" x=\"%.3f\"> %s </tspan> \n"
	    "<tspan class=\"textEl\" x=\"%.3f\" dy=\"10\">%% %.5f </tspan> \n"
	    "</text> \n", tName.str().c_str(), y+10, x+4, pName.c_str(), x+4, curr->percent_pp);
  } else {
    fprintf(fh, "<text id=\"%s\" class=\"text\" y=\"0\" font-size=\"0\" > \n"
	    "<tspan class=\"textEl\" x=\"0\"> %s </tspan> \n"
	    "<tspan class=\"textEl\" x=\"0\" dy=\"10\">%% %.5f </tspan> \n"
	    "</text>\n", tName.str().c_str(), pName.c_str(), curr->percent_pp);
  }

  if (curr->children.size() != 0) {
    fprintf(fh, "<g id=\"%s\"> \n", dNames.str().c_str()); 
  // drawing rectangles of children
  for (auto& elt: curr->children) {
    RV_Timer_Node* const ch = &elt.second;
    x = ch->level*scaler; 
    double t = generate_rect(x,y,w,scaler,h,ch, fh);
    y += t;
  }
  fprintf(fh, "</g> \n"); 
  }


  return h;
}


void
RV_Data::generate_text_tree()
{
  // Write out a text representation of timer tree.

  // Put root of tree on stack.
  vector<RV_Timer_Node*> stack = { &timer_tree };

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
  // Write out SVG image showing timer values.

  // Declare Cactus parameters. (Needed for out_dir.)
  DECLARE_CCTK_PARAMETERS;

  string svg_file_path = string(out_dir) + "/run-view.html";
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
  const double plot_area_wpt = image_wpt -2 * plot_area_left_xpt;
  const double plot_area_hpt = image_hpt -2 * plot_area_top_ypt;


  // Convenience functions for emitting SVG rectangles.
   auto rect = [&](double x, double y, double w, double h)
     { fprintf(fh, "<rect fill=\"gray\" stroke=\"black\" "
               "x=\"%.3f\" y=\"%.3f\" width=\"%.3f\" height=\"%.3f\" />\n",
               x, y, w, h); };

  // Emit a box around the entire plot area.
  // rect( plot_area_left_xpt, plot_area_top_ypt, plot_area_wpt-4, plot_area_hpt);

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

  // NOT A ROBUST SYSTEM YET. ONLY WORKS IF CHILD OF ROOT IS CALLED MAIN
  // WHICH MAY NOT ALWAYS BE THE CASE? 
  timer_tree.dur_node_s = timer_tree.dur_kids_s; 
  RV_Timer_Node* main = &timer_tree.children.find("main")->second; 
  double total_time = main->dur_kids_s; 
  main->dur_node_s = main->dur_kids_s; 
 
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

       nd->percent_pp = ((nd->dur_node_s - nd->dur_kids_s)/total_time)*100; 

      // Set the pseudo_time_start members of this node's children.
      //
      for ( auto& pair: nd->children )
        {
          RV_Timer_Node* const ch = &pair.second;
          ch->pseudo_time_start = pseudo_time_start;

	  // calculating relevant percentages
	  ch->percent_op = (ch->dur_node_s / nd->dur_node_s)*100;
	 

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

  main->dur_node_s = 0; 
  

  // Compute scale factors
  //
  // Seconds to points.
  const double s_to_pt = plot_area_hpt / max_time;
  //
  // Level to points.
  const double level_to_pt = plot_area_wpt / ( max_level + 1 );

  const int width_char = 1.5 * level_to_pt / font_size;  // Approximate width.

  // setting up main for drawRectangles function
   main->percent_op = 100; 

  // Write to html file 
  // Write SVG Header
  fprintf(fh,"%s",
	  "<html> \n <head> \n"
	  "<meta http-equiv=\"Conten<h1></h1>t-Type\" content=\"text/html; charset=UTF-8\"> \n"
	  "<script src=\"http://code.jquery.com/jquery-latest.min.js\"></script>\n"
	  "</head> \n <body> \n"
          "<?xml version=\"1.0\" standalone=\"no\"?>\n"
          "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n"
          " \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n"
	  );

  // Set SVG so that one user unit is one point. This is assuming that
  // user has adjusted font rendering so that a ten-point font is the
  // smallest size that's comfortably readable for substantial amounts
  // of text.
  //
  fprintf(fh,"<svg width=\"%.3fpt\" height=\"%.3fpt\"\n"
          "viewBox=\"0 0 %.3f %.3f\"\n"
          "version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\n",
          image_wpt, image_hpt, image_wpt, image_hpt);

  fprintf(fh,"%s\n","<desc>Created by RunView</desc>");
  fprintf(fh,"<g id=\"all\" font-size=\"%.3f\" font-family=\"sans-serif\">\n",
          font_size);

  // draw rectangle around entire space
    fprintf(fh, "<rect  fill=\"rgb(203, 203, 203)\"  stroke=\"black\" "
	    "x=\"%.3f\" y=\"%.3f\" width=\"%.3f\" height=\"%.3f\" />\n", plot_area_left_xpt,  
	    plot_area_top_ypt, plot_area_wpt -4, plot_area_hpt);

   // Draw root rectangle
   fprintf(fh, "<rect id=\"%s\" class=\"myrect\" fill=\"rgb(179, 225, 255)\" stroke=\"black\" "
	   "x=\"%.3f\" y=\"%.3f\" width=\"%.3f\" height=\"%.3f\" />\n",timer_tree.name.c_str(),
	   plot_area_left_xpt,  plot_area_top_ypt, level_to_pt, plot_area_hpt);

    fprintf(fh, "<text id=\"rootTXT\" class=\"text\" y=\"%.3f\" font-size=\"10\" > \n"
	    "<tspan class=\"textEl\" x=\"%.3f\"> %s </tspan> \n"
	    "<tspan class= \"textEl\" x=\"%.3f\" dy=\"10\">%% %.5f </tspan> \n"
	    "</text> \n",  plot_area_top_ypt+10, plot_area_left_xpt+4, timer_tree.name.c_str(), 
	    plot_area_left_xpt+4, timer_tree.percent_pp);
   
   fprintf(fh, "<g id=\"rootD\" > \n");

   // Draw rectangles
   srand(time(NULL)); 
   double ta = generate_rect(main->level * level_to_pt, plot_area_top_ypt, 
			     level_to_pt,level_to_pt, plot_area_hpt, main, fh); 
   fprintf(fh, "%s", "</g> \n"); 
   fprintf(fh, "%s", "</svg> \n"); 
   

   // Add javascript features 
   fprintf(fh, "%s", 
	   "<script type=\"text/javascript\"> \n"
	   "$('#all').find('.myrect').mouseenter( function () colorChange(this) ); \n"
	   "$('#all').find('.myrect').click( function () expand(this) ); \n"
	   "function colorChange(rect) { \n"
	   "var color = rect.getAttribute(\"fill\"); \n"
	   "$(rect).attr(\"fill\", \"rgb(255,101,142)\"); \n "
	   "$(rect).mouseleave(function () { $(rect).attr(\"fill\", color);}) \n } \n"
	   "function expand(rect) {\n"
	   "var findMain = document.getElementById(\"all\").getElementsByClassName(\"myrect\"); \n "
	   "var svgHeight = Number(findMain.item(0).getAttribute(\"height\")); \n"
	   "var width = Number(rect.getAttribute(\"width\")); \n"
	   "var height = Number(rect.getAttribute(\"height\")); \n"
	   "var scale = svgHeight/height; \n"
	   "var name = rect.getAttribute(\"id\");\n"
	   "var d = \"D\";\n"
	   "var decendants = name.concat(d);\n"
	   "$(\"#all\").find(\".myrect\").hide(); \n"
	   "$(\"#all\").find(\".text\").hide(); \n"
	   "var coeff = Number(rect.getAttribute(\"x\"))/width; \n"
	   "var y = 5; \n var parentY=Number(rect.getAttribute(\"y\")); \n parentH = Number(rect.getAttribute(\"height\")); \n"
	   "if (document.getElementById(decendants) !== null) { \n"
	   "var decs = document.getElementById(decendants).getElementsByClassName(\"myrect\"); \n"
	   " for (var i = 0; i < decs.length; i++) { \n"
	   "var curr = decs.item(i); \n"
	   "var newHeight = Number(curr.getAttribute(\"height\"))*scale; \n"
	   "var x = Number(curr.getAttribute(\"x\")) - coeff * width+5; \n"
	   "var currY = Number(curr.getAttribute(\"y\")); \n"
	   "y=(((currY - parentY) * svgHeight)/parentH +5); \n"
	   "curr.setAttribute(\"x\", x);\n "
	   "curr.setAttribute(\"y\", y); \n"
	   "curr.setAttribute(\"height\", newHeight); \n"
	   "var rN = curr.getAttribute(\"id\"); \n"
	   "var t = \"TXT\"; \n"
	   "var text = rN.concat(t); \n"
	   "if (newHeight > 21) { \n"
	   "var textEl = document.getElementById(text); \n"
	   "textEl.setAttribute(\"y\", y+10); \n"
	   "textEl.setAttribute(\"font-size\", \"10\"); \n"
	   "var textEls = document.getElementById(text).getElementsByClassName(\"textEl\"); \n"
	   "textEls.item(0).setAttribute(\"x\", x+5); \n"
	   "textEls.item(1).setAttribute(\"x\", x+5); \n"
	   "} \n"
	   "\n } \n } \n"
	   "rect.setAttribute(\"x\", \"5\"); \n rect.setAttribute(\"y\", \"5\"); \n rect.setAttribute(\"height\", svgHeight); \n"
	   "var rN = rect.getAttribute(\"id\"); \n"
	   "var t = \"TXT\"; \n"
	   "var text = rN.concat(t); \n"
	   "var textEl = document.getElementById(text); \n"
	   "textEl.setAttribute(\"y\", \"15\"); \n"
	   "textEl.setAttribute(\"font-size\", \"10\"); \n"
	   "var textEls = document.getElementById(text).getElementsByClassName(\"textEl\"); \n"
	   "textEls.item(0).setAttribute(\"x\", \"10\"); \n"
	   "textEls.item(1).setAttribute(\"x\", \"10\"); \n"
	   "$(document.getElementById(name)).show(); \n"
	   " $(document.getElementById(decendants)).find(\".myrect\").show(); \n"
	   "$(document.getElementById(text)).show(); \n"
	   " $(document.getElementById(decendants)).find(\".text\").show(); \n"
	   "}"
	   "</script> \n");

   // Close
   fprintf(fh, "%s", "</body> \n </head> \n");
 
   fclose(fh);
}

void
RV_Data::generate_timeline_simple()
{
  DECLARE_CCTK_PARAMETERS;

  string svg_file_path = string(out_dir) + "/run-view-timeline.html";
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
          (idx,timer_stack.size(),ev_last->event_time_s,ev.event_time_s);
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
  const double level_ht_lines = 2;
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

  fprintf(fh, "%s",
	  "<html> \n <body> \n"); 

  fprintf(fh, "%s",
	  "<head> \n"
	  "<meta http-equiv=\"Conten<h1></h1>t-Type\" content=\"text/html; charset=UTF-8\"> \n"
	  "<script src=\"http://code.jquery.com/jquery-latest.min.js\"></script> \n"
	  "</head> \n");

  fprintf(fh,"%s",
          "<?xml version=\"1.0\" standalone=\"no\"?>\n"
          "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n"
          "  \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");

  // Set SVG so that one user unit is one point. This is assuming that
  // user has adjusted font rendering so that a ten-point font is the
  // smallest size that's comfortably readable for substantial amounts
  // of text.
  //
  fprintf(fh,"<svg width=\"%.3fpt\" height=\"%.3fpt\"\n"
          "viewBox=\"0 0 %.3f %.3f\"\n"
          "version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\n",
          image_wpt, image_hpt, image_wpt, image_hpt);

  fprintf(fh,"%s\n","<desc>Created by RunView</desc>");

  // Make background of plot area gray.
  //
  fprintf
    (fh,
     "<rect x=\"%.3f\" y=\"%.3f\" width=\"%.3f\" height=\"%.3f\" "
     "fill=\"#bbb\" stroke=\"none\"/>\n",
     0.0, 0.0, plot_area_wpt, plot_area_hpt);

  fprintf(fh,"<g font-size=\"%.3f\" font-family=\"sans-serif\" "
          "stroke-width=\"0.2\">\n", font_size);

  // Generate SVG for individual segments.
  //
  map<string, int> classNames; 
  srand(time(NULL)); 

  for ( auto& s: seg_info )
    {
      const double xpt = s.start_s * s_to_pt;
      const double ypt = s.level * level_to_pt;
      const double wpt = ( s.end_s - s.start_s ) * s_to_pt;

      //const string name = leaf_name[s.timer_idx]; 
       //printf("\n  %s  ", name.c_str()); 
       //printf("  %d  \n", s.timer_idx);

      int clsName; 
      std::ostringstream key;
      key << s.timer_idx; 

  
      if (classNames.find(key.str()) != classNames.end()) {
	clsName = classNames.find(key.str())->second; 
      } else {
	clsName = rand()%1000000; 
	classNames.insert(std::pair<string,int>(key.str(),clsName));
      } 

      fprintf(fh, "<rect class=\"%d\" id=\"%s \" fill=\"white\" stroke=\"black\" "
              "x=\"%.3f\" y=\"%.3f\" width=\"%.3f\" height=\"%.3f\" />\n", 
	      clsName, leaf_name[s.timer_idx].c_str(), xpt, ypt, wpt, seg_hpt);

      // Estimate width assuming that character width is font_size/1.2.
      const int width_char = 1.2 * wpt / font_size;
      const string name = escapeForXML( leaf_name[s.timer_idx].substr(0,width_char) );

      if ( width_char < 2 ) continue;
      fprintf(fh, "<text x=\"%.3f\" y=\"%.3f\">%s</text>\n",
              xpt + 0.5*font_size, ypt + font_size, name.c_str() );
    }

  fprintf(fh,"%s","</g></svg>\n");

  fprintf(fh, "%s", 
	  "<script type=\"text/javascript\"> \n"
	  "$('rect').mouseenter(function () highlight(this)); \n"
	  "function highlight(rect) { \n"
	  "var clsName = rect.getAttribute('class'); \n"
	  "$('.'+clsName).attr(\"fill\", \"rgb(255,145, 48)\"); \n"
	  "$('.'+clsName).mouseleave( function() {$('.'+clsName).attr(\"fill\", \"white\"); }); } \n"
	  "</script> \n");
  fprintf(fh, "%s","</body> \n </html>"); 
  fclose(fh);
}
