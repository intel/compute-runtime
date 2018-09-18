/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cinttypes>
#include <vector>
namespace OCLRT {
class OsContext;

struct FenceData {
    uint64_t lastFence = 0;
    OsContext *osContext = nullptr;
};

struct ResidencyData {
    ResidencyData() = default;
    ~ResidencyData() = default;
    bool resident = false;

    void updateCompletionData(uint64_t newFenceValue, OsContext *context);
    uint64_t getFenceValueForContextId(uint32_t contextId);
    OsContext *getOsContextFromId(uint32_t contextId);

  protected:
    std::vector<FenceData> completionData;
};
} // namespace OCLRT
