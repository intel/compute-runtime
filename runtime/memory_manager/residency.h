/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "engine_limits.h"

#include <array>
#include <cstdint>

namespace OCLRT {

struct ResidencyData {
    bool resident[maxOsContextCount] = {};

    void updateCompletionData(uint64_t newFenceValue, uint32_t contextId);
    uint64_t getFenceValueForContextId(uint32_t contextId);

  protected:
    std::array<uint64_t, maxOsContextCount> lastFenceValues = {};
};
} // namespace OCLRT
