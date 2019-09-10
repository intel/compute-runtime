/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/residency.h"

using namespace NEO;

void ResidencyData::updateCompletionData(uint64_t newFenceValue, uint32_t contextId) {
    lastFenceValues[contextId] = newFenceValue;
}

uint64_t ResidencyData::getFenceValueForContextId(uint32_t contextId) {
    return lastFenceValues[contextId];
}
