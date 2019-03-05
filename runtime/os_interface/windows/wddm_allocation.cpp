/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/os_interface/windows/wddm_allocation.h"

namespace OCLRT {
std::string WddmAllocation::getAllocationInfoString() const {
    return getHandleInfoString();
}
uint32_t WddmAllocation::getNumHandles() const {
    return 1u;
}
} // namespace OCLRT