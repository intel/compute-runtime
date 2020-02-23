/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void statelessKernel(__global uchar* src) {
    uint tid = get_global_id(0);
    src[tid] = 0xCD;
}