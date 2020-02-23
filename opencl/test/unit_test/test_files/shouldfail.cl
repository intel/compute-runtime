/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

__kernel void shouldfail(global ushort *dst) {
    // idx and dummy are not defined, compiler should fail the build.
    dst[idx] = dummy;
}
