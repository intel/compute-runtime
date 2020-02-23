/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__constant uint constant_a[2] = {0xabcd5432u, 0xaabb5533u};

__kernel void test(__global uint *in, __global uint *out) {
    int i = get_global_id(0);
    int j = get_global_id(0) % (sizeof(constant_a) / sizeof(constant_a[0]));

    out[i] = constant_a[j];
}
