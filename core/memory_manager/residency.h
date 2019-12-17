/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/memory_manager/memory_manager.h"

#include <vector>

namespace NEO {

struct ResidencyData {
    std::vector<bool> resident = std::vector<bool>(MemoryManager::maxOsContextCount, 0);

    void updateCompletionData(uint64_t newFenceValue, uint32_t contextId);
    uint64_t getFenceValueForContextId(uint32_t contextId);

  protected:
    std::vector<uint64_t> lastFenceValues = std::vector<uint64_t>(MemoryManager::maxOsContextCount, 0);
};
} // namespace NEO
