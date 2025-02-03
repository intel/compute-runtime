/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void
SimpleKernelLargeGRF(
	       __global unsigned int* src,
	           __global unsigned int* dst )
{
    int id = (int)get_global_id(0);
    dst[id] = src[id] + 1;
}

