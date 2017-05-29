// -*- c++ -*-
#ifndef CTIMERS_H
#define CTIMERS_H

#include <vector>

enum RV_Event_Type
  { RET_Unset, RET_Timer_Start, RET_Timer_Stop, RET_Timer_Reset, RET_ENUM_SIZE};

struct RV_Timer_Event {
  RV_Timer_Event(int idx, RV_Event_Type et);
  const double event_time_s;
  const int timer_index;
  const RV_Event_Type event_type;
};

class RV_CTimer {
public:
  RV_CTimer()
    :event_last(RET_Unset), event_count(RET_ENUM_SIZE), duration_s(0),
     missequence_count(0){}
  RV_Event_Type event_last;
  std::vector<int> event_count;
  double duration_s;
  double last_start_time_s;
  int missequence_count; // E.g., double start, stop when not running.
};

class RV_CTimers {
public:
  RV_CTimers(){ num_events = 0; }

  void init();
  void timers_update();
  RV_CTimer& get(int cactus_timer_index);

  std::vector<RV_Timer_Event> events;
  double epoch_s;
private:
  std::vector<RV_CTimer> timers;
  size_t num_events; // Number of events reflected in timers. See timers_update.
};

extern RV_CTimers rv_ctimers;

#endif
