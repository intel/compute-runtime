/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/memory_manager.h"

namespace NEO {
uint32_t MemoryManager::getBanksCount() {
    return 1u;
}
} // namespace NEO
