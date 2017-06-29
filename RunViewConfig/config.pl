#!/usr/bin/perl -W

$r = int( rand 2000000000 );

$tfile = "/tmp/runview-papi-$$-$r.cc";

open T, ">$tfile" || die "Could not open $tfile for output.\n";

print T <<"EOS;";
#include <papi.h>
int main(int argc, char **argv)
{ PAPI_library_init(PAPI_VER_CURRENT); return 1; }
EOS;
close T;

$cmd = "g++ $tfile -o /dev/null -lpapi &> /dev/null ";
$exit_status = system $cmd;
$exit_val = $exit_status / 256;

unlink $tfile;

print <<"EOS;" if $exit_val == 0;
BEGIN MAKE_DEFINITION
CXXFLAGS += -DHAVE_PAPI
END MAKE_DEFINITION
LIBRARY -lpapi
EOS;

