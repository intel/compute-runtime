/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__attribute__((intel_reqd_sub_group_size(16)))
__kernel void
CopyBuffer(
    __global unsigned int* src,
    __global unsigned int* dst )
{
    int id = (int)get_global_id(0);
    dst[id] = src[id];
}
