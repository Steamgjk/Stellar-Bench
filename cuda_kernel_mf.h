#ifndef CUDA_KERNEL_H
#define CUDA_KERNEL_H
#include <stdio.h>
#include <cuda.h>
#include <stdlib.h>
#include <algorithm>
__global__ void cuda_mf(double *row_eles, double* col_eles, int dim, int row_height, int col_height, double* rating_entries, int* random_row_id, int* random_col_id, int wn);
#endif