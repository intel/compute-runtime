/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void testIndirect(__global long* buf) {
    size_t gid = get_global_id(0);
    if (gid == 0) {
        char* val = (char*)buf[0];
        *val = 1;
    }
}
