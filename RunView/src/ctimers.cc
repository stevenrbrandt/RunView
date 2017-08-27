#include <cctk.h>
#include "rv_papi.h"
#include "ctimers.h"
#include "util.h"

using namespace std;

RV_Timer_Event::RV_Timer_Event
(RV_PAPI_Sample *ps, int idx, RV_Event_Type et):
  papi_sample(ps), timer_index(idx), event_type(et), have_papi_sample(true){}
RV_Timer_Event::RV_Timer_Event
(int64_t etimep, int idx, RV_Event_Type et):
  etime(etimep), timer_index(idx), event_type(et), have_papi_sample(false){}

void
RV_CTimers::init()
{
  assert( this == &rv_ctimers );

  static const char* const clock_name = "RunView";

  epoch_ns = time_wall_ns();
  // Changed to clock period if papi_init called.
  event_sec_per_unit = 1e-9;

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
  [](int idx, void *data){ rv_ctimers.handle_timer_event(idx,et); }
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

void
RV_CTimers::papi_init(vector<int> papi_events)
{
  assert( !pm );
  assert(events.empty()); // Because pre-papi events would be ns, post are cyc.
  pm = &rv_papi_manager;
  pm->setup(papi_events);
  const double cpu_clock = pm->cpu_clock_max_hz_get();
  event_sec_per_unit = 1.0 / cpu_clock;
}

RV_CTimer&
RV_CTimers::get(int cactus_timer_index)
{
  timers_update();
  return timers[cactus_timer_index];
}

void
RV_CTimers::handle_timer_event(int timer_index, RV_Event_Type et)
{
  if ( !pm )
    {
      events.emplace_back( time_wall_ns()-epoch_ns, timer_index, et );
      return;
    }

  if ( timer_index >= timers.size() ) timers.resize(timer_index+1);
  RV_CTimer& t = timers[timer_index];

  const int limit_p_sample_per_timer = 1000;
  const int limit_p_sample_soft = 1;

  const bool event_keeps_sample =
    t.h_last_start_event
    || t.n_papi_samples < limit_p_sample_per_timer
    || n_papi_samples < limit_p_sample_soft;

  RV_PAPI_Sample papi_sample(pm->sample_get());

  if ( event_keeps_sample )
    {
      RV_PAPI_Sample* const ps_ptr = new RV_PAPI_Sample(move(papi_sample));
      events.emplace_back(ps_ptr,timer_index,et);
      n_papi_samples++;
      t.n_papi_samples++;
      t.h_last_start_event = et == RET_Timer_Start ? &events.back() : NULL;
      return;
    }

  events.emplace_back(papi_sample.cyc, timer_index, et );

  switch ( et ) {
  case RET_Timer_Start:
    if ( t.papi_event_last == RET_Timer_Start ) t.missequence_count++;
    else t.papi_sample_at_start = move(papi_sample);
    break;
  case RET_Timer_Stop:
    if ( t.papi_event_last != RET_Timer_Start ) t.missequence_count++;
    else t.papi_sample.accumulate(papi_sample,t.papi_sample_at_start);
    break;
  case RET_Timer_Reset:
    t.papi_sample.reset();
    break;
  default: assert( false ); }
  t.papi_event_last = et;
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
        else                                   t.u_last_start_event = &e;
        break;
      case RET_Timer_Stop:
        if ( t.event_last != RET_Timer_Start ) t.missequence_count++;
        else
          {
            if ( e.have_papi_sample )
              t.papi_sample.accumulate
                (*e.papi_sample,*t.u_last_start_event->papi_sample);
            else if ( !pm )
              t.duration += e.etime - t.u_last_start_event->etime;
          }
        break;
      case RET_Timer_Reset: t.duration = 0; break;
      default: assert( false ); }
      t.event_last = e.event_type;
      t.event_count[e.event_type]++;
    }
  for ( auto& t: timers )
    {
      if ( pm ) t.duration = t.papi_sample.cyc;
      t.duration_s = t.duration * event_sec_per_unit;
    }
}
