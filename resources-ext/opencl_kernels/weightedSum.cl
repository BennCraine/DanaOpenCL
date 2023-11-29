__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;

__kernel void weightedSum(__global double* input, read_only image2d_t weights, __global double* biases, __global double* activations) {
    int gid = get_global_id(0);
    
    double sum = 0.0;
    for (int i = 0; i < get_image_height(weights); i++) {
        float4 vfour = read_imagef(weights, sampler, (int2)(gid, i));
        float w = vfour[0];
        sum += (input[i] * w);
    }

    sum += biases[gid];
    activations[gid] = sum;
}
