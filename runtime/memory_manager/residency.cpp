/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/memory_manager/residency.h"
#include "runtime/os_interface/os_context.h"

using namespace OCLRT;

void ResidencyData::updateCompletionData(uint64_t newFenceValue, OsContext *context) {
    auto contextId = context->getContextId();
    if (contextId + 1 > completionData.size()) {
        completionData.resize(contextId + 1);
    }
    completionData[contextId].lastFence = newFenceValue;
    completionData[contextId].osContext = context;
}

uint64_t ResidencyData::getFenceValueForContextId(uint32_t contextId) {
    return completionData[contextId].lastFence;
}

OsContext *ResidencyData::getOsContextFromId(uint32_t contextId) {
    return completionData[contextId].osContext;
}
