#include <cctk.h>
#include "ctimers.h"
#include "util.h"


RV_Timer_Event::RV_Timer_Event(int idx, RV_Event_Type et):
    event_time_s(time_wall_fp()),timer_index(idx),event_type(et)
{
}

void
RV_CTimers::init()
{
  assert( this == &rv_ctimers );

  static const char* const clock_name = "RunView";

  cClockFuncs funcs;
  funcs.n_vals = 1;

  funcs.create = [](int idx) -> void* { return NULL; };
  funcs.destroy = [](int idx, void *data){ };
  funcs.set = [](int idx, void *data, cTimerVal *tv){ assert( false ); };
  funcs.get = [](int idx, void *data, cTimerVal *tv)
    {
      rv_ctimers.timers_update();
      const RV_CTimer& t = rv_ctimers.timers[idx];
      tv->val.d = tv->seconds = t.duration_s;
      tv->type = val_double;
      tv->units = "s";
      tv->heading = clock_name; // Must be name of clock.
    };

#define MAKE_FUNC(et)                                                   \
  [](int idx, void *data){ rv_ctimers.events.emplace_back(idx,et); }
  funcs.start = MAKE_FUNC(RET_Timer_Start);
  funcs.stop = MAKE_FUNC(RET_Timer_Stop);
  funcs.reset = MAKE_FUNC(RET_Timer_Reset);
#undef MAKE_FUNC

  funcs.seconds =
     [](int idx, void *data, cTimerVal *tv)
     {
       assert( false ); // Congratulations, you've found a test case!
       return 0.0;
     };

  CCTK_ClockRegister(clock_name, &funcs);
}

RV_CTimer&
RV_CTimers::get(int cactus_timer_index)
{
  timers_update();
  return timers[cactus_timer_index];
}

void
RV_CTimers::timers_update()
{
  const size_t n_events = events.size();
  if ( n_events == num_events ) return;
  const size_t ntimers = CCTK_NumTimers();
  assert( ntimers >= timers.size() );
  timers.resize(ntimers);
  while ( num_events < n_events )
    {
      const RV_Timer_Event& e = events[num_events++];
      assert( (unsigned int)(e.timer_index) < ntimers );
      RV_CTimer& t = timers[e.timer_index];
      switch ( e.event_type ) {
      case RET_Timer_Start:
        if ( t.event_last == RET_Timer_Start ) t.missequence_count++;
        else t.last_start_time_s = e.event_time_s;
        break;
      case RET_Timer_Stop:
        if ( t.event_last != RET_Timer_Start ) t.missequence_count++;
        else t.duration_s += e.event_time_s - t.last_start_time_s;
        break;
      case RET_Timer_Reset: t.duration_s = 0; break;
      default: assert( false ); }
      t.event_last = e.event_type;
      t.event_count[e.event_type]++;
    }
}
