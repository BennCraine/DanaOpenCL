__kernel void relu(__global double* activations) {
    int gid = get_global_id(0);

    if (!(activations[gid] > 0.0)) {
        activations[gid] = 0.0;
    }
}
