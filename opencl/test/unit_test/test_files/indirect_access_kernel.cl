/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void testIndirect(__global long* buf) {
    size_t gid = get_global_id(0);
    if (gid == 0) {
        __global char* val = (__global char*)buf[0];
        *val = 1;
    }
}
