/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler_bind.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/device/device.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/os_context.h"

namespace NEO {

DrmMemoryOperationsHandlerBind::DrmMemoryOperationsHandlerBind(const RootDeviceEnvironment &rootDeviceEnvironment, uint32_t rootDeviceIndex)
    : DrmMemoryOperationsHandler(rootDeviceIndex), rootDeviceEnvironment(rootDeviceEnvironment){};

DrmMemoryOperationsHandlerBind::~DrmMemoryOperationsHandlerBind() = default;

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::makeResident(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations, bool isDummyExecNeeded, const bool forcePagingFence) {
    auto &engines = device->getAllEngines();
    MemoryOperationsStatus result = MemoryOperationsStatus::success;
    for (const auto &engine : engines) {
        engine.commandStreamReceiver->initializeResources(false, device->getPreemptionMode());
        result = this->makeResidentWithinOsContext(engine.osContext, gfxAllocations, false, forcePagingFence, true);
        if (result != MemoryOperationsStatus::success) {
            break;
        }
    }
    return result;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::lock(Device *device, ArrayRef<GraphicsAllocation *> gfxAllocations) {
    for (auto gfxAllocation = gfxAllocations.begin(); gfxAllocation != gfxAllocations.end(); gfxAllocation++) {
        (*gfxAllocation)->setLockedMemory(true);
    }
    return makeResident(device, gfxAllocations, false, false);
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::makeResidentWithinOsContext(OsContext *osContext, ArrayRef<GraphicsAllocation *> gfxAllocations, bool evictable, const bool forcePagingFence, const bool acquireLock) {
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

            if (drmAllocation->storageInfo.isChunked) {
                bo = drmAllocation->getBO();
            }

            if (!bo->getBindInfo()[bo->getOsContextId(osContext)][drmIterator]) {
                bo->requireExplicitLockedMemory(drmAllocation->isLockedMemory());
                bo->requireImmediateBinding(true);

                int result = drmAllocation->makeBOsResident(osContext, drmIterator, nullptr, true, forcePagingFence);
                if (result) {
                    return MemoryOperationsStatus::outOfMemory;
                }
            }
            if (!evictable) {
                drmAllocation->updateResidencyTaskCount(GraphicsAllocation::objectAlwaysResident, osContext->getContextId());
            }
        }
    }

    return MemoryOperationsStatus::success;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::evict(Device *device, GraphicsAllocation &gfxAllocation) {
    auto &engines = device->getAllEngines();
    auto retVal = MemoryOperationsStatus::success;
    gfxAllocation.setLockedMemory(false);
    for (const auto &engine : engines) {
        retVal = this->evictWithinOsContext(engine.osContext, gfxAllocation);
        if (retVal != MemoryOperationsStatus::success) {
            return retVal;
        }
    }
    return MemoryOperationsStatus::success;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::evictWithinOsContext(OsContext *osContext, GraphicsAllocation &gfxAllocation) {
    std::lock_guard<std::mutex> lock(mutex);
    int retVal = evictImpl(osContext, gfxAllocation, osContext->getDeviceBitfield());
    if (retVal) {
        return MemoryOperationsStatus::failed;
    }
    return MemoryOperationsStatus::success;
}

int DrmMemoryOperationsHandlerBind::evictImpl(OsContext *osContext, GraphicsAllocation &gfxAllocation, DeviceBitfield deviceBitfield) {
    auto drmAllocation = static_cast<DrmAllocation *>(&gfxAllocation);
    for (auto drmIterator = 0u; drmIterator < deviceBitfield.size(); drmIterator++) {
        if (deviceBitfield.test(drmIterator)) {
            int retVal = drmAllocation->makeBOsResident(osContext, drmIterator, nullptr, false, false);
            if (retVal) {
                return retVal;
            }
            auto bo = drmAllocation->storageInfo.getNumBanks() > 1 ? drmAllocation->getBOs()[drmIterator] : drmAllocation->getBO();
            if (drmAllocation->storageInfo.isChunked) {
                bo = drmAllocation->getBO();
            }
            bo->requireImmediateBinding(false);
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
        return MemoryOperationsStatus::success;
    }
    return MemoryOperationsStatus::memoryNotFound;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::mergeWithResidencyContainer(OsContext *osContext, ResidencyContainer &residencyContainer) {
    if (debugManager.flags.MakeEachAllocationResident.get() == 2) {
        auto memoryManager = static_cast<DrmMemoryManager *>(this->rootDeviceEnvironment.executionEnvironment.memoryManager.get());

        auto allocLock = memoryManager->acquireAllocLock();
        this->makeResidentWithinOsContext(osContext, ArrayRef<GraphicsAllocation *>(memoryManager->getSysMemAllocs()), true, false, true);
        this->makeResidentWithinOsContext(osContext, ArrayRef<GraphicsAllocation *>(memoryManager->getLocalMemAllocs(this->rootDeviceIndex)), true, false, true);
    }

    auto retVal = this->makeResidentWithinOsContext(osContext, ArrayRef<GraphicsAllocation *>(residencyContainer), true, false, true);
    if (retVal != MemoryOperationsStatus::success) {
        return retVal;
    }

    return MemoryOperationsStatus::success;
}

std::unique_lock<std::mutex> DrmMemoryOperationsHandlerBind::lockHandlerIfUsed() {
    return std::unique_lock<std::mutex>(mutex);
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

        if (status == MemoryOperationsStatus::gpuHangDetectedDuringOperation) {
            return MemoryOperationsStatus::gpuHangDetectedDuringOperation;
        }
    }

    return MemoryOperationsStatus::success;
}

MemoryOperationsStatus DrmMemoryOperationsHandlerBind::evictUnusedAllocationsImpl(std::vector<GraphicsAllocation *> &allocationsForEviction, bool waitForCompletion) {
    const auto &engines = this->rootDeviceEnvironment.executionEnvironment.memoryManager->getRegisteredEngines(this->rootDeviceIndex);
    std::vector<GraphicsAllocation *> evictCandidates;

    for (auto subdeviceIndex = 0u; subdeviceIndex < GfxCoreHelper::getSubDevicesCount(rootDeviceEnvironment.getHardwareInfo()); subdeviceIndex++) {
        for (auto &allocation : allocationsForEviction) {
            bool evict = true;

            if (allocation->getRootDeviceIndex() != this->rootDeviceIndex) {
                continue;
            }

            for (const auto &engine : engines) {
                if (engine.osContext->getDeviceBitfield().test(subdeviceIndex)) {
                    if (allocation->isAlwaysResident(engine.osContext->getContextId())) {
                        evict = false;
                        break;
                    }
                    if (allocation->isLockedMemory()) {
                        evict = false;
                        break;
                    }
                    if (waitForCompletion && engine.commandStreamReceiver->isInitialized()) {
                        const auto waitStatus = engine.commandStreamReceiver->waitForCompletionWithTimeout(WaitParams{false, false, false, 0}, engine.commandStreamReceiver->peekLatestFlushedTaskCount());
                        if (waitStatus == WaitStatus::gpuHang) {
                            return MemoryOperationsStatus::gpuHangDetectedDuringOperation;
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
                if (engine.osContext->getDeviceBitfield().test(subdeviceIndex)) {
                    DeviceBitfield deviceBitfield;
                    deviceBitfield.set(subdeviceIndex);
                    this->evictImpl(engine.osContext, *allocationToEvict, deviceBitfield);
                }
            }
        }
        evictCandidates.clear();
    }

    return MemoryOperationsStatus::success;
}

} // namespace NEO
