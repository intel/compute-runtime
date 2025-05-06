/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "implicit_args.h"

namespace ImplicitArgsTestHelper {
constexpr uint32_t getImplicitArgsSize(uint32_t version) {
    if (version == 0) {
        return NEO::ImplicitArgsV0::getSize();
    } else if (version == 1) {
        return NEO::ImplicitArgsV1::getSize();
    }
    return 0;
}
} // namespace ImplicitArgsTestHelper