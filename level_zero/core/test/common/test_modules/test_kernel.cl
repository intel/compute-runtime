/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void test_pointer_by_value(long address) {
    if (address) {
        ((__global int *)address)[get_global_id(0)] += 1;
    }
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

__kernel void test_get_local_id(__global uint *output) {
    unsigned int groupSizeX = get_global_size(0);
    unsigned int groupSizeY = get_global_size(1);

    unsigned int globalIdX = get_global_id(0);
    unsigned int globalIdY = get_global_id(1);
    unsigned int globalIdZ = get_global_id(2);

    unsigned int globalIndex = globalIdX + globalIdY * groupSizeX + globalIdZ * groupSizeX * groupSizeY;
    
    output[globalIndex] = get_local_size(0);
}

kernel void memcpy_bytes(__global char *dst, const __global char *src) {
    unsigned int gid = get_global_id(0);
    dst[gid] = src[gid];
}
