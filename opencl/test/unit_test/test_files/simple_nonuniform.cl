/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void simpleNonUniform(int atomicOffset, __global volatile int *dst) {
    int id = (int)(get_global_id(2) * (get_global_size(1) * get_global_size(0)) + get_global_id(1) * get_global_size(0) + get_global_id(0));
    dst[id] = id;

    __global volatile atomic_int *atomic_dst = ( __global volatile atomic_int * )dst;
    atomic_fetch_add_explicit( &atomic_dst[atomicOffset], 1 , memory_order_relaxed, memory_scope_all_svm_devices );
}