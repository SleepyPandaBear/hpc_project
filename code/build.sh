#!/usr/bin/sh

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
    jumpto help
fi
if [[ "$arg" == "basic" ]]; then
    jumpto basic
fi
if [[ "$arg" == "cuda" ]]; then
    jumpto cuda
fi
if [[ "$arg" == "help" ]]; then
    jumpto help
fi
jumpto end

help:
	echo "You can run next commands:"
	echo "  - basic"
	echo "    Build basic version of the model."
	echo "  - cuda"
	echo "    Build cuda version of the model."
jumpto end

basic:
    gcc -Wall snow_crystal_growth_model.cpp -lm -fopenmp -o ../build/snow_crystal_growth_model -lstdc++
jumpto end

cuda:
    module load CUDA/10.1.243-GCC-8.3.0
	nvcc snow_crystal_growth_model_cuda.cu --compiler-options -Wall -noeh -O2 -o ../build/snow_crystal_growth_model_cuda
jumpto end

end:
