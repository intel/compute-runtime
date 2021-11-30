/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void StatefulCopyBuffer(
    const __global uchar* src,
    __global uchar* dst)
{
    uint id = get_global_id(0);
    dst[id] = src[id];
}
