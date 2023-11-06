__kernel void vadd(__global long int *A, __global long int *B, __global long int *C) {int i = get_global_id(0);C[i] = A[i] + B[i];}
