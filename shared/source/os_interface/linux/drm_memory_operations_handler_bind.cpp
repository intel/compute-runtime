/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/os_interface.h"

namespace NEO {

DrmMemoryOperationsHandlerBind::DrmMemoryOperationsHandlerBind(const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t rootDeviceIndex)
    : DrmMemoryOperationsHandler(rootDeviceIndex), rootDeviceEnvironment(rootDeviceEnvironment){};

DrmMemoryOperationsHandlerBind::~DrmMemoryOperationsHandlerBind() = default;

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) {
    auto &engines = device->getAllEngines();
    for (const auto &engine : engines) {
        engine.commandStreamReceiver->initializeResources();
        this->makeResidentWithinOsContext(engine.osContext, gfxAllocations, false);
    }
    return MemoryOperationsStatus::SUCCESS;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable) {
    auto deviceBitfield = osContext->getDeviceBitfield();

    std::lock_guard<std::mutex> lock(mutex);
    auto devicesDone = 0u;
    for (auto drmIterator = 0u; devicesDone < deviceBitfield.count(); drmIterator++) {
        if (!deviceBitfield.test(drmIterator)) {
            continue;
        }
        devicesDone++;

        for (auto gfxAllocation = gfxAllocations.begin(); gfxAllocation != gfxAllocations.end(); gfxAllocation++) {
            auto drmAllocation = static_cast<DrmAllocation *>(*gfxAllocation);
            auto bo = drmAllocation->storageInfo.getNumBanks() > 1 ? drmAllocation->getBOs()[drmIterator] : drmAllocation->getBO();

            if (!bo->bindInfo[bo->getOsContextId(osContext)][drmIterator]) {
                int result = drmAllocation->makeBOsResident(osContext, drmIterator, nullptr, true);
                if (result) {
                    return MemoryOperationsStatus::OUT_OF_MEMORY;
                }
            }

            if (!evictable) {
                drmAllocation->updateResidencyTaskCount(GraphicsAllocation::objectAlwaysResident, osContext->getContextId());
            }
        }
    }

    return MemoryOperationsStatus::SUCCESS;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::evict(Device *device, GraphicsAllocation &gfxAllocation) {
    auto &engines = device->getAllEngines();
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
    int retVal = evictImpl(osContext, gfxAllocation, osContext->getDeviceBitfield());
    if (retVal) {
        return MemoryOperationsStatus::FAILED;
    }
    return MemoryOperationsStatus::SUCCESS;
}

int DrmMemoryOperationsHandlerBind::evictImpl(OsContext *osContext, GraphicsAllocation &gfxAllocation, DeviceBitfield deviceBitfield) {
    auto drmAllocation = static_cast<DrmAllocation *>(&gfxAllocation);
    for (auto drmIterator = 0u; drmIterator < deviceBitfield.size(); drmIterator++) {
        if (deviceBitfield.test(drmIterator)) {
            int retVal = drmAllocation->makeBOsResident(osContext, drmIterator, nullptr, false);
            if (retVal) {
                return retVal;
            }
        }
    }
    drmAllocation->updateResidencyTaskCount(GraphicsAllocation::objectNotResident, osContext->getContextId());

    return 0;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::isResident(Device *device, GraphicsAllocation &gfxAllocation) {
    std::lock_guard<std::mutex> lock(mutex);
    bool isResident = true;
    auto &engines = device->getAllEngines();
    for (const auto &engine : engines) {
        isResident &= gfxAllocation.isAlwaysResident(engine.osContext->getContextId());
    }

    if (isResident) {
        return MemoryOperationsStatus::SUCCESS;
    }
    return MemoryOperationsStatus::MEMORY_NOT_FOUND;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::mergeWithResidencyContainer(OsContext *osContext, ResidencyContainer &residencyContainer) {
    if (DebugManager.flags.MakeEachAllocationResident.get() == 2) {
        auto memoryManager = static_cast<DrmMemoryManager *>(this->rootDeviceEnvironment.executionEnvironment.memoryManager.get());

        auto allocLock = memoryManager->acquireAllocLock();
        this->makeResidentWithinOsContext(osContext, ArrayRef<GraphicsAllocation *>(memoryManager->getSysMemAllocs()), true);
        this->makeResidentWithinOsContext(osContext, ArrayRef<GraphicsAllocation *>(memoryManager->getLocalMemAllocs(this->rootDeviceIndex)), true);
    }

    auto retVal = this->makeResidentWithinOsContext(osContext, ArrayRef<GraphicsAllocation *>(residencyContainer), true);
    if (retVal != MemoryOperationsStatus::SUCCESS) {
        return retVal;
    }

    return MemoryOperationsStatus::SUCCESS;
}

std::unique_lock<std::mutex> DrmMemoryOperationsHandlerBind::lockHandlerIfUsed() {
    return std::unique_lock<std::mutex>();
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::evictUnusedAllocations(bool waitForCompletion, bool isLockNeeded) {
    auto memoryManager = static_cast<DrmMemoryManager *>(this->rootDeviceEnvironment.executionEnvironment.memoryManager.get());

    std::unique_lock<std::mutex> evictLock(mutex, std::defer_lock);
    if (isLockNeeded) {
        evictLock.lock();
    }

    auto allocLock = memoryManager->acquireAllocLock();

    for (const auto status : {
             this->evictUnusedAllocationsImpl(memoryManager->getSysMemAllocs(), waitForCompletion),
             this->evictUnusedAllocationsImpl(memoryManager->getLocalMemAllocs(this->rootDeviceIndex), waitForCompletion)}) {

        if (status == MemoryOperationsStatus::GPU_HANG_DETECTED_DURING_OPERATION) {
            return MemoryOperationsStatus::GPU_HANG_DETECTED_DURING_OPERATION;
        }
    }

    return MemoryOperationsStatus::SUCCESS;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::evictUnusedAllocationsImpl(std::vector<GraphicsAllocation *> &allocationsForEviction, bool waitForCompletion) {
    const auto &engines = this->rootDeviceEnvironment.executionEnvironment.memoryManager->getRegisteredEngines();
    std::vector<GraphicsAllocation *> evictCandidates;

    for (auto subdeviceIndex = 0u; subdeviceIndex < GfxCoreHelper::getSubDevicesCount(rootDeviceEnvironment.getHardwareInfo()); subdeviceIndex++) {
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
                        const auto waitStatus = engine.commandStreamReceiver->waitForCompletionWithTimeout(WaitParams{false, false, 0}, engine.commandStreamReceiver->peekLatestFlushedTaskCount());
                        if (waitStatus == WaitStatus::GpuHang) {
                            return MemoryOperationsStatus::GPU_HANG_DETECTED_DURING_OPERATION;
                        }
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

    return MemoryOperationsStatus::SUCCESS;
}

} // namespace NEO
