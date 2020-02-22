/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void CopyBuffer(__global unsigned int *src, __global unsigned int *dst) {
    int id = (int)get_global_id(0);
    dst[id] = lgamma((float)src[id]);
    dst[id] = src[id];
}
