/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/mocks/mock_mdi.h"

void cloneMdi(MultiDispatchInfo &dst, const MultiDispatchInfo &src) {
    for (auto &srcDi : src) {
        dst.push(srcDi);
    }
    dst.setBuiltinOpParams(src.peekBuiltinOpParams());
}