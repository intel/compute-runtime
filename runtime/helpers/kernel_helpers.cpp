/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/kernel/kernel.h"

namespace NEO {
bool Kernel::requiresCacheFlushCommand(const CommandQueue &commandQueue) const {
    return false;
}
void Kernel::reconfigureKernel() {
}
} // namespace NEO