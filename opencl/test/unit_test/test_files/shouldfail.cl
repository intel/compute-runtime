/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void shouldfail(global ushort *dst) {
    // idx and dummy are not defined, compiler should fail the build.
    dst[idx] = dummy;
}
