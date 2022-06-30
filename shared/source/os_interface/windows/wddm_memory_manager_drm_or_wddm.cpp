/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_memory_manager.h"

namespace NEO {
bool WddmMemoryManager::isWCMemory(const void *ptr) {
    return false;
}
} // namespace NEO