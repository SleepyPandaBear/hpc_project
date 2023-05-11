#!/usr/bin/bash

function jumpto
{
    label=$1
    cmd=$(sed -n "/$label:/{:a;n;p;ba};" $0 | grep -v ':$')
    eval "$cmd"
    exit
}

arg=$1

start=${1:-"start"}

jumpto $start

start:
if [[ "$#" -eq 1 ]]; then
    jumpto basic
fi
if [[ "$2" == "basic" ]]; then
    jumpto basic
fi
if [[ "$2" == "help" ]]; then
    jumpto help
fi
jumpto end

help:
	echo "You can run next commands:"
	echo "  - basic"
	echo "    Build basic version of the model."
jumpto end

basic:
    gcc -Wall snow_crystal_growth_model.cpp -lm -o ../build/snow_crystal_growth_model -lstdc++
jumpto end

end:
