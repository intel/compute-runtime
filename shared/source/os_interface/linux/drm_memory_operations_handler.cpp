/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"

#include <algorithm>

namespace NEO {

DrmMemoryOperationsHandler::DrmMemoryOperationsHandler() = default;
DrmMemoryOperationsHandler::~DrmMemoryOperationsHandler() = default;

MemoryOperationsStatus DrmMemoryOperationsHandler::makeResident(ArrayRef<GraphicsAllocation *> gfxAllocations) {
    std::lock_guard<std::mutex> lock(mutex);
    this->residency.insert(gfxAllocations.begin(), gfxAllocations.end());
    return MemoryOperationsStatus::SUCCESS;
}

MemoryOperationsStatus DrmMemoryOperationsHandler::evict(GraphicsAllocation &gfxAllocation) {
    std::lock_guard<std::mutex> lock(mutex);
    this->residency.erase(&gfxAllocation);
    return MemoryOperationsStatus::SUCCESS;
}

MemoryOperationsStatus DrmMemoryOperationsHandler::isResident(GraphicsAllocation &gfxAllocation) {
    std::lock_guard<std::mutex> lock(mutex);
    auto ret = this->residency.find(&gfxAllocation);
    if (ret == this->residency.end()) {
        return MemoryOperationsStatus::MEMORY_NOT_FOUND;
    }
    return MemoryOperationsStatus::SUCCESS;
}

void DrmMemoryOperationsHandler::mergeWithResidencyContainer(ResidencyContainer &residencyContainer) {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto gfxAllocation = this->residency.begin(); gfxAllocation != this->residency.end(); gfxAllocation++) {
        auto ret = std::find(residencyContainer.begin(), residencyContainer.end(), *gfxAllocation);
        if (ret == residencyContainer.end()) {
            residencyContainer.push_back(*gfxAllocation);
        }
    }
}

} // namespace NEO
