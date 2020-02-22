/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void test(__global ulong *results) {
    __private char mem0[3];
    __private char2 mem2[3];
    __private char3 mem3[3];
    __private char4 mem4[3];
    __private char8 mem8[3];
    __private char16 mem16[3];

    results[0] = (ulong)&mem0[0];
    results[1] = (ulong)&mem2[0];
    results[2] = (ulong)&mem3[0];
    results[3] = (ulong)&mem4[0];
    results[4] = (ulong)&mem8[0];
    results[5] = (ulong)&mem16[0];
}
