__kernel void vaddi(__global long unsigned int *A, __global long unsigned int *B, __global long unsigned int *C) {
    //this is a comment
    int i = get_global_id(0);
    C[i] = A[i] + B[i];
    /*

    this is a block comment
    second line bishhhhh
    */
}
