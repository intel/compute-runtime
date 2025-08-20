/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/tag_allocation_layout.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_memory_manager.h"
#include "shared/source/os_interface/linux/drm_memory_operations_handler.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/ioctl_helper.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/os_interface/linux/sys_calls.h"
#include "shared/source/utilities/wait_util.h"

#include <iostream>
#include <memory>

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
DrmDirectSubmission<GfxFamily, Dispatcher>::DrmDirectSubmission(const DirectSubmissionInputParams &inputParams)
    : DirectSubmissionHw<GfxFamily, Dispatcher>(inputParams) {
    this->execObjectsStorage.resize(1u);

    this->completionFenceValue = inputParams.initialCompletionFenceValue;
    if (debugManager.flags.OverrideUserFenceStartValue.get() != -1) {
        this->completionFenceValue = static_cast<decltype(completionFenceValue)>(debugManager.flags.OverrideUserFenceStartValue.get());
    }

    auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);

    auto subDevices = osContextLinux->getDeviceBitfield();
    bool dispatcherSupport = Dispatcher::isMultiTileSynchronizationSupported();
    if (ImplicitScalingHelper::isImplicitScalingEnabled(subDevices, true) && dispatcherSupport) {
        this->activeTiles = static_cast<uint32_t>(subDevices.count());
    }
    this->partitionedMode = this->activeTiles > 1u;
    this->partitionConfigSet = !this->partitionedMode;

    auto &drm = osContextLinux->getDrm();
    drm.setDirectSubmissionActive(true);

    if (!drm.isVmBindAvailable()) {
        static_cast<DrmMemoryManager *>(this->memoryManager)->disableForcePin();
    }

    auto usePciBarrier = !this->hwInfo->capabilityTable.isIntegratedDevice;
    if (debugManager.flags.DirectSubmissionPCIBarrier.get() != -1) {
        usePciBarrier = debugManager.flags.DirectSubmissionPCIBarrier.get();
    }

    if (usePciBarrier) {
        auto ptr = static_cast<uint32_t *>(drm.getIoctlHelper()->pciBarrierMmap());
        if (ptr != MAP_FAILED) {
            this->pciBarrierPtr = ptr;
        }
    }
    PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "Using PCI barrier ptr: %p\n", this->pciBarrierPtr);
    if (this->pciBarrierPtr) {
        this->miMemFenceRequired = false;
    }

    if (this->partitionedMode) {
        this->workPartitionAllocation = inputParams.workPartitionAllocation;
        UNRECOVERABLE_IF(this->workPartitionAllocation == nullptr);
    }

    if (this->miMemFenceRequired || drm.completionFenceSupport()) {
        this->completionFenceAllocation = inputParams.completionFenceAllocation;
        if (this->completionFenceAllocation) {
            this->gpuVaForAdditionalSynchronizationWA = this->completionFenceAllocation->getGpuAddress() + 8u;
            if (drm.completionFenceSupport()) {
                this->completionFenceSupported = true;
            }

            if (debugManager.flags.PrintCompletionFenceUsage.get()) {
                std::cout << "Completion fence for DirectSubmission:"
                          << " GPU address: " << std::hex << (this->completionFenceAllocation->getGpuAddress() + TagAllocationLayout::completionFenceOffset)
                          << ", CPU address: " << (castToUint64(this->completionFenceAllocation->getUnderlyingBuffer()) + TagAllocationLayout::completionFenceOffset)
                          << std::dec << std::endl;
            }
        }
    }
    this->notifyKmdDuringMonitorFence = true;
}

template <typename GfxFamily, typename Dispatcher>
inline DrmDirectSubmission<GfxFamily, Dispatcher>::~DrmDirectSubmission() {
    if (this->ringStart) {
        this->stopRingBuffer(true);
    }
    this->tagAddress = nullptr;
    if (this->isCompletionFenceSupported()) {
        auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
        auto &drm = osContextLinux->getDrm();
        auto completionFenceCpuAddress = reinterpret_cast<uint64_t>(this->completionFenceAllocation->getUnderlyingBuffer()) + TagAllocationLayout::completionFenceOffset;
        drm.waitOnUserFences(*osContextLinux, completionFenceCpuAddress, this->completionFenceValue, this->activeTiles, -1, this->immWritePostSyncOffset, false, NEO::InterruptId::notUsed, nullptr);
    }
    this->deallocateResources();
    if (this->pciBarrierPtr) {
        SysCalls::munmap(this->pciBarrierPtr, MemoryConstants::pageSize);
    }
}

