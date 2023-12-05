__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;

__kernel void weightedSum(__global double* b, read_only image2d_t matrixOne, read_only image2d_t matrixTwo, write_only image2d_t z) {
    int gidx = get_global_id(0);
    int gidy = get_global_id(1);

    double ws = 0.0;
    for (int i = 0; i < get_image_height(matrixOne); i++) {
        int diff = gidx - gidy;
        float vFour = read_imagef(matrixOne, sampler, int2(gidx, i));
        float a = vFour[0];
        vFour = read_imagef(matrixTwo, sampler, int2(gidx, gidy));
        float w = vFour[0];
        float ws += a*w;
    }
    
    z[gidx][gidy] = ws + b[gidx];
}
