/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/utilities/stackvec.h"

#include <vector>

namespace NEO {

struct ResidencyData {
    ResidencyData() : lastFenceValues(static_cast<size_t>(MemoryManager::maxOsContextCount)) {}
    std::vector<bool> resident = std::vector<bool>(MemoryManager::maxOsContextCount, 0);

    void updateCompletionData(uint64_t newFenceValue, uint32_t contextId);
    uint64_t getFenceValueForContextId(uint32_t contextId);

  protected:
    StackVec<uint64_t, 32> lastFenceValues;
};
} // namespace NEO
