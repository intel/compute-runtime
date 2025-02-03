/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__attribute__((reqd_work_group_size(32, 1, 1))) // force LWS to 32
__attribute__((intel_reqd_sub_group_size(16))) // force SIMD to 16
__kernel void spillFillKernelLargeGRF(__global int *resIdx, global long16 *src, global long16 *dst){
    size_t lid = get_local_id(0);
    size_t gid = get_global_id(0);

    long16 res1 = src[gid];
    long16 res2 = src[gid] + 1 - res1;
    long16 res3 = src[gid] + 2;

    __local long16 locMem[16];
    locMem[lid] = res1;
    barrier(CLK_LOCAL_MEM_FENCE);
    barrier(CLK_GLOBAL_MEM_FENCE);
    dst[gid] = (locMem[resIdx[gid]]*res3) + res2;
}
