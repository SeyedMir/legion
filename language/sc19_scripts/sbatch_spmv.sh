#!/bin/bash -eu
#SBATCH --nodes=256
#SBATCH --constraint=gpu
#SBATCH --time=00:40:00

ROOT_DIR="$PWD"

export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}:$LEGION_DIR/bindings/regent/"
if [[ ! -z "${HDF_ROOT:-}" ]]; then
    export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}:$HDF_ROOT/lib"
fi

if [[ ! -d auto ]]; then mkdir auto; fi
pushd auto

for n in {1,2,4,8,16,32,64,128,256}; do
  echo "Running $n"
  srun -n $n -N $n --ntasks-per-node 1 --cpu_bind none "$ROOT_DIR/spmv.auto" -p $n -size $(( 8*n )) -ll:gpu 1 -ll:util 1 -ll:dma 2 -ll:csize 15000 -ll:fsize 15000 -ll:rsize 512 -ll:gsize 0 | tee out_"$n".log
done

popd
