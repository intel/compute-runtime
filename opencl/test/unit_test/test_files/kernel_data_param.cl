/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void test(__global int *dst) {
    int tid = get_global_id(0);
    int n = get_global_size(0);

    dst[tid] = n;
};

__kernel void test_get_local_size(__global int *dst) {
    int tid = get_global_id(0);
    int n = get_local_size(0);

    dst[tid] = n;
};
