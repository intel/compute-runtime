/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/implicit_scaling.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/linux/drm_direct_submission.h"
#include "shared/source/os_interface/linux/drm_allocation.h"
#include "shared/source/os_interface/linux/drm_buffer_object.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/drm_wrappers.h"
#include "shared/source/os_interface/linux/os_context_linux.h"
#include "shared/source/utilities/wait_util.h"

#include <memory>

namespace NEO {

template <typename GfxFamily, typename Dispatcher>
DrmDirectSubmission<GfxFamily, Dispatcher>::DrmDirectSubmission(const DirectSubmissionInputParams &inputParams)
    : DirectSubmissionHw<GfxFamily, Dispatcher>(inputParams) {

    this->disableMonitorFence = true;

    if (DebugManager.flags.DirectSubmissionDisableMonitorFence.get() != -1) {
        this->disableMonitorFence = DebugManager.flags.DirectSubmissionDisableMonitorFence.get();
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
        }
    }
}

template <typename GfxFamily, typename Dispatcher>
inline DrmDirectSubmission<GfxFamily, Dispatcher>::~DrmDirectSubmission() {
    if (this->ringStart) {
        this->stopRingBuffer();
        this->wait(static_cast<uint32_t>(this->currentTagData.tagValue));
    }
    if (this->isCompletionFenceSupported()) {
        auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
        auto &drm = osContextLinux->getDrm();
        auto completionFenceCpuAddress = reinterpret_cast<uint64_t>(this->completionFenceAllocation->getUnderlyingBuffer()) + Drm::completionFenceOffset;
        drm.waitOnUserFences(*osContextLinux, completionFenceCpuAddress, this->completionFenceValue, this->activeTiles, this->postSyncOffset);
    }
    this->deallocateResources();
}

template <typename GfxFamily, typename Dispatcher>
uint32_t *DrmDirectSubmission<GfxFamily, Dispatcher>::getCompletionValuePointer() {
    if (this->isCompletionFenceSupported()) {
        return &this->completionFenceValue;
    }
    return DirectSubmissionHw<GfxFamily, Dispatcher>::getCompletionValuePointer();
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::allocateOsResources() {
    this->currentTagData.tagAddress = this->semaphoreGpuVa + offsetof(RingSemaphoreData, tagAllocation);
    this->currentTagData.tagValue = 0u;
    this->tagAddress = reinterpret_cast<volatile uint32_t *>(reinterpret_cast<uint8_t *>(this->semaphorePtr) + offsetof(RingSemaphoreData, tagAllocation));
    return true;
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::submit(uint64_t gpuAddress, size_t size) {
    auto bb = static_cast<DrmAllocation *>(this->ringCommandStream.getGraphicsAllocation())->getBO();

    auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
    auto &drm = osContextLinux->getDrm();
    auto execFlags = osContextLinux->getEngineFlag() | drm.getIoctlHelper()->getDrmParamValue(DrmParam::ExecNoReloc);
    auto &drmContextIds = osContextLinux->getDrmContextIds();

    ExecObject execObject{};

    this->handleResidency();

    auto currentBase = this->ringCommandStream.getGraphicsAllocation()->getGpuAddress();
    auto offset = ptrDiff(gpuAddress, currentBase);

    bool ret = false;
    uint32_t drmContextId = 0u;

    uint32_t completionValue = 0u;
    uint64_t completionFenceGpuAddress = 0u;
    if (this->isCompletionFenceSupported()) {
        completionValue = ++completionFenceValue;
        completionFenceGpuAddress = this->completionFenceAllocation->getGpuAddress() + Drm::completionFenceOffset;
    }

    for (auto drmIterator = 0u; drmIterator < osContextLinux->getDeviceBitfield().size(); drmIterator++) {
        if (osContextLinux->getDeviceBitfield().test(drmIterator)) {
            ret |= !!bb->exec(static_cast<uint32_t>(size),
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
            drmContextId++;
            if (completionFenceGpuAddress) {
                completionFenceGpuAddress += this->postSyncOffset;
            }
        }
    }

    return !ret;
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::handleResidency() {
    auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
    osContextLinux->waitForPagingFence();
    return true;
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::isNewResourceHandleNeeded() {
    auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
    auto newResourcesBound = osContextLinux->isTlbFlushRequired();

    if (DebugManager.flags.DirectSubmissionNewResourceTlbFlush.get() != -1) {
        newResourcesBound = DebugManager.flags.DirectSubmissionNewResourceTlbFlush.get();
    }

    return newResourcesBound;
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::handleNewResourcesSubmission() {
    if (isNewResourceHandleNeeded()) {
        auto osContextLinux = static_cast<OsContextLinux *>(&this->osContext);
        auto tlbFlushCounter = osContextLinux->peekTlbFlushCounter();

        Dispatcher::dispatchTlbFlush(this->ringCommandStream, this->gpuVaForMiFlush, *this->hwInfo);
        osContextLinux->setTlbFlushed(tlbFlushCounter);
    }
}

template <typename GfxFamily, typename Dispatcher>
size_t DrmDirectSubmission<GfxFamily, Dispatcher>::getSizeNewResourceHandler() {
    // Overestimate to avoid race
    return Dispatcher::getSizeTlbFlush();
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::handleStopRingBuffer() {
    if (this->disableMonitorFence) {
        this->currentTagData.tagValue++;
    }
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::handleSwitchRingBuffers() {
    if (this->disableMonitorFence) {
        this->currentTagData.tagValue++;

        bool updateCompletionFences = this->ringStart;
        if (DebugManager.flags.EnableRingSwitchTagUpdateWa.get() == 0) {
            updateCompletionFences = true;
        }

        if (updateCompletionFences) {
            this->ringBuffers[this->previousRingBuffer].completionFence = this->currentTagData.tagValue;
        }
    }

    if (this->ringStart) {
        if (this->ringBuffers[this->currentRingBuffer].completionFence != 0) {
            this->wait(static_cast<uint32_t>(this->ringBuffers[this->currentRingBuffer].completionFence));
        }
    }
}

template <typename GfxFamily, typename Dispatcher>
uint64_t DrmDirectSubmission<GfxFamily, Dispatcher>::updateTagValue() {
    if (!this->disableMonitorFence) {
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
        pollAddress = ptrOffset(pollAddress, this->postSyncOffset);
    }
    return true;
}

template <typename GfxFamily, typename Dispatcher>
bool DrmDirectSubmission<GfxFamily, Dispatcher>::isCompletionFenceSupported() {
    return this->completionFenceSupported;
}

template <typename GfxFamily, typename Dispatcher>
void DrmDirectSubmission<GfxFamily, Dispatcher>::wait(uint32_t taskCountToWait) {
    auto pollAddress = this->tagAddress;
    for (uint32_t i = 0; i < this->activeTiles; i++) {
        while (!WaitUtils::waitFunction(pollAddress, taskCountToWait)) {
        }
        pollAddress = ptrOffset(pollAddress, this->postSyncOffset);
    }
}

} // namespace NEO
