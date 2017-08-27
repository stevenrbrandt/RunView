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
#include "rv_papi.h"
#include <cmath>
#include <sys/stat.h>

using namespace std;

string getfilepath(string fN) 
  {
     char result[ PATH_MAX ];
     ssize_t count = readlink( "/proc/self/exe", result, PATH_MAX ); 
     string exePath = string(result, (count > 0) ? count : 0);

     // redirect to directory holding javascript file
     size_t found = exePath.find("exe"); 
     string fileName = exePath.substr(0,found) + 
     "arrangements/RunView/RunView/src/" + fN;

     return fileName; 
  }

void
copy_to_server(string path)
{
  // Comment out the line below to enable copying to Web server.
  return;
  const string target_root = "/free/apache/htdocs/rvdemo";
  const char* const target_dn = "ecelsrv3.ece.lsu.edu";
  Strings pieces = split(path.c_str(),'/');
  assert( pieces.size() > 1 );
  string fname = pieces.back();  pieces.pop_back();
  string pname = pieces.back();
  string target_name = pname + "-" + fname;
  string target_path = target_root + "/" + target_name;
  string cmd = "scp -p " + path + " " + target_dn + ":" + target_path;
  pid_t pid = fork();
  if ( pid != 0 ) return;
  printf("Command \"%s\"\n",cmd.c_str());
  chmod(path.c_str(),0644);
  execl("/bin/sh", "sh", "-c", cmd.c_str(), NULL);
  assert( false );
}


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
  double percent_op;  // Percent of parent, set by immediate parent
  double percent_pp;  // percent of the program runtime that is dedicated to this node 

  RV_PAPI_Sample papi_node;
  RV_PAPI_Sample papi_kids;

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

//
// Structure and container for holding information on segments to plot.
//
// There is a segment for each time a timer is stopped. It will be
// plotted as a rectangle with the left side based on the timer
// start time and the right side based on the timer stop time.
//
struct Seg_Info {
  Seg_Info(int e_idx, int ti, int l, double ss, double es):
    start_event_idx(e_idx),
    timer_idx(ti), level(l), start_s(ss), end_s(es),composite(false){};
  int start_event_idx;
  int timer_idx, level;
  double start_s, end_s;
  bool composite;
  RV_PAPI_Sample papi;
};


class RV_Data {
public:
  RV_Data():timer_tree("root"){};
  void init();
  int atend();
  double generate_rect(double y, double w, double scaler, double Psize, RV_Timer_Node* curr, FILE* fh);
  vector<Seg_Info> generate_segments
  (const vector<RV_Timer_Event>& events, int& max_level);
  void generate_text_tree();
  void generate_graph_simple();
  void generate_timeline_simple();
  RV_Timer_Node timer_tree;
  int id_serial; 
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
  const bool show_all_timers = false;

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

vector<Seg_Info>
RV_Data::generate_segments
(const vector<RV_Timer_Event>& events, int& max_level)
{
  vector<Seg_Info> seg_info;

  // Maximum depth of tree.
  max_level = 0;

  // List of start events for currently active timers.
  //
  vector<const RV_Timer_Event*> timer_stack = { NULL };

  for ( int i=0; i<events.size(); i++ )
    {
      const RV_Timer_Event& ev = events[i];
      const int idx = ev.timer_index;

      const RV_Timer_Event* const ev_last = timer_stack.back();

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
          (i,idx,timer_stack.size(),ev_last->etime_s_get(),ev.etime_s_get());
        if ( ev_last->have_papi_sample )
          seg_info.back().papi.accumulate(ev.papi_sample,ev_last->papi_sample);
        break;

      case RET_Timer_Reset: break;

      default: assert( false );
      }
    }
  return move(seg_info);
}


