/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma OPENCL EXTENSION cl_khr_global_int32_base_atomics : enable

__kernel void simple_kernel_0(
    const uint arg0,
    const float arg1,
    __global uint *dst) {

    uint idx = get_global_id(0);
    uint data = arg0 + (uint)arg1;

    dst[idx] = data;
}

__kernel void simple_kernel_1(
    __global uint *src,
    const uint arg1,
    __global uint *dst) {

    uint idx = get_global_id(0);

    dst[idx] = src[idx] + arg1;
}

__kernel void simple_kernel_2(
    const uint arg0,
    __global uint *dst) {

    uint idx = get_global_id(0);

    dst[idx] = arg0;
}

__kernel void simple_kernel_3(
    __global uint *dst) {
    dst[get_global_id(0)] = 0;
}

__kernel void simple_kernel_4() {
}

__kernel void simple_kernel_5(__global uint *dst) {
    //first uint holds the total work item count
    atomic_inc(dst);
    uint groupIdX = get_group_id(0);
    uint groupIdY = get_group_id(1);
    uint groupIdZ = get_group_id(2);
    
    uint groupCountX = get_num_groups(0);
    uint groupCountY = get_num_groups(1);
    uint groupCountZ = get_num_groups(2);

    __global uint* groupCounters = dst+1;
    //store current group position in 3D array
    uint destination = groupIdZ * groupCountY * groupCountX + groupIdY * groupCountX + groupIdX;
    atomic_inc(&groupCounters[destination]);
}

#define SIMPLE_KERNEL_6_ARRAY_SIZE 256
__kernel void simple_kernel_6(__global uint *dst, __global uint2 *src, uint scalar, uint maxIterations, uint maxIterations2) {
    __private uint2 array[SIMPLE_KERNEL_6_ARRAY_SIZE];
    __private uint2 sum;
    __private size_t gid = get_global_id(0);
    __private size_t lid = get_local_id(0);

    __private uint multi = 1;
    if(lid == 1024) {
        multi = 4;
    }
    sum = (uint2)(0, 0);

    for(int i = 0; i < maxIterations; ++i) {
        array[i] = src[i] + (uint2)(i*multi, i*multi+scalar);
    }

    for(int i = 0; i < maxIterations2; ++i) {
        sum.x = array[i].x + sum.x;
        sum.y = array[i].y + sum.y;
    }

    vstore2(sum, gid, dst);
}

typedef long16 TYPE;
__attribute__((reqd_work_group_size(32, 1, 1))) // force LWS to 32
__attribute__((intel_reqd_sub_group_size(8))) // force SIMD to 8
__kernel void simple_kernel_7(__global int *resIdx, global TYPE *src, global TYPE *dst){
    size_t lid = get_local_id(0);
    size_t gid = get_global_id(0);

    TYPE res1 = src[gid*3];
    TYPE res2 = src[gid*3+1];
    TYPE res3 = src[gid*3+2];

    __local TYPE locMem[32];
    locMem[lid] = res1;
    barrier(CLK_LOCAL_MEM_FENCE);
    barrier(CLK_GLOBAL_MEM_FENCE);
    TYPE res = (locMem[resIdx[gid]]*res3)*res2 + res1;

    dst[gid] = res;
}

__kernel void simple_kernel_8(__global uint *dst, uint incrementationsCount) {
    uint groupIdX = get_group_id(0);
    uint groupIdY = get_group_id(1);
    uint groupIdZ = get_group_id(2);

    uint groupCountX = get_num_groups(0);
    uint groupCountY = get_num_groups(1);
    uint groupCountZ = get_num_groups(2);

    uint destination = groupIdZ * groupCountY * groupCountX + groupIdY * groupCountX + groupIdX;

    for(uint i = 0; i < incrementationsCount; i++){
        dst[destination]++;
    }
}
