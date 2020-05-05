/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

const sampler_t sampler =
    CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP | CLK_FILTER_NEAREST;

kernel void test(const global float *a, const global float *b,
                 global float *c,
                 read_only image2d_t input,
                 write_only image2d_t output,
                 sampler_t sampler) {
    const int global_id = get_global_id(0);
    const int local_id = get_local_id(0);

    local float a_local[16];
    float sum = 0.0f;

    a_local[local_id] = a[local_id] + b[local_id];

    barrier(CLK_LOCAL_MEM_FENCE);

    for (int i = 0; i < get_local_size(0); ++i) {
        sum += a_local[i];
    }
    barrier(CLK_LOCAL_MEM_FENCE);

    c[global_id] = sum;

    int2 coord = {get_global_id(0), get_global_id(1)};

    printf("local_id = %d, global_id = %d \n", local_id, global_id);
}