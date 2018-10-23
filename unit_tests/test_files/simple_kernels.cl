/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void simple_kernel_0(
    const uint arg0,
    const float arg1,
    __global uint *dst) {

    uint idx = get_global_id(0);
    uint data = arg0 + (uint)arg1;

    dst[idx] = data;
}

__kernel void simple_kernel_1(
    __global uint *src,
    const uint arg1,
    __global uint *dst) {

    uint idx = get_global_id(0);

    dst[idx] = src[idx] + arg1;
}

__kernel void simple_kernel_2(
    const uint arg0,
    __global uint *dst) {

    uint idx = get_global_id(0);

    dst[idx] = arg0;
}

__kernel void simple_kernel_3(
    __global uint *dst) {
    dst[get_global_id(0)] = 0;
}

__kernel void simple_kernel_4() {
}
