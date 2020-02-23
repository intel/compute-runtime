/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"

namespace NEO {
uint32_t MemoryManager::getBanksCount() {
    return 1u;
}
} // namespace NEO
