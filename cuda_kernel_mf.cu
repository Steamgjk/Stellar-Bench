#include "cuda_kernel_mf.h"
__global__ void cuda_mf(double *row_eles, double* col_eles, int dim, int row_height, int col_height, double* rating_entries, int* random_row_id, int* random_col_id, int wn){
	int tid =  threadIdx.x;
	int r_rx = random_row_id[tid];
	int r_cx = random_col_id[tid];
	int i, j, k;
	double yita = 0.2;
	double theta = 0.2;
	double err = 0;
	for( i = r_rx; i < row_height; i+= wn){
		for(j = r_cx; j < col_height; j+= wn ){
			err = rating_entries[i*col_height+j];
			if(err == 0){
				continue;
			}
			for(k = 0; k < dim; k++){
				err -= row_eles[i*dim+k] * col_eles[j*dim+k];
			}

	        for (k = 0; k < dim; ++k)
	        {
	        	row_eles[i*dim+k] += yita * (err * col_eles[j*dim+k] - theta * row_eles[i*dim+k]);

	            col_eles[j*dim+k] += yita * (err * row_eles[i*dim+k] - theta* col_eles[j*dim+k]);
	        }
		}
	}
}