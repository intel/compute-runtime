/*
 * Copyright (C) 2020-2021 Intel Corporation
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
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {

DrmMemoryOperationsHandlerBind::DrmMemoryOperationsHandlerBind(RootDeviceEnvironment &rootDeviceEnvironment, uint32_t rootDeviceIndex)
    : rootDeviceEnvironment(rootDeviceEnvironment),
      rootDeviceIndex(rootDeviceIndex){};

DrmMemoryOperationsHandlerBind::~DrmMemoryOperationsHandlerBind() = default;

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) {
    auto &engines = device->getEngines();
    for (const auto &engine : engines) {
        engine.osContext->ensureContextInitialized();
        this->makeResidentWithinOsContext(engine.osContext, gfxAllocations, false);
    }
    return MemoryOperationsStatus::SUCCESS;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto gfxAllocation = gfxAllocations.begin(); gfxAllocation != gfxAllocations.end(); gfxAllocation++) {
        auto drmAllocation = static_cast<DrmAllocation *>(*gfxAllocation);
        for (auto drmIterator = 0u; drmIterator < osContext->getDeviceBitfield().size(); drmIterator++) {
            if (osContext->getDeviceBitfield().test(drmIterator)) {
                drmAllocation->makeBOsResident(osContext, drmIterator, nullptr, true);
            }
        }
        if (!evictable) {
            drmAllocation->updateResidencyTaskCount(GraphicsAllocation::objectAlwaysResident, osContext->getContextId());
        }
    }
    return MemoryOperationsStatus::SUCCESS;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::evict(Device *device, GraphicsAllocation &gfxAllocation) {
    auto &engines = device->getEngines();
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
    evictImpl(osContext, gfxAllocation, osContext->getDeviceBitfield());
    return MemoryOperationsStatus::SUCCESS;
}

void DrmMemoryOperationsHandlerBind::evictImpl(OsContext *osContext, GraphicsAllocation &gfxAllocation, DeviceBitfield deviceBitfield) {
    auto drmAllocation = static_cast<DrmAllocation *>(&gfxAllocation);
    for (auto drmIterator = 0u; drmIterator < deviceBitfield.size(); drmIterator++) {
        if (deviceBitfield.test(drmIterator)) {
            drmAllocation->makeBOsResident(osContext, drmIterator, nullptr, false);
        }
    }
    drmAllocation->updateResidencyTaskCount(GraphicsAllocation::objectNotResident, osContext->getContextId());
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::isResident(Device *device, GraphicsAllocation &gfxAllocation) {
    std::lock_guard<std::mutex> lock(mutex);
    bool isResident = true;
    auto &engines = device->getEngines();
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

    auto clearContainer = true;

    if (DebugManager.flags.PassBoundBOToExec.get() != -1) {
        clearContainer = !DebugManager.flags.PassBoundBOToExec.get();
    }

    if (clearContainer) {
        residencyContainer.clear();
    }
}

std::unique_lock<std::mutex> DrmMemoryOperationsHandlerBind::lockHandlerIfUsed() {
    return std::unique_lock<std::mutex>();
}

void DrmMemoryOperationsHandlerBind::evictUnusedAllocations(bool waitForCompletion, bool isLockNeeded) {
    auto memoryManager = static_cast<DrmMemoryManager *>(this->rootDeviceEnvironment.executionEnvironment.memoryManager.get());

    std::unique_lock<std::mutex> evictLock(mutex, std::defer_lock);
    if (isLockNeeded) {
        evictLock.lock();
    }

    auto allocLock = memoryManager->acquireAllocLock();

    this->evictUnusedAllocationsImpl(memoryManager->getSysMemAllocs(), waitForCompletion);
    this->evictUnusedAllocationsImpl(memoryManager->getLocalMemAllocs(this->rootDeviceIndex), waitForCompletion);
}

void DrmMemoryOperationsHandlerBind::evictUnusedAllocationsImpl(std::vector<GraphicsAllocation *> &allocationsForEviction, bool waitForCompletion) {
    const auto &engines = this->rootDeviceEnvironment.executionEnvironment.memoryManager->getRegisteredEngines();
    std::vector<GraphicsAllocation *> evictCandidates;

    for (auto subdeviceIndex = 0u; subdeviceIndex < HwHelper::getSubDevicesCount(rootDeviceEnvironment.getHardwareInfo()); subdeviceIndex++) {
        for (auto &allocation : allocationsForEviction) {
            bool evict = true;

            for (const auto &engine : engines) {
                if (this->rootDeviceIndex == engine.commandStreamReceiver->getRootDeviceIndex() &&
                    engine.osContext->getDeviceBitfield().test(subdeviceIndex)) {
                    if (allocation->isAlwaysResident(engine.osContext->getContextId())) {
                        evict = false;
                        break;
                    }

                    if (waitForCompletion) {
                        engine.commandStreamReceiver->waitForCompletionWithTimeout(false, 0, engine.commandStreamReceiver->peekLatestFlushedTaskCount());
                    }

                    if (allocation->isUsedByOsContext(engine.osContext->getContextId()) &&
                        allocation->getTaskCount(engine.osContext->getContextId()) > *engine.commandStreamReceiver->getTagAddress()) {
                        evict = false;
                        break;
                    }
                }
            }
            if (evict) {
                evictCandidates.push_back(allocation);
            }
        }

        for (auto &allocationToEvict : evictCandidates) {
            for (const auto &engine : engines) {
                if (this->rootDeviceIndex == engine.commandStreamReceiver->getRootDeviceIndex() &&
                    engine.osContext->getDeviceBitfield().test(subdeviceIndex)) {
                    DeviceBitfield deviceBitfield;
                    deviceBitfield.set(subdeviceIndex);
                    this->evictImpl(engine.osContext, *allocationToEvict, deviceBitfield);
                }
            }
        }
        evictCandidates.clear();
    }
}

} // namespace NEO
