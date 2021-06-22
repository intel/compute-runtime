/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/windows/wddm_memory_manager.h"

namespace NEO {

size_t WddmMemoryManager::getHugeGfxMemoryChunkSize() const {
    return 31 * MemoryConstants::megaByte;
}

} // namespace NEO
