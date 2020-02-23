/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void SimpleArg(int src, __global int *dst) {
    int id = (int)get_global_id(0);

#ifdef DEF_WAS_SPECIFIED
    int val = 1;
#else
    // fail to compile
#endif

    dst[id] = src + val;
}
