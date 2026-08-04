/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

__kernel void simple_kernel_0(
    const uint arg0,
    __global uint *dst) {

    uint idx = get_global_id(0);

    dst[idx] = arg0;
}

__kernel void simple_kernel_1(
    __global uint *dst) {
    dst[get_global_id(0)] = 0;
}

__kernel void simple_kernel_2(__global uint *dst) {
    uint offset = get_max_sub_group_size() * get_sub_group_id();
    dst[get_sub_group_local_id() + offset] = get_local_id(0);
}

__kernel void simple_kernel_3(__global uint *dst, uint incrementationsCount) {
    uint groupIdX = get_group_id(0);
    uint groupIdY = get_group_id(1);
    uint groupIdZ = get_group_id(2);

    uint groupCountX = get_num_groups(0);
    uint groupCountY = get_num_groups(1);
    uint groupCountZ = get_num_groups(2);

    uint destination = groupIdZ * groupCountY * groupCountX + groupIdY * groupCountX + groupIdX;

    for (uint i = 0; i < incrementationsCount; i++) {
        dst[destination]++;
    }
}

__kernel void simple_kernel_4(
    __global const uint *src,
    const uint arg1,
    __global uint *dst) {

    uint idx = get_global_id(0);

    dst[idx] = src[idx] + arg1;
}

__kernel void simple_kernel_5(__global uint *dst) {
    // first uint holds the total work item count
    atomic_inc(dst);
    uint groupIdX = get_group_id(0);
    uint groupIdY = get_group_id(1);
    uint groupIdZ = get_group_id(2);

    uint groupCountX = get_num_groups(0);
    uint groupCountY = get_num_groups(1);
    uint groupCountZ = get_num_groups(2);

    __global uint *groupCounters = dst + 1;
    // store current group position in 3D array
    uint destination = groupIdZ * groupCountY * groupCountX + groupIdY * groupCountX + groupIdX;
    atomic_inc(&groupCounters[destination]);
}

#define SIMPLE_KERNEL_6_ARRAY_SIZE 32
__kernel void simple_kernel_6(__global uint *dst, __constant uint2 *src, uint scalar, uint maxIterations, uint maxIterations2) {
    __private uint2 array[SIMPLE_KERNEL_6_ARRAY_SIZE];
    __private uint2 sum;
    __private size_t gid = get_global_id(0);
    __private size_t lid = get_local_id(0);

    __private uint multi = 1;
    if (lid == 1024) {
        multi = 4;
    }
    sum = (uint2)(0, 0);

    for (int i = 0; i < maxIterations; ++i) {
        array[i] = src[i] + (uint2)(i * multi, i * multi + scalar);
    }

    for (int i = 0; i < maxIterations2; ++i) {
        sum.x = array[i].x + sum.x;
        sum.y = array[i].y + sum.y;
    }

    vstore2(sum, gid, dst);
}

__kernel void test_get_global_size(__global int *dst) {
    int tid = get_global_id(0);
    int n = get_global_size(0);

    dst[tid] = n;
};

__kernel void test_printf_number(__global uint *in) {
    printf("in[0] = %d\n", in[0]);
}
__kernel void simple_arg_int(int src, __global int *dst) {
    int id = (int)get_global_id(0);
    dst[id] = src;
}
