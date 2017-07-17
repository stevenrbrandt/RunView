// -*- c++ -*-
#ifndef CTIMERS_H
#define CTIMERS_H

#include <vector>
#include "rv_papi.h"

enum RV_Event_Type
  { RET_Unset, RET_Timer_Start, RET_Timer_Stop, RET_Timer_Reset, RET_ENUM_SIZE};

struct RV_Timer_Event {
  RV_Timer_Event(RV_PAPI_Sample *ps, int idx, RV_Event_Type et);
  RV_Timer_Event(int64_t etime, int idx, RV_Event_Type et);
  union {
    RV_PAPI_Sample *papi_sample; // If have_papi_sample true;
    int64_t etime;               // either cycles or nanoseconds.
  };
  papi_long etime_get() const
  { return have_papi_sample ? papi_sample->cyc : etime; }
  double etime_s_get() const;
  const int timer_index;
  const RV_Event_Type event_type;
  const bool have_papi_sample;
};

class RV_CTimer {
public:
  RV_CTimer()
    :event_last(RET_Unset), event_count(RET_ENUM_SIZE), duration_s(0),
     h_last_start_event(NULL), u_last_start_event(NULL), n_papi_samples(0),
     missequence_count(0){}
  RV_Event_Type event_last;
  std::vector<int> event_count;
  double duration_s;
  int64_t duration;         // Either cycles or nanoseconds.
  const RV_Timer_Event *h_last_start_event, *u_last_start_event;
  RV_Event_Type papi_event_last;
  int n_papi_samples;
  RV_PAPI_Sample papi_sample_at_start;
  RV_PAPI_Sample papi_sample;
  int missequence_count; // E.g., double start, stop when not running.
};

class RV_CTimers {
public:
  RV_CTimers(){
    num_events = 0; n_papi_samples = 0; pm = NULL;
    event_sec_per_unit = 0;
  }

  void init();
  void papi_init(std::vector<int> papi_events);
  void handle_timer_event(int timer_index, RV_Event_Type et);
  void timers_update();
  RV_CTimer& get(int cactus_timer_index);
  double event_sec_per_unit_get(){ return event_sec_per_unit; }

  std::vector<RV_Timer_Event> events;
  int64_t epoch_ns;
  int n_papi_samples;  // Number of RV_PAPI_Sample pointers in events.
private:
  std::vector<RV_CTimer> timers;
  RV_PAPI_Manager* pm;
  double event_sec_per_unit;
  size_t num_events; // Number of events reflected in timers. See timers_update.
};

extern RV_CTimers rv_ctimers;

inline double
RV_Timer_Event::etime_s_get() const
{ return rv_ctimers.event_sec_per_unit_get() * etime_get(); }


#endif
