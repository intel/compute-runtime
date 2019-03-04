/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "engine_node.h"

#include <cinttypes>
#include <vector>
namespace OCLRT {

struct ResidencyData {
    ResidencyData() {
        std::fill_n(resident, sizeof(resident), false);
    }

    ~ResidencyData() = default;
    bool resident[maxOsContextCount];

    void updateCompletionData(uint64_t newFenceValue, uint32_t contextId);
    uint64_t getFenceValueForContextId(uint32_t contextId);

  protected:
    std::vector<uint64_t> lastFenceValues;
};
} // namespace OCLRT