double 
RV_Data::generate_rect(double y, double w, double scaler,
 double Psize, RV_Timer_Node* curr, FILE* fh) 
{ 
  double h = Psize *( curr->percent_op); 
  const double x = curr->level*scaler; 

  // Random number for tracking
  int name = id_serial++; 

  string dNames = to_string(name) + "D";
  string tName = to_string(name) + "TXT"; 
  string pName = curr->name;    

  if (pName.length() > 13) {         // shortens print name if larger than 13 characters
    pName = pName.substr(0,10);
  } 

  // Declaring colors
  string red;
  string green; 
  string blue = "100";

  // Declaring values for drawing
  double __attribute__((unused)) mpkiC;
  double full = 0;
  double fullns = 0;
  double stall = 0;

  // Color
  string fill_color = "rgb(179, 225, 255)"; 

  if ( curr->papi_node.filled() )
    {
      const papi_long n_insn =
	max(papi_long(1),curr->papi_node[PAPI_TOT_INS]);
      const papi_long n_cyc = max(papi_long(1),curr->papi_node.cyc);
      const double mpki = 1000.0 * curr->papi_node[PAPI_L3_TCM] / n_insn;
      
      const papi_long n_cyc_stall = curr->papi_node[PAPI_STL_CCY];
      const papi_long n_cyc_full = curr->papi_node[PAPI_FUL_CCY];
	
      // Scaling colors based on full and fullns :: EXCEPT IT DOES NOT, CHECK IT OUT
      fullns = 100.0 * n_cyc_full / max(papi_long(1),n_cyc-n_cyc_stall);
      full = 100.0 * n_cyc_full / n_cyc;
      mpkiC = mpki; 
      stall =  100.00 * double(n_cyc_stall) / n_cyc;
      
      int r = abs(255*full / 1000 - 255);
      int g = abs((255 * fullns) / 100 + 20);
      green = to_string(g);
      red = to_string(r);
      

      fill_color = "rgb(" + red +"," +green+"," + blue + ")";
    } 

  

  // Drawing rectangle
  fprintf(fh, "<rect class=\"myrect\" id=\"%d\" fill=\"%s\" stroke=\"black\" x=\"%.3f\" "
	  "y=\"%.3f\" width=\"%.3f\" height=\"%.3f\" /> \n ", 
	  name, fill_color.c_str(), x, y, w, h);

  // Calculating and drawing bubbles based on stall & rectangle width
  if (h > 10) {
    double bubble_total_radius = sqrt((mpkiC) * (w*(h-15)) * (4*atan(1)));
    
    while (bubble_total_radius != 0) {
      // establishing radius
      double radius; 
      if ((bubble_total_radius > (w / 3)) || (bubble_total_radius > ((h-15)/3))) {
	if (w > h) {radius = (h-15)/3;}
	else {radius = w/3;}
	bubble_total_radius = bubble_total_radius - radius; 
      } else {
	radius = bubble_total_radius;
	bubble_total_radius = 0; 
      }
      
      // establishing x and y coordinates
      double circle_x = rand()%max(1,int((x+w)-x+1)) + x;
      if ((circle_x + radius) > (x+w)) {circle_x = (x+w) - radius;}
      else if (circle_x - radius < x) {circle_x = x + radius;}
      double circle_y = rand()%max(1,int((y+h)-(y+15)+1)) + (y+15);
      if ((circle_y + radius) > (y+h)) {circle_y = (y+h) - radius;}
      else if (circle_y - radius < (y+15)) {circle_y = (y+15) + radius;}
      
      
      fprintf(fh, "<circle class=\"circleCls\" cx=\"%.3f\" cy=\"%.3f\" r=\"%.5f\" fill=\"white\" "
	      "stroke=\"black\" /> \n", circle_x, circle_y, radius);
    }
  }

  // For testing: prints out data in associated rectangle: 
  /*
  if ( h >= 20 ) {
    fprintf(fh, "<text id=\"%s\" class=\"text\" y=\"%.3f\" font-size=\"10\" > \n"
	    "<tspan class=\"textEl\" x=\"%.3f\"> %s </tspan> \n"
	    "<tspan class=\"textEl\" x=\"%.3f\" dy=\"10\">%%PP: %.3f </tspan> \n" 
	    "<tspan class=\"textEl\" x=\"%.3f\" dy=\"10\">%%Stall: %.3f </tspan> \n"
	    "<tspan class=\"textEl\" x=\"%.3f\" dy=\"10\">%%Full: %.3f </tspan> \n" 
 	    "<tspan class=\"textEl\" x=\"%.3f\" dy=\"10\">%%Fullns: %.3f </tspan> \n" 
	    "</text> \n", tName.c_str(), y+10, x+4, pName.c_str(), x+4, curr->percent_pp*100, 
	    x+4, stall, x+4, full, x+4, fullns);
  } else {
    fprintf(fh, "<text id=\"%s\" class=\"text\" y=\"0\" font-size=\"0\" > \n"
	    "<tspan class=\"textEl\" x=\"0\"> %s </tspan> \n"
	    "<tspan class=\"textEl\" x=\"0\" dy=\"10\">%%PP: %.3f </tspan> \n"
	    "<tspan class=\"textEl\" x=\"0\" dy=\"10\">%%Stall: %.3f </tspan> \n" 
	    "<tspan class=\"textEl\" x=\"0\" dy=\"10\">%%Full: %.3f </tspan> \n" 
	    "<tspan class=\"textEl\" x=\"0\" dy=\"10\">%%Fullns: %.3f </tspan> \n" 
	    "</text> \n", tName.c_str(), pName.c_str(), curr->percent_pp*100, 
	    stall, full, fullns);
  }
*/

  // For regular printing 
  if ( h >= 20 ) {
    fprintf(fh, "<text id=\"%s\" class=\"text\" y=\"%.3f\" font-size=\"10\" > \n"
	    "<tspan class=\"textEl\" x=\"%.3f\"> %s </tspan> \n"
	    "</text> \n", tName.c_str(), y+10, x+4, pName.c_str());
  } else {
    fprintf(fh, "<text id=\"%s\" class=\"text\" y=\"0\" font-size=\"0\" > \n"
	    "<tspan class=\"textEl\" x=\"0\"> %s </tspan> \n"
	    "</text> \n", tName.c_str(), pName.c_str());
  }
  
  if (curr->children.size() != 0) {
    fprintf(fh, "<g id=\"%s\"> \n", dNames.c_str()); 

  // drawing rectangles of children
  for (auto& elt: curr->children) {
    RV_Timer_Node* const ch = &elt.second;
    double t = generate_rect(y,w,scaler,h,ch, fh);
    y += t;
  }
  fprintf(fh, "</g> \n"); 
  }


  return h;
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
  const double plot_area_wpt = image_wpt - 2 * plot_area_left_xpt;
  const double plot_area_hpt = image_hpt - 2 * plot_area_top_ypt;

  // Write SVG Header
  //
  /*
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

  */
  // Convenience function for emitting SVG rectangles.
  //

    // Write to html file 
  // Write SVG Header
  fprintf(fh,"%s",
	  "<html> \n <head> \n"
	  "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\"> \n"
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

       nd->percent_pp = ((nd->dur_node_s - nd->dur_kids_s)/total_time); 

      // Set the pseudo_time_start members of this node's children.
      //
      for ( auto& pair: nd->children )
        {
          RV_Timer_Node* const ch = &pair.second;
          ch->pseudo_time_start = pseudo_time_start;

	  // calculating relevant percentages
	  ch->percent_op = (ch->dur_node_s / nd->dur_node_s);
	 

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
  // Level to points.
  const double level_to_pt = plot_area_wpt / ( max_level + 1 );

  stack.push_back(&timer_tree);

  // setting up main for drawRectangles function
  main->percent_op = 1; 
  
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
	  "</text> \n",  plot_area_top_ypt+10, plot_area_left_xpt+4, timer_tree.name.c_str());
  
  fprintf(fh, "<g id=\"rootD\" > \n");
  
  // Draw rectangles
  generate_rect(plot_area_top_ypt, level_to_pt,level_to_pt,
                plot_area_hpt, main, fh);
  
  fprintf(fh, "%s", "</g> \n"); 
  fprintf(fh, "%s", "</svg> \n"); 
  
   
   // Add javascript features from file
   string fN = "simpleGraphScript.txt";
   string fileName = getfilepath(fN); 
   
   // Open and read from file
   string line; 
   std::ifstream scriptFile;
   scriptFile.open(fileName.c_str(), std::ifstream::in); 
   if (scriptFile.is_open()) {
     while (!scriptFile.eof()) {
       getline(scriptFile, line); 
       fprintf(fh, "%s \n", line.c_str()); 
     }
   }

   // Close
   fprintf(fh, "%s", "</body> \n </head>\n </html>\n");
 
   fclose(fh);

   copy_to_server(svg_file_path);
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

  // Maximum depth of tree.
  int max_level = 0;

  vector<RV_Timer_Event> events;
  for ( auto& ev: rv_ctimers.events )
    if ( participant[ev.timer_index] ) events.push_back(ev);

  vector<Seg_Info> seg_info = generate_segments(events,max_level);

  /// Write SVG Image of Segments
  //

  const double font_size = 10;  // All units in points.
  const double baselineskip_pt = font_size * 1.5;

  // Plot Area (area holding segments) Width
  //
  const double plot_area_wpt = 800;

  // Compute scale factors.
  //
  // Seconds to Points
  const double s_to_pt = plot_area_wpt / seg_info.back().end_s;

  // Total time
  double duration = seg_info.back().end_s; 
  printf("\n\n duration: %.3f \n \nf", duration); 
  //
  // Level to Point
  const double level_ht_lines = 3.5; //5.7
  const double level_to_pt = level_ht_lines * baselineskip_pt;

  // Height of a Segment.
  const double seg_hpt = level_to_pt;

  // Plot Area (area holding segments) Height
  //
  const double plot_area_hpt = max_level * level_to_pt * 2;

  const double image_wpt = plot_area_wpt;
  const double image_hpt = plot_area_hpt;

  fprintf(fh,"%s",R"~~(<?xml version="1.0" standalone="no"?>
<!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN"
  "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd">
)~~");

  fprintf(fh,"%s",
          "<?xml version=\"1.0\" standalone=\"no\"?>\n"
          "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\"\n"

          "  \"http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd\">\n");
  // InfoBox information 
  fprintf(fh,"<svg width=\"%.3fpt\" height=\"100pt\"\n"
          "viewBox=\"0 0 %.3f 100\"\n"
	  "preserveAspectRatio = \"xMinYMin meet\" "
          "version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\n",
          image_wpt, image_wpt);
  
  string name = "DK"; 

  fprintf(fh, "<g font-size=\"10.000\" font-family=\"sans-serif\" stroke-width=\"0.2\"> \n"
	  " <rect id=\"infoBoxBackground\" fill=\"rgb(125, 175, 255)\" "
	  "stroke=\"none\" x=\"0\" y=\"0\" width=\"%.3fpt\" height=\"100pt\" /> \n"
	  "<text id=\"infoText\" class=\"text\" y=\"15.000\" font-size=\"10\"> \n"
	  "<tspan class=\"textEl\" x=\"9.000\" font-size=\"15\" >Summary of Execution:  </tspan> \n "
	  "<tspan class= \"textEl\" x=\"9.000\" dy=\"15\">Author: %s </tspan> \n"
	  "<tspan class= \"textEl\" x=\"9.000\" dy=\"10\">Duration: %.4f</tspan> \n "
	  "</text> \n </g> \n </svg> \n", image_wpt, name.c_str(), duration);

  // Drawing the timeline
  double secMarks = floor (duration); 
  double spaceBtwTicks = ((image_wpt * secMarks) / duration) / secMarks; 
  
  fprintf(fh,"<svg width=\"%.3fpt\" height=\"35pt\"\n"
	  "viewbox=\"0 0 %.3f 35\""
          "version=\"1.1\" xmlns=\"http://www.w3.org/2000/svg\">\n",
          image_wpt, image_wpt);

  fprintf(fh, "<g font-size=\"10.000\" font-family=\"sans-serif\" stroke-width=\"0.2\"> \n"
	  "<line x1=\"0\" y1=\"20\" x2=\"%.3f\" y2=\"20\"  style=\"stroke:rgb(0,0,0);stroke-width:1\"/> \n"
	  , image_wpt);
 
  // setting tick mark locations
  double xLocation = 0;

  for (int i = 0; i <= secMarks; i++) {
    fprintf(fh, "<line x1=\"%.3f\" y1=\"15\" x2=\"%.3f\" y2=\"25\" style=\"stroke:rgb(0,0,0);stroke-width:1\"/> \n", xLocation, xLocation); 

    if (i > 0) {
    fprintf(fh, "<text x=\"%.3f\" y=\"9\" > %d </text> \n", xLocation-3, i); 
    }

    xLocation = xLocation + spaceBtwTicks;
  }

  fprintf(fh, "</g> \n </svg>"); 



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

  // Make main rectangle
  fprintf
    (fh,
     "<rect class= \"main\" x=\"%.3f\" y=\"%.3f\" fill=\"rgb(179, 225, 255)\" width=\"%.3f\" height=\"%.3f\" "
     "stroke=\"white\" stroke-width=\".75\"/>\n",
     0.0, 0.0, plot_area_wpt, seg_hpt);

  fprintf(fh, "<text x=\"%.3f\" y=\"%.3f\">Main</text>\n",
              0 + 0.5*font_size, 0 + font_size );




  // Generate SVG for individual segments.
  // Also tracking for pattern recognition

  for ( auto& s: seg_info )
    {
      const double xpt = s.start_s * s_to_pt;
      const double ypt = s.level * level_to_pt;
      const double wpt = ( s.end_s - s.start_s ) * s_to_pt;

      string clsName = "timer-" + to_string(s.timer_idx); 

      string fill_color = "rgb(179, 225, 255)"; 

      fprintf(fh, "<rect class=\"%s\" fill=\"%s\" stroke=\"white\" stroke-width=\".75\" "
	      "x=\"%.3f\" y=\"%.3f\" width=\"%.3f\" height=\"%.3f\" />\n", 
	      clsName.c_str(), fill_color.c_str(), xpt, ypt, wpt, seg_hpt);

      // Estimate width assuming that character width is font_size/1.2.
      const int width_char = 1.2 * wpt / font_size;
      const string name = escapeForXML( leaf_name[s.timer_idx].substr(0,width_char) );

      if ( width_char > 2 ) 
      fprintf(fh, R"--(<text x="%.3f" y="%.3f">%s</text>)--",
              xpt + 0.5*font_size, ypt + font_size, name.c_str() );
      
      // Declaring colors
      string red;
      string green; 
      string blue = "50"; 
 
#ifdef HAVE_PAPI
      if ( ! s.papi.filled() ) continue;
      {
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
	
	double fullns = 100.0 * n_cyc_full / max(papi_long(1),n_cyc-n_cyc_stall);
	double full = 100.0 * n_cyc_full / n_cyc;
	double mpkiC = mpki; 
	double stall =  100.00 * double(n_cyc_stall) / n_cyc;
	
	int r = abs(255*round(full) / 100 - 255);
	int g = (255 * round(fullns)) / 100;
	green = to_string(g);
	red = to_string(r);

	fill_color = "rgb(" + red +"," +green+"," + blue + ")";
		
	fprintf(fh, "<rect class=\"%s\" fill=\"%s\" stroke=\"white\" stroke-width=\".75\" "
		"x=\"%.3f\" y=\"%.3f\" width=\"%.3f\" height=\"%.3f\" />\n", 
		clsName.c_str(), fill_color.c_str(), xpt, ypt, wpt, seg_hpt);

	// Calculating and drawing bubbles based on stall & rectangle width
	if (wpt > 10) {
	  double bubble_total_radius = sqrt((mpki) * (wpt*seg_hpt) * (4*atan(1)));
	  
	  while (bubble_total_radius != 0) {
	    // establishing radius
	    double radius; 
	    if ((bubble_total_radius > (wpt / 2.5)) || (bubble_total_radius > (seg_hpt/2.5))) {
	      if (wpt > seg_hpt) {radius = seg_hpt/3;}
	      else {radius = wpt/3;}
	      bubble_total_radius = bubble_total_radius - radius; 
	    } else {
	      radius = bubble_total_radius;
	      bubble_total_radius = 0; 
	    }
	    
	    // establishing x and y coordinates
	    double circle_x = rand()%(int)((xpt+wpt)-xpt+1) + xpt; 
	    if ((circle_x + radius) > (xpt+wpt)) {circle_x = (xpt+wpt) - radius;}
	    else if (circle_x - radius < xpt) {circle_x = xpt + radius;}
	    double circle_y = rand()%(int)((ypt+seg_hpt-(ypt+baselineskip_ypt))+1) 
	      + (ypt+baselineskip_ypt); 
	    if ((circle_y + radius) > (ypt+seg_hpt)) 
	      {circle_y = (ypt+seg_hpt) - radius;}
	    else if (circle_y - radius < (ypt+baselineskip_ypt)) 
	      {circle_y = (ypt+baselineskip_ypt) + radius;}
      
      
	    fprintf(fh, "<circle class=\"circleCls\" cx=\"%.3f\" cy=\"%.3f\""
		    "r=\"%.5f\" fill=\"white\" "
		    "stroke=\"black\" /> \n", circle_x, circle_y, radius);
	  }
	}

	if ( width_char < 2 ) continue;
	fprintf(fh, R"--(<text x="%.3f" y="%.3f">%s</text>)--",
		xpt + 0.5*font_size, ypt + font_size, name.c_str() );
	
	/*
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
	*/

      }
#endif

    }

  vector <int> event_indices;
  {
    int level = 0;
    for ( auto& ev: events )
      switch ( ev.event_type ) {
      case RET_Timer_Start:
        event_indices.push_back
          ( PatternFinderEncodeStart(ev.timer_index,level) );
        level++;
        break;
      case RET_Timer_Stop:
        level--;
        assert( level >= 0 );
        event_indices.push_back
          ( PatternFinderEncodeStop(ev.timer_index,level) );
        break;
      case RET_Timer_Reset: event_indices.push_back( -2 ); break;
      }
  }

  assert( event_indices.size() == events.size() );

  PatternFinder pf2(move(event_indices), events, 2000);

  vector<RV_Timer_Event> timer_compact;

  vector<bool> events_done(events.size());
  timer_compact.push_back( events[0] );
  events_done[0] = true;
  vector<bool> timer_compact_is_composite;

  for ( int i=1; i<events.size(); i++ )
    {
      if ( events_done[i] ) continue;
      const RV_Timer_Event& ev = events[i];
      const RV_Timer_Event& ev_last = timer_compact.back();
      int64_t duration = 0;
      RV_PAPI_Sample* const papi_sample =
        ev_last.have_papi_sample
        ? new RV_PAPI_Sample(*ev_last.papi_sample) : NULL;
      auto back_pairs = pf2.getBackPairs(i);
      for ( int ii: back_pairs )
        {
          RV_Timer_Event& prev = events[ii-1];
          RV_Timer_Event& curr = events[ii];
          assert( !events_done[ii] );
          events_done[ii] = true;
          if ( papi_sample && prev.have_papi_sample && curr.have_papi_sample )
            papi_sample->accumulate(curr.papi_sample,prev.papi_sample);
          else
            duration += curr.etime_get() - prev.etime_get();
        }
      timer_compact_is_composite.push_back( back_pairs.size() > 1 );
      if ( papi_sample )
        {
          papi_sample->cyc += duration;
          timer_compact.emplace_back(papi_sample,ev.timer_index,ev.event_type);
        }
      else
        timer_compact.emplace_back
          (duration+ev_last.etime_get(),ev.timer_index,ev.event_type);
    }

  vector<Seg_Info> seg_compact_info =
    generate_segments(timer_compact,max_level);

  const int sc_to_pt = plot_area_wpt / seg_compact_info.back().end_s;

  for ( auto& s: seg_compact_info )
    {
      const bool composite = timer_compact_is_composite[s.start_event_idx];

      // If not part of a pattern draw in gray 
      const char* const color = composite ? "yellow" : "gray";

      double xpt = s.start_s * sc_to_pt;
      double wpt = ( s.end_s - s.start_s ) * sc_to_pt;
      double ypt = s.level * level_to_pt  + (level_to_pt * max_level);
      string clsName = "timer-" + to_string(s.timer_idx) + "B";
      string name1 = leaf_name[s.timer_idx] + "B";

      fprintf(fh, "<rect class=\"%s\" fill=\"%s\" stroke=\"white\" stroke-width=\".75\" "
              "x=\"%.3f\" y=\"%.3f\" width=\"%.3f\" height=\"%.3f\" />\n", 
              clsName.c_str(), color, xpt, ypt, wpt, seg_hpt);
      
      fprintf(fh, R"--(<text x="%.3f" y="%.3f">%s</text>)--",
	      xpt + 0.5*font_size, ypt + font_size, name1.c_str() );

    }

  fprintf(fh,"%s","</g></svg>\n");


   // Add javascript features from file
  string fN = "timelineSimpleScript.txt";
  string fileName = getfilepath(fN); 

  string line; 
  std::ifstream scriptFile;
  scriptFile.open(fileName.c_str(), std::ifstream::in); 
  if (scriptFile.is_open()) {
    while (!scriptFile.eof()) {
      getline(scriptFile, line); 
      fprintf(fh, "%s \n", line.c_str()); 
    }
  }

  fprintf(fh, "%s","</body> \n </html>"); 
  fclose(fh);

  copy_to_server(svg_file_path);
}
