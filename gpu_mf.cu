#include <stdio.h>
#include <stdlib.h>
#include <cuda.h>
#include <algorithm>
#include "gpu_mf.h"
using namespace std;
void gpu_mf(double *row_eles, double* col_eles, int dim, int row_height, int col_height, double* rating_entries, int* random_row_id, int* random_col_id, int wn){
	double* d_row_eles, *d_col_eles, *d_rating_entries;
	int* d_random_row_id, *d_random_col_id;
	cudaMalloc((void**)&d_row_eles, sizeof(double)* row_height*dim);
	cudaMalloc((void**)&d_col_eles, sizeof(double)* col_height*dim);
	cudaMalloc((void**)&d_rating_entries, sizeof(double)* row_height*col_height);
	cudaMalloc((void**)&d_random_row_id, sizeof(int)* wn);
	cudaMalloc((void**)&d_random_col_id, sizeof(int)* wn);

	cudaMemcpy(d_row_eles, row_eles, sizeof(double)* row_height*dim,  cudaMemcpyHostToDevice);
	cudaMemcpy(d_col_eles, col_eles, sizeof(double)* col_height*dim,  cudaMemcpyHostToDevice);
	cudaMemcpy(d_rating_entries, rating_entries, sizeof(double)* row_height*col_height,  cudaMemcpyHostToDevice);

	int iter = 0;
	int iter_num = 100;
	int i = 0;
	for(iter = 0;  iter < iter_num; iter++){
		for( i = 0; i < wn; i++){
			random_row_id[i] = i;
			random_col_id[i] =i;
		}
		random_shuffle(random_row_id, random_row_id+wn);
		random_shuffle(random_col_id, random_col_id+wn);
		cudaMemcpy(d_random_row_id, random_row_id, sizeof(int)* wn, cudaMemcpyHostToDevice);
		cudaMemcpy(d_random_col_id, random_col_id, sizeof(int)* wn, cudaMemcpyHostToDevice);
		cuda_mf<<<1, wn>>>(d_row_eles, d_col_eles, dim,row_height,col_height, d_rating_entries, d_random_row_id, d_random_col_id, wn);
	}
	cudaMemcpy(row_eles, d_row_eles, sizeof(double)* row_height*dim, cudaMemcpyDeviceToHost);
	cudaMemcpy(col_eles, d_col_eles, sizeof(double)* col_height*dim, cudaMemcpyDeviceToHost);



}