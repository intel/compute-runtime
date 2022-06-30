/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/utilities/stackvec.h"

#include <vector>

namespace NEO {

struct ResidencyData {
    ResidencyData(size_t maxOsContextCount) : resident(maxOsContextCount, 0), lastFenceValues(static_cast<size_t>(maxOsContextCount)) {}
    std::vector<bool> resident;

    void updateCompletionData(uint64_t newFenceValue, uint32_t contextId);
    uint64_t getFenceValueForContextId(uint32_t contextId);

  protected:
    StackVec<uint64_t, 32> lastFenceValues;
};
} // namespace NEO
