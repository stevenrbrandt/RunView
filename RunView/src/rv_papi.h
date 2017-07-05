// -*- c++ -*-

#ifndef RV_PAPI_H
#define RV_PAPI_H

#include <stdio.h>
#include <vector>
#include <map>
#include <cctk.h>

typedef long long papi_long;

#ifdef HAVE_PAPI

#include <papi.h>

#define PE( c )                                                               \
{                                                                             \
  const int __pe_rv = c;                                                      \
  if ( __pe_rv < PAPI_OK )                                                    \
    CCTK_VWarn                                                                \
      (CCTK_WARN_ABORT, __LINE__,__FILE__, CCTK_THORNSTRING,                  \
       "Internal error using PAPI API: %s\n", PAPI_strerror(__pe_rv) );       \
}
#else

#define PE( c )
inline papi_long PAPI_get_real_cyc(){ return 0; }

// WARNING: Values are meaningless. They are here to avoid compilation errors.
enum { PAPI_OK = 0, PAPI_NULL = 0,
    PAPI_TOT_INS, PAPI_L3_TCM, PAPI_PRF_DM, PAPI_TLB_DM };

#endif

class RV_PAPI_Sample {
public:
  RV_PAPI_Sample():cyc(0){};
  RV_PAPI_Sample(papi_long c, int n_events):cyc(c),values(n_events){};
  RV_PAPI_Sample(const RV_PAPI_Sample& s):cyc(s.cyc),values(s.values){};
  RV_PAPI_Sample(papi_long c, std::vector<papi_long>&& vals):
    cyc(c),values(vals){};
  bool filled() { return !values.empty(); }
  inline bool available(int papi_event) const;
  inline papi_long operator [](int papi_event) const;
  papi_long* values_ptr() { return values.data(); }
  void operator +=(const RV_PAPI_Sample& s)
    {
      if ( s.values.empty() ) return;
      if ( values.empty() ) { *this = s;  return; }
      cyc += s.cyc;
      for ( int i=0; i<values.size(); i++ ) values[i] += s.values[i];
    }
  void accumulate(RV_PAPI_Sample&& at_end, RV_PAPI_Sample& at_start)
    {
      if ( values.empty() ) values.resize( at_end.values.size() );
      cyc += at_end.cyc - at_start.cyc;
      for ( int i=0; i<at_end.values.size(); i++ )
        values[i] += at_end.values[i] - at_start.values[i];
    }
  void reset()
    {
      cyc = 0;
      values.clear();
    }
  papi_long cyc;
private:
  std::vector<papi_long> values;
};


class RV_PAPI_Manager {
public:

  RV_PAPI_Manager(){ eset = PAPI_NULL; ncounters = -1; }

  int ncounters;
  int eset;
  std::vector<int> events;
  std::map<int,int> event_to_idx;
  int n_events;

  papi_long cyc_start;

  int n_samples;
  std::vector<papi_long> samples;

  int periodic_period;

  void setup(std::vector<int> eventsp);

  RV_PAPI_Sample sample_get()
  {
    RV_PAPI_Sample rv(PAPI_get_real_cyc(),n_events);
    PE( PAPI_read(eset, rv.values_ptr()) );
    return rv;
  }

  int ncallbacks;
  void callback(int eset, void *pc);
  void destroy();

};

extern RV_PAPI_Manager rv_papi_manager;

inline papi_long
RV_PAPI_Sample::operator [](int papi_event) const
{
  if ( !available(papi_event) ) return 0;
  auto idxi = rv_papi_manager.event_to_idx.find(papi_event);
  return values[idxi->second];
}

inline bool
RV_PAPI_Sample::available(int papi_event) const
{
  if ( values.empty() ) return false;
  auto idxi = rv_papi_manager.event_to_idx.find(papi_event);
  return idxi != rv_papi_manager.event_to_idx.end();
}

#endif
