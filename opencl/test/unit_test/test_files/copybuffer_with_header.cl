/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "simple_header.h"
__kernel void
CopyBuffer(
    __global unsigned int *src,
    __global unsigned int *dst) {
    int id = (int)get_global_id(0);
    dst[id] = src[id];
}
