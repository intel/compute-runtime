/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/kernel_descriptor.h"

namespace NEO {
bool KernelDescriptor::hasRTCalls() const {
    return false;
}
} // namespace NEO
