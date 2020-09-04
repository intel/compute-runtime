/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/os_interface.h"

namespace NEO {

DrmMemoryOperationsHandlerBind::DrmMemoryOperationsHandlerBind() = default;
DrmMemoryOperationsHandlerBind::~DrmMemoryOperationsHandlerBind() = default;

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) {
    auto engines = device->getEngines();
    for (const auto &engine : engines) {
        this->makeResidentWithinOsContext(engine.osContext, gfxAllocations, false);
    }
    return MemoryOperationsStatus::SUCCESS;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto gfxAllocation = gfxAllocations.begin(); gfxAllocation != gfxAllocations.end(); gfxAllocation++) {
        auto drmAllocation = static_cast<DrmAllocation *>(*gfxAllocation);
        auto &drmContextIds = static_cast<const OsContextLinux *>(osContext)->getDrmContextIds();
        for (uint32_t drmIterator = 0u; drmIterator < drmContextIds.size(); drmIterator++) {
            drmAllocation->makeBOsResident(osContext, drmIterator, nullptr, true);
        }
        if (!evictable) {
            drmAllocation->updateResidencyTaskCount(GraphicsAllocation::objectAlwaysResident, osContext->getContextId());
        }
    }
    return MemoryOperationsStatus::SUCCESS;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::evict(Device *device, GraphicsAllocation &gfxAllocation) {
    auto engines = device->getEngines();
    auto retVal = MemoryOperationsStatus::SUCCESS;
    for (const auto &engine : engines) {
        retVal = this->evictWithinOsContext(engine.osContext, gfxAllocation);
        if (retVal != MemoryOperationsStatus::SUCCESS) {
            return retVal;
        }
    }
    return MemoryOperationsStatus::SUCCESS;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) {
    std::lock_guard<std::mutex> lock(mutex);
    auto drmAllocation = static_cast<DrmAllocation *>(&gfxAllocation);
    auto &drmContextIds = static_cast<const OsContextLinux *>(osContext)->getDrmContextIds();
    for (uint32_t drmIterator = 0u; drmIterator < drmContextIds.size(); drmIterator++) {
        drmAllocation->makeBOsResident(osContext, drmIterator, nullptr, false);
    }
    drmAllocation->updateResidencyTaskCount(GraphicsAllocation::objectNotResident, osContext->getContextId());
    return MemoryOperationsStatus::SUCCESS;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::isResident(Device *device, GraphicsAllocation &gfxAllocation) {
    std::lock_guard<std::mutex> lock(mutex);
    bool isResident = true;
    auto engines = device->getEngines();
    for (const auto &engine : engines) {
        isResident &= gfxAllocation.isAlwaysResident(engine.osContext->getContextId());
    }

    if (isResident) {
        return MemoryOperationsStatus::SUCCESS;
    }
    return MemoryOperationsStatus::MEMORY_NOT_FOUND;
}

void DrmMemoryOperationsHandlerBind::mergeWithResidencyContainer(OsContext *osContext, ResidencyContainer &residencyContainer) {
    this->makeResidentWithinOsContext(osContext, ArrayRef<GraphicsAllocation *>(residencyContainer), true);
    residencyContainer.clear();
}

std::unique_lock<std::mutex> DrmMemoryOperationsHandlerBind::lockHandlerForExecWA() {
    return std::unique_lock<std::mutex>();
}

} // namespace NEO
