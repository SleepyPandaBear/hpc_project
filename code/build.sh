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
if [[ "$arg" == "openmpi" ]]; then
    jumpto openmpi
fi
if [[ "$arg" == "openmpi_arnes" ]]; then
    jumpto openmpi_arnes
fi
if [[ "$arg" == "openmp" ]]; then
    jumpto openmp
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
	echo "  - openmpi"
	echo "    Build openmpi version of the model."
jumpto end

basic:
    gcc -Wall snow_crystal_growth_model.cpp -lm -fopenmp -o ../build/snow_crystal_growth_model -lstdc++
    #gcc -Wall snow_crystal_growth_model.cpp -DSAVE_DURING_ITERATIONS -lm -fopenmp -o ../build/snow_crystal_growth_model_iterations -lstdc++
jumpto end

cuda:
    module load CUDA/10.1.243-GCC-8.3.0
	nvcc snow_crystal_growth_model_cuda.cu --compiler-options -Wall -noeh -O2 -o ../build/snow_crystal_growth_model_cuda
	#nvcc snow_crystal_growth_model_cuda.cu --compiler-options -DSAVE_DURING_ITERATIONS -Wall -noeh -O2 -o ../build/snow_crystal_growth_model_cuda_iterations
jumpto end

openmpi:
    #module load mpi/openmpi-4.1.3
    module load OpenMPI/4.1.0-GCC-10.2.0
    srun --mpi=pmix --reservation=fri --time=00:05:00 mpic++ snow_crystal_growth_model_openmpi.cpp -lm -o ../build/snow_crystal_growth_model_openmpi
    #srun --mpi=pmix --reservation=fri --time=00:05:00 mpic++ -DSAVE_DURING_ITERATIONS snow_crystal_growth_model_openmpi.cpp -lm -o ../build/snow_crystal_growth_model_openmpi_iterations
jumpto end

openmpi_arnes:
    module load mpi/openmpi-4.1.3
    srun --reservation=fri-vr --partition=gpu --time=00:05:00 mpic++ -Wall snow_crystal_growth_model_openmpi.cpp -lm -o ../build/snow_crystal_growth_model_openmpi
    #srun --reservation=fri-vr --partition=gpu --time=00:05:00 mpic++ -Wall -DSAVE_DURING_ITERATIONS snow_crystal_growth_model_openmpi.cpp -lm -o ../build/snow_crystal_growth_model_openmpi_iterations
    #srun --mpi=pmix --reservation=fri-vr --partition=gpu --time=00:05:00 mpicc snow_crystal_growth_model_openmpi.cpp -lm -o ../build/snow_crystal_growth_model_openmpi
    #srun --mpi=pmix --reservation=fri-vr --partition=gpu --time=00:05:00 mpicc -DSAVE_DURING_ITERATIONS snow_crystal_growth_model_openmpi.cpp -lm -o ../build/snow_crystal_growth_model_openmpi_iterations
jumpto end

openmp:
    gcc -Wall snow_crystal_growth_model_openmp.cpp -lm -fopenmp -o ../build/snow_crystal_growth_model_openmp -lstdc++
    #gcc -Wall -DSAVE_DURING_ITERATIONS snow_crystal_growth_model_openmp.cpp -lm -fopenmp -o ../build/snow_crystal_growth_model_openmp_iterations -lstdc++
jumpto end

end:
