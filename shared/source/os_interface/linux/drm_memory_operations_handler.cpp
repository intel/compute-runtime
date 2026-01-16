/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"

#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/os_context.h"

namespace NEO {

MemoryOperationsStatus DrmMemoryOperationsHandler::evictUnusedAllocationsImpl(std::vector<GraphicsAllocation *> &allocationsForEviction, bool waitForCompletion) {
    if (!this->rootDeviceEnvironment.executionEnvironment.memoryManager.get()) {
        return MemoryOperationsStatus::success;
    }
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
