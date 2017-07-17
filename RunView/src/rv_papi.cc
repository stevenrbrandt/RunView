#include <map>
#include <algorithm>
#include <iostream>
#include <assert.h>
#include <cctk.h>
#include "rv_papi.h"

using namespace std;

void overflow_handler(int eset, void *pc, papi_long ovector, void *ctx);

RV_PAPI_Manager rv_papi_manager;

void
RV_PAPI_Manager::setup(vector<int> eventsp)
{
  periodic_period = 0;

# ifdef HAVE_PAPI

  PAPI_library_init(PAPI_VER_CURRENT);

  ncounters = PAPI_num_counters();
  ncallbacks = 0;

  // Low-Level API
  PE( PAPI_create_eventset( &eset ) );

  for ( auto e: eventsp )
    {
      const int rv = PAPI_add_event( eset, e );
      if ( rv >= PAPI_OK ) continue;
      PAPI_event_info_t ei;
      PE( PAPI_get_event_info( e, &ei ) );
      CCTK_VInfo
        (CCTK_THORNSTRING,
         "Could not initialize PAPI event %#x %s: %s\n",
         e, ei.symbol, PAPI_strerror(rv));
    }

  if ( periodic_period )
    PE( PAPI_overflow
        ( eset, PAPI_TOT_INS, periodic_period, 0, overflow_handler) );

  int n2 = n_events = PAPI_num_events( eset );
  events.resize(n_events);
  PE( PAPI_list_events( eset, events.data(), &n2 ) );
  assert( n_events == n2 );

  for ( int i=0; i<n_events; i++ ) event_to_idx[events[i]] = i;

  vector<int> ebad;
  for ( auto e: eventsp )
    if ( event_to_idx.find(e) == event_to_idx.end() ) ebad.push_back(e);

  if ( periodic_period )
    samples.reserve((1+n_events)*100);

  PE( PAPI_start( eset ) );
  cyc_start = PAPI_get_real_cyc();

# endif
}

double
RV_PAPI_Manager::cpu_clock_max_hz_get()
{
  const PAPI_hw_info_t* const hw_info = PAPI_get_hardware_info();
  if ( !hw_info ) return 0;
  return hw_info->cpu_max_mhz * 1e6;
}

void
RV_PAPI_Manager::callback(int eset, void *pc)
{
  samples.push_back(PAPI_get_real_cyc());
  const size_t n_s = samples.size();
  if ( n_s + n_events > samples.capacity() )
    samples.reserve( n_s + 100 * ( 1 + n_events ) );
  samples.resize(n_s+n_events);
  PE( PAPI_accum(eset, samples.data() + n_s ) );
}

void
RV_PAPI_Manager::destroy()
{
  PE( PAPI_cleanup_eventset( eset ) );  eset = PAPI_NULL;
}

void
overflow_handler(int eset, void *pc, papi_long ovector, void *ctx)
{
  rv_papi_manager.callback(eset,pc);
}

