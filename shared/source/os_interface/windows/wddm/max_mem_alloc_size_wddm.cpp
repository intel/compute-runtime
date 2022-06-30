/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

size_t Wddm::getMaxMemAllocSize() const {
    return std::numeric_limits<size_t>::max();
}

} // namespace NEO
