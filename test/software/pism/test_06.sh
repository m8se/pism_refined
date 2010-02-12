#!/bin/bash

source ../functions.sh

test="Test #6: bootstrapping from files with symmetric and non-symmetric x- and y-variable ranges."
files="foo.nc bar.nc baz.nc"
dir=`pwd`

OPTS="-boot_from foo.nc -Mx 11 -My 11 -Mz 11 -Mbz 1 -Lz 4000 -y 0 -surface constant"
test_06 ()
{
    cleanup

    set -e

    # Create a file to bootstrap from:
    run -n 1 pismv -test G -Lx 4000 -Ly 4000 -Mx 21 -My 21 -Mz 11 -Mbz 1 -y 0 -o foo.nc

    # Bootstrap with a symmetric range:
    run -n 1 pismr $OPTS -o bar.nc
    # Change the range:
    run ncap2 -O -s"\"x=x+1e4;y=y+1e4\"" foo.nc foo.nc
    # Bootstrap with a non-symmetric range:
    run -n 1 pismr $OPTS -o baz.nc

    set +e

    # Check:
    run nccmp.py -t 1e-16 -x -v x,y bar.nc baz.nc
    if [ $? != 0 ];
    then
	fail "files bar.nc and baz.nc are different"
	return 1
    fi

    pass
    return 0
}

test_06
