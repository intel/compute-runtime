/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/memory_manager.h"

template <typename KernelT>
void NEO::SyncBufferHandler::prepareForEnqueue(size_t workGroupsCount, KernelT &kernel) {
    auto requiredSize = alignUp(workGroupsCount, CommonConstants::maximalSizeOfAtomicType);

    auto patchData = obtainAllocationAndOffset(requiredSize);

    kernel.patchSyncBuffer(patchData.first, patchData.second);
}