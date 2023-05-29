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
size=$5
max_iteration=$6

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
if [[ "$arg" == "openmpi" ]]; then
    jumpto openmpi
fi
if [[ "$arg" == "openmpi_arnes" ]]; then
    jumpto openmpi_arnes
fi
if [[ "$arg" == "openmp" ]]; then
    jumpto openmpi
fi
if [[ "$arg" == "help" ]]; then
    jumpto help
fi
jumpto end

basic:
    pushd ../build/
    #srun --reservation=fri --cpus-per-task=1 --ntasks=1 --time=00:20:00 ../build/snow_crystal_growth_model $alpha $beta $gamma $size $max_iteration
    srun --reservation=fri --cpus-per-task=1 --ntasks=1 --time=00:20:00 snow_crystal_growth_model $alpha $beta $gamma $size $max_iteration
    #./snow_crystal_growth_model $2 $3 $4
    popd
jumpto end

cuda:
    pushd ../build/
    srun --reservation=fri -G1 -n1 --time=00:20:00 ../build/snow_crystal_growth_model_cuda $alpha $beta $gamma $size $max_iteration
    popd
jumpto end

openmpi:
    pushd ../build/
    srun --mpi=pmix -n4 -N1 --reservation=fri --time=00:20:00 ../build/snow_crystal_growth_model_openmpi $alpha $beta $gamma $size $max_iteration
    popd
jumpto end

openmpi_arnes:
    srun --mpi=pmix -n4 -N1 --reservation=fri-vr --partition=gpu --time=00:20:00 ../build/snow_crystal_growth_model_openmpi $alpha $beta $gamma $size $max_iteration
jumpto end

openmp:
    pushd ../build/
    srun --cpus-per-task=32 --ntasks=1 --reservation=fri --time=00:20:00 ../build/snow_crystal_growth_model_openmp $alpha $beta $gamma $size $max_iteration
    popd
jumpto end

help:
	echo "You can run next commands:"
	echo "  - basic"
	echo "    Run basic version of the model."
jumpto end


end:
