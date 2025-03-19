/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void kernelVariableRegisterPerThread(__global unsigned int *input,
                                              __global unsigned int *output) {
    int id = (int)get_global_id(0);
    output[id] = input[id] + 1;
}
