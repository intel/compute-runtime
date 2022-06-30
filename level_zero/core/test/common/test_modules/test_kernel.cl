/*
 * Copyright (C) 2020-2022 Intel Corporation
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

__kernel void test_get_global_sizes(__global uint *outGlobalSize) {
    outGlobalSize[0] = get_global_size(0);
    outGlobalSize[1] = get_global_size(1);
    outGlobalSize[2] = get_global_size(2);
}

__kernel void test_get_work_dim(__global uint *outWorkDim) {
    outWorkDim[0] = get_work_dim();
}
__kernel void test_get_group_count(__global uint *outGroupCount) {
    outGroupCount[0] = get_num_groups(0);
    outGroupCount[1] = get_num_groups(1);
    outGroupCount[2] = get_num_groups(2);
}

kernel void memcpy_bytes(__global char *dst, const __global char *src) {
    unsigned int gid = get_global_id(0);
    dst[gid] = src[gid];
}

kernel __attribute__((work_group_size_hint(1, 1, 1))) void memcpy_bytes_attr(__global char *dst, const __global char *src) {
    unsigned int gid = get_global_id(0);
    dst[gid] = src[gid];
}
