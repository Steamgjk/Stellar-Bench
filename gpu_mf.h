#ifndef GPU_MF_H
#define GPU_MF_H
#include "cuda_kernel_mf.h"
void gpu_mf(double *row_eles, double* col_eles, int dim, int row_height, int col_height, double* rating_entries, int* random_row_id, int* random_col_id, int wn);
#endif