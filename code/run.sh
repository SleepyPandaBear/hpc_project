#!/usr/bin/sh

function jumpto
{
    label=$1
    cmd=$(sed -n "/$label:/{:a;n;p;ba};" $0 | grep -v ':$')
    eval "$cmd"
    exit
}

arg=$1
alpha=$2
beta=$3
gamma=$4

start=${1:-"start"}

jumpto $start

start:
if [[ "$#" -eq 1 ]]; then
    jumpto help
fi
if [[ "$arg" == "basic" ]]; then
    jumpto basic
fi
if [[ "$arg" == "help" ]]; then
    jumpto help
fi
jumpto end

basic:
    #if [[ "$#" -ne 5 ]]; then
    #    jumpto help
    #fi
    echo "$0"
    echo "$1"
    echo "$alpha"
    echo "$beta"
    echo "$gamma"
    srun --reservation=fri --cpus-per-task=1 --ntasks=1 --time=00:20:00 ../build/snow_crystal_growth_model $alpha $beta $gamma
    #pushd ../build/
    #./snow_crystal_growth_model $2 $3 $4
    #popd
jumpto end

help:
	echo "You can run next commands:"
	echo "  - basic"
	echo "    Run basic version of the model."
jumpto end


end:
