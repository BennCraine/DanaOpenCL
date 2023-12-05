__constant sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_NONE | CLK_FILTER_NEAREST;

__kernel void chopColumnI(__global unsigned int* startEnd, read_only image2d_t in, write_only image2d_t out) {
    int gidx = get_global_id(0);
    int gidy = get_global_id(1);

    unsigned int diff = startEnd[1] - startEnd[0];
    
    if (gidx >= startEnd[0] && gidx < startEnd[1]) {
        //do nothing
    }
    else if (gidx < startEnd[0]) {
        //read from in 
        float4 vfour = read_imagef(in, sampler, (int2)(gidx, gidy));
        float pix = vfour[0];
        //write to out in same index
        write_imagef(out, (int2)(gidx, gidy), (float4)(pix, 0 ,0 ,0));
    }
    else { //gidx >= startEnd[1]
        //read from in 
        float4 vfour = read_imagef(in, sampler, (int2)(gidx, gidy));
        float pix = vfour[0];
        //write to out at gidx + (end-start) 
        write_imagef(out, (int2)(gidx + diff, gidy), (float4)(pix, 0 ,0 ,0));
    }
}
