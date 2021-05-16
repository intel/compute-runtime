/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void test() {
    printf("OpenCL\n");
}

__kernel void test_printf_number(__global uint* in) {
    printf("in[0] = %d\n", in[0]);
}
