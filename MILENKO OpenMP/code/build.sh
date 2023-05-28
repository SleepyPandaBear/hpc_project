#!/usr/bin/bash
#SBATCH --job-name=project_snowflake
#SBATCH --time=00:20:00
#SBATCH --reservation=fri
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=64  
#SBATCH --output=project_snowflake.txt

export OMP_PLACES=cores
export OMP_PROC_BIND=TRUE
export OMP_NUM_THREADS=64

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
    gcc -O2 -Wall snow_crystal_growth_model.cpp -lm -fopenmp -o snow_crystal_growth_model -lstdc++
    srun snow_crystal_growth_model 1.0 0.5 0.001 >> results_snowflake_OpenMP.txt
jumpto end

end:
