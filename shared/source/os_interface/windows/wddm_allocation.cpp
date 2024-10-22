/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_allocation.h"

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/resource_info.h"

namespace NEO {
std::string WddmAllocation::getAllocationInfoString() const {
    return getHandleInfoString();
}
std::string WddmAllocation::getPatIndexInfoString(const ProductHelper &) const {
    return "";
}
} // namespace NEO