#!/bin/bash

#   Creates a credible initial state for the EISMINT-Greenland experiments.
# Uses .nc files generated by preprocess.sh.  Continue using ssl2.sh.
# See PISM User's Manual.

#   Recommended way to run with 2 processors is "./bootstrap.sh 2 >& out.boot &"
# which gives a transcript.

NN=2  # default number of processors
if [ $# -gt 0 ] ; then  # if user says "bootstrap.sh 8" then NN = 8
  NN="$1"
fi
set -e  # exit on error

PGRN="pismr -e 3 -ocean_kill -atmosphere eismint_greenland -surface pdd"

echo ""
echo "BOOTSTRAP.SH: running pismr on eis_green_smoothed.nc for 1 year to smooth surface;"
echo "  creates green20km_y1.nc ..."
mpiexec -n $NN $PGRN -boot_file eis_green_smoothed.nc -Mx 83 -My 141 -Lz 4000 -Mz 51 -Lbz 2000 -Mbz 21 -skip 1 -y 1 -o green20km_y1.nc

NMSPINUP=25000   # number of years to give rough -no_mass temp equilibrium

echo ""
echo "BOOTSTRAP.SH: running pismr on green20km_y1.nc for $NMSPINUP years with fixed surface"
echo "  to equilibriate temps; creates green20km_Tsteady.nc ..."
mpiexec -n $NN $PGRN -i green20km_y1.nc -no_mass -y $NMSPINUP -o green20km_Tsteady.nc
