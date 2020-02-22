/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void SimpleArg(int src, __global int *dst) {
    int id = (int)get_global_id(0);
    dst[id] = src;
}
