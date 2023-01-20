/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/constants.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace NEO {

size_t Wddm::getMaxMemAllocSize() const {
    return MemoryConstants::gigaByte;
}

} // namespace NEO
