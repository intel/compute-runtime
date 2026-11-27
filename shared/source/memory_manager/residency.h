/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace NEO {

struct ResidencyData {
    ResidencyData(size_t maxOsContextCount) : resident(maxOsContextCount, 0), lastFenceValues(static_cast<size_t>(maxOsContextCount)) {}
    std::vector<bool> resident;

    void updateCompletionData(uint64_t newFenceValue, uint32_t contextId);
    uint64_t getFenceValueForContextId(uint32_t contextId);

  protected:
    std::vector<uint64_t> lastFenceValues;
};
} // namespace NEO
