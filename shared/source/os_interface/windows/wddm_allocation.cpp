/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_allocation.h"

namespace NEO {
std::string WddmAllocation::getAllocationInfoString() const {
    return getHandleInfoString();
}
} // namespace NEO