/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel __attribute__((reqd_work_group_size(8, 4, 4)))
void CopyBuffer(
    __global unsigned int *src,
    __global unsigned int *dst)
{
    int id = (int)get_global_id(0);
    dst[id] = src[id];
}

__kernel __attribute__((reqd_work_group_size(1, 1, 1)))
void CopyBuffer2(
    __global unsigned int *src,
    __global unsigned int *dst)
{
    int id = (int)get_global_id(0);
    dst[id] = src[id];
}
