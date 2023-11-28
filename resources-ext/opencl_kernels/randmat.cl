__kernel void randMat(__global unsigned int* seed, __global unsigned int* width, __global unsigned int* height, write_only image2d_t matrix) {
    int globalID_x = get_global_id(0);
    int globalID_y = get_global_id(1);

    if (globalID_x >= 10) {
        printf("out of bounds\n");
    }
    if (globalID_y >= 20) {
        printf("out of bounds\n");
    }

    float seeded = (float) 10.0;

    write_imagef(matrix, (int2)(globalID_x, globalID_y), (float4)(seeded, 0 ,0 ,0));
}