template <typename GfxFamily, typename Dispatcher>
TaskCountType *DrmDirectSubmission<GfxFamily, Dispatcher>::getCompletionValuePointer() {
    if (this->isCompletionFenceSupported()) {
        return &this->completionFenceValue;
    }
    return DirectSubmissionHw<GfxFamily, Dispatcher>::getCompletionValuePointer();
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::ensureRingCompletion() {
    if (this->tagAddress) {
        this->wait(static_cast<uint32_t>(this->currentTagData.tagValue));
    }
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::allocateOsResources() {
    DirectSubmissionHw<GfxFamily, Dispatcher>::allocateOsResources();
    this->currentTagData.tagAddress = this->semaphoreGpuVa + offsetof(RingSemaphoreData, tagAllocation);
    this->currentTagData.tagValue = 0u;
    return true;
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::submit(uint64_t gpuAddress, size_t size, const ResidencyContainer *allocationsForResidency) {
    auto bb = static_cast<DrmAllocation *>(this->ringCommandStream.getGraphicsAllocation())->getBO();

    auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
    auto &drm = osContextLinux->getDrm();
    auto execFlags = osContextLinux->getEngineFlag() | drm.getIoctlHelper()->getDrmParamValue(DrmParam::execNoReloc);
    auto &drmContextIds = osContextLinux->getDrmContextIds();

    if (!allocationsForResidency) {
        this->handleResidency();
    } else {
        this->lastUllsLightExecTimestamp = std::chrono::steady_clock::now();
        this->boHandleForExec = bb->peekHandle();
    }

    auto currentBase = this->ringCommandStream.getGraphicsAllocation()->getGpuAddress();
    auto offset = ptrDiff(gpuAddress, currentBase);

    bool ret = true;
    uint32_t drmContextId = 0u;

    TaskCountType completionValue = 0u;
    uint64_t completionFenceGpuAddress = 0u;
    if (this->isCompletionFenceSupported()) {
        completionValue = completionFenceValue + 1;
        completionFenceGpuAddress = this->completionFenceAllocation->getGpuAddress() + TagAllocationLayout::completionFenceOffset;
    }

    for (auto drmIterator = 0u; drmIterator < osContextLinux->getDeviceBitfield().size(); drmIterator++) {
        if (osContextLinux->getDeviceBitfield().test(drmIterator)) {
            auto size = allocationsForResidency ? allocationsForResidency->size() : 0u;
            for (uint32_t i = 0; i < size; i++) {
                auto drmAlloc = static_cast<DrmAllocation *>((*allocationsForResidency)[i]);
                drmAlloc->makeBOsResident(&this->osContext, drmIterator, &this->residency, false, false);
            }

            auto requiredSize = this->residency.size() + 1;
            if (requiredSize > this->execObjectsStorage.size()) {
                this->execObjectsStorage.resize(requiredSize);
            }

            auto errorCode = bb->exec(static_cast<uint32_t>(size),
                                      offset,
                                      execFlags,
                                      false,
                                      &this->osContext,
                                      drmIterator,
                                      drmContextIds[drmContextId],
                                      this->residency.data(),
                                      this->residency.size(),
                                      this->execObjectsStorage.data(),
                                      completionFenceGpuAddress,
                                      completionValue);
            if (errorCode != 0) {
                this->dispatchErrorCode = errorCode;
                ret = false;
                break;
            }
            drmContextId++;
            if (completionFenceGpuAddress) {
                completionFenceGpuAddress += this->immWritePostSyncOffset;
            }
        }
    }

    this->residency.clear();

    if (this->isCompletionFenceSupported() && ret) {
        completionFenceValue++;
    }

    return ret;
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::handleResidency() {
    auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
    osContextLinux->waitForPagingFence();
    return true;
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::handleRingRestartForUllsLightResidency(const ResidencyContainer *allocationsForResidency) {
    if (allocationsForResidency) {
        auto restartNeeded = (static_cast<DrmMemoryOperationsHandler *>(this->memoryOperationHandler)->obtainAndResetNewResourcesSinceLastRingSubmit() ||
                              std::chrono::duration_cast<std::chrono::microseconds>(this->getCpuTimePoint() - this->lastUllsLightExecTimestamp) > std::chrono::microseconds{ullsLightTimeout});
        if (restartNeeded) {
            this->stopRingBuffer(false);
        }
    }
}

template <typename GfxFamily, typename Dispatcher>
inline void DrmDirectSubmission<GfxFamily, Dispatcher>::handleResidencyContainerForUllsLightNewRingAllocation(ResidencyContainer *allocationsForResidency) {
    if (allocationsForResidency) {
        allocationsForResidency->clear();
        static_cast<DrmMemoryOperationsHandler *>(this->memoryOperationHandler)->mergeWithResidencyContainer(&this->osContext, *allocationsForResidency);
    }
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::handleStopRingBuffer() {
    this->currentTagData.tagValue++;
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::dispatchStopRingBufferSection() {
    TagData currentTagData = {};
    getTagAddressValue(currentTagData);
    Dispatcher::dispatchMonitorFence(this->ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, this->rootDeviceEnvironment, this->partitionedMode, this->dcFlushRequired, this->notifyKmdDuringMonitorFence);
}

template <typename GfxFamily, typename Dispatcher>
size_t DrmDirectSubmission<GfxFamily, Dispatcher>::dispatchStopRingBufferSectionSize() {
    return Dispatcher::getSizeMonitorFence(this->rootDeviceEnvironment);
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::handleSwitchRingBuffers(ResidencyContainer *allocationsForResidency) {
    if (this->ringStart) {
        this->currentTagData.tagValue++;
    }

    bool updateCompletionFences = true;
    if (debugManager.flags.EnableRingSwitchTagUpdateWa.get() != -1) {
        updateCompletionFences = !debugManager.flags.EnableRingSwitchTagUpdateWa.get() || this->ringStart;
    }

    if (updateCompletionFences) {
        this->ringBuffers[this->previousRingBuffer].completionFence = this->currentTagData.tagValue;
    }
}

template <typename GfxFamily, typename Dispatcher>
uint64_t DrmDirectSubmission<GfxFamily, Dispatcher>::updateTagValue(bool requireMonitorFence) {
    if (requireMonitorFence) {
        this->currentTagData.tagValue++;
        this->ringBuffers[this->currentRingBuffer].completionFence = this->currentTagData.tagValue;
    }
    return boHandleForExec;
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::getTagAddressValue(TagData &tagData) {
    tagData.tagAddress = this->currentTagData.tagAddress;
    tagData.tagValue = this->currentTagData.tagValue + 1;
}
template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::getTagAddressValueForRingSwitch(TagData &tagData) {
    getTagAddressValue(tagData);
}

template <typename GfxFamily, typename Dispatcher>
inline bool DrmDirectSubmission<GfxFamily, Dispatcher>::isCompleted(uint32_t ringBufferIndex) {
    auto taskCount = this->ringBuffers[ringBufferIndex].completionFence;
    auto pollAddress = this->tagAddress;
    for (uint32_t i = 0; i < this->activeTiles; i++) {
        if (*pollAddress < taskCount) {
            return false;
        }
        pollAddress = ptrOffset(pollAddress, this->immWritePostSyncOffset);
    }
    return true;
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::isCompletionFenceSupported() {
    return this->completionFenceSupported;
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::wait(TaskCountType taskCountToWait) {
    auto lastHangCheckTime = std::chrono::high_resolution_clock::now();
    auto waitStartTime = lastHangCheckTime;
    auto pollAddress = this->tagAddress;
    for (uint32_t i = 0; i < this->activeTiles; i++) {
        while (!WaitUtils::waitFunction(pollAddress, taskCountToWait, std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - waitStartTime).count()) &&
               !isGpuHangDetected(lastHangCheckTime)) {
        }
        pollAddress = ptrOffset(pollAddress, this->immWritePostSyncOffset);
    }
}

template <typename GfxFamily, typename Dispatcher>
std::chrono::steady_clock::time_point DrmDirectSubmission<GfxFamily, Dispatcher>::getCpuTimePoint() {
    return std::chrono::steady_clock::now();
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::isGpuHangDetected(std::chrono::high_resolution_clock::time_point &lastHangCheckTime) {
    if (!this->detectGpuHang) {
        return false;
    }

    auto currentTime = std::chrono::high_resolution_clock::now();
    auto elapsedTimeSinceGpuHangCheck = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - lastHangCheckTime);
    if (elapsedTimeSinceGpuHangCheck.count() >= gpuHangCheckPeriod.count()) {
        lastHangCheckTime = currentTime;
        auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
        auto &drm = osContextLinux->getDrm();
        return drm.isGpuHangDetected(this->osContext);
    }
    return false;
}

} // namespace NEO
