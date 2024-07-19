/*
 * Copyright (C) 2020-2024 Intel Corporation
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
}

template <typename GfxFamily, typename Dispatcher>
inline DrmDirectSubmission<GfxFamily, Dispatcher>::~DrmDirectSubmission() {
    if (this->ringStart) {
        this->stopRingBuffer(true);
    }
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
    this->wait(static_cast<uint32_t>(this->currentTagData.tagValue));
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::allocateOsResources() {
    this->currentTagData.tagAddress = this->semaphoreGpuVa + offsetof(RingSemaphoreData, tagAllocation);
    this->currentTagData.tagValue = 0u;
    this->tagAddress = reinterpret_cast<volatile TagAddressType *>(reinterpret_cast<uint8_t *>(this->semaphorePtr) + offsetof(RingSemaphoreData, tagAllocation));
    return true;
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::submit(uint64_t gpuAddress, size_t size) {
    auto bb = static_cast<DrmAllocation *>(this->ringCommandStream.getGraphicsAllocation())->getBO();

    auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
    auto &drm = osContextLinux->getDrm();
    auto execFlags = osContextLinux->getEngineFlag() | drm.getIoctlHelper()->getDrmParamValue(DrmParam::execNoReloc);
    auto &drmContextIds = osContextLinux->getDrmContextIds();

    ExecObject execObject{};

    this->handleResidency();

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
            uint32_t errorCode = bb->exec(static_cast<uint32_t>(size),
                                          offset,
                                          execFlags,
                                          false,
                                          &this->osContext,
                                          drmIterator,
                                          drmContextIds[drmContextId],
                                          nullptr,
                                          0,
                                          &execObject,
                                          completionFenceGpuAddress,
                                          completionValue);
            if (errorCode != 0) {
                this->dispatchErrorCode = errorCode;
                ret = false;
            }
            drmContextId++;
            if (completionFenceGpuAddress) {
                completionFenceGpuAddress += this->immWritePostSyncOffset;
            }
        }
    }

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
void DrmDirectSubmission<GfxFamily, Dispatcher>::handleStopRingBuffer() {
    if (this->disableMonitorFence) {
        this->currentTagData.tagValue++;
    }
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::handleSwitchRingBuffers(ResidencyContainer *allocationsForResidency) {
    if (this->disableMonitorFence) {
        this->currentTagData.tagValue++;

        bool updateCompletionFences = true;
        if (debugManager.flags.EnableRingSwitchTagUpdateWa.get() != -1) {
            updateCompletionFences = !debugManager.flags.EnableRingSwitchTagUpdateWa.get() || this->ringStart;
        }

        if (updateCompletionFences) {
            this->ringBuffers[this->previousRingBuffer].completionFence = this->currentTagData.tagValue;
        }
    }
}

template <typename GfxFamily, typename Dispatcher>
uint64_t DrmDirectSubmission<GfxFamily, Dispatcher>::updateTagValue(bool requireMonitorFence) {
    if (requireMonitorFence) {
        this->currentTagData.tagValue++;
        this->ringBuffers[this->currentRingBuffer].completionFence = this->currentTagData.tagValue;
    }
    return 0ull;
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::getTagAddressValue(TagData &tagData) {
    tagData.tagAddress = this->currentTagData.tagAddress;
    tagData.tagValue = this->currentTagData.tagValue + 1;
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
    auto pollAddress = this->tagAddress;
    for (uint32_t i = 0; i < this->activeTiles; i++) {
        while (!WaitUtils::waitFunction(pollAddress, taskCountToWait)) {
        }
        pollAddress = ptrOffset(pollAddress, this->immWritePostSyncOffset);
    }
}

} // namespace NEO
