/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/direct_submission/windows/wddm_direct_submission.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm_residency_logger.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/source/utilities/arrayref.h"

namespace NEO {

// Initialize COMMAND_BUFFER_HEADER         Type PatchList  Streamer Perf Tag
DECLARE_COMMAND_BUFFER(CommandBufferHeader, UMD_OCL, FALSE, FALSE, PERFTAG_OCL);

template <typename GfxFamily, typename Dispatcher>
WddmDirectSubmission<GfxFamily, Dispatcher>::WddmDirectSubmission(const DirectSubmissionInputParams &inputParams)
    : DirectSubmissionHw<GfxFamily, Dispatcher>(inputParams) {
    osContextWin = reinterpret_cast<OsContextWin *>(&this->osContext);
    wddm = osContextWin->getWddm();
    commandBufferHeader = std::make_unique<COMMAND_BUFFER_HEADER_REC>();
    *(commandBufferHeader.get()) = CommandBufferHeader;
    if (osContextWin->getPreemptionMode() != PreemptionMode::Disabled) {
        commandBufferHeader->NeedsMidBatchPreEmptionSupport = true;
    }
    perfLogResidencyVariadicLog(wddm->getResidencyLogger(), "Starting Wddm ULLS. Placement ring buffer: %d semaphore %d\n",
                                debugManager.flags.DirectSubmissionBufferPlacement.get(),
                                debugManager.flags.DirectSubmissionSemaphorePlacement.get());

    this->completionFenceAllocation = inputParams.completionFenceAllocation;
    UNRECOVERABLE_IF(!this->completionFenceAllocation);
    if (this->miMemFenceRequired) {
        this->gpuVaForAdditionalSynchronizationWA = this->completionFenceAllocation->getGpuAddress() + 8u;
    }
}

template <typename GfxFamily, typename Dispatcher>
WddmDirectSubmission<GfxFamily, Dispatcher>::~WddmDirectSubmission() {
    perfLogResidencyVariadicLog(wddm->getResidencyLogger(), "Stopping Wddm ULLS\n");
    if (this->ringStart) {
        this->stopRingBuffer(true);
    }
    this->deallocateResources();
    wddm->getWddmInterface()->destroyMonitorFence(ringFence);
}

template <typename GfxFamily, typename Dispatcher>
inline void WddmDirectSubmission<GfxFamily, Dispatcher>::flushMonitorFence(bool notifyKmd) {
    auto needStart = !this->ringStart;

    size_t requiredMinimalSize = this->getSizeSemaphoreSection(false) +
                                 Dispatcher::getSizeMonitorFence(this->rootDeviceEnvironment) +
                                 this->getSizeNewResourceHandler() +
                                 this->getSizeSwitchRingBufferSection() +
                                 this->getSizeEnd(false);
    this->switchRingBuffersNeeded(requiredMinimalSize, nullptr);

    auto startVA = this->ringCommandStream.getCurrentGpuAddressPosition();

    this->handleNewResourcesSubmission();

    TagData currentTagData = {};
    this->getTagAddressValue(currentTagData);
    Dispatcher::dispatchMonitorFence(this->ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, this->rootDeviceEnvironment, this->partitionedMode, this->dcFlushRequired, notifyKmd);

    this->dispatchSemaphoreSection(this->currentQueueWorkCount + 1);
    this->submitCommandBufferToGpu(needStart, startVA, requiredMinimalSize, true, nullptr);
    this->currentQueueWorkCount++;

    this->updateTagValueImpl(this->currentRingBuffer);
}

template <typename GfxFamily, typename Dispatcher>
void WddmDirectSubmission<GfxFamily, Dispatcher>::ensureRingCompletion() {
    WddmDirectSubmission<GfxFamily, Dispatcher>::handleCompletionFence(ringFence.lastSubmittedFence, ringFence);
}

template <typename GfxFamily, typename Dispatcher>
bool WddmDirectSubmission<GfxFamily, Dispatcher>::allocateOsResources() {
    bool ret = wddm->getWddmInterface()->createFenceForDirectSubmission(ringFence, *this->osContextWin);
    perfLogResidencyVariadicLog(wddm->getResidencyLogger(), "ULLS resource allocation finished with: %d\n", ret);

    this->ringBufferEndCompletionTagData.tagAddress = this->semaphoreGpuVa + offsetof(RingSemaphoreData, tagAllocation);
    this->ringBufferEndCompletionTagData.tagValue = 0u;

    DirectSubmissionHw<GfxFamily, Dispatcher>::allocateOsResources();

    return ret;
}

template <typename GfxFamily, typename Dispatcher>
bool WddmDirectSubmission<GfxFamily, Dispatcher>::submit(uint64_t gpuAddress, size_t size, const ResidencyContainer *allocationsForResidency) {
    perfLogResidencyVariadicLog(wddm->getResidencyLogger(), "ULLS Submit to GPU\n");
    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandBufferHeader.get());
    pHeader->RequiresCoherency = false;

    pHeader->UmdRequestedSliceState = 0;
    pHeader->UmdRequestedEUCount = wddm->getRequestedEUCount();

    pHeader->UmdRequestedSubsliceCount = 0;
    pHeader->NeedsMidBatchPreEmptionSupport = true;

    WddmSubmitArguments submitArgs = {};
    submitArgs.contextHandle = osContextWin->getWddmContextHandle();
    submitArgs.hwQueueHandle = osContextWin->getHwQueue().handle;
    submitArgs.monitorFence = &ringFence;

    return wddm->submit(gpuAddress, size, pHeader, submitArgs);
}

template <typename GfxFamily, typename Dispatcher>
bool WddmDirectSubmission<GfxFamily, Dispatcher>::handleResidency() {
    wddm->waitOnPagingFenceFromCpu(this->lastSubmittedThrottle == QueueThrottle::LOW);
    perfLogResidencyVariadicLog(wddm->getResidencyLogger(), "ULLS residency wait exit\n");
    return true;
}

template <typename GfxFamily, typename Dispatcher>
void WddmDirectSubmission<GfxFamily, Dispatcher>::handleStopRingBuffer() {
    if (this->disableMonitorFence) {
        updateTagValueImplForSwitchRingBuffer(this->currentRingBuffer);
        updateTagValueImpl(this->currentRingBuffer);
    }
}

template <typename GfxFamily, typename Dispatcher>
void WddmDirectSubmission<GfxFamily, Dispatcher>::handleSwitchRingBuffers(ResidencyContainer *allocationsForResidency) {
    if (this->disableMonitorFence) {
        updateTagValueImplForSwitchRingBuffer(this->previousRingBuffer);
    }
}

template <typename GfxFamily, typename Dispatcher>
uint64_t WddmDirectSubmission<GfxFamily, Dispatcher>::updateTagValue(bool requireMonitorFence) {
    if (this->detectGpuHang) {
        bool osHang = wddm->isGpuHangDetected(*osContextWin);
        bool ringHang = *ringFence.cpuAddress == Wddm::gpuHangIndication;

        if (osHang || ringHang) {
            wddm->getDeviceState();
            return DirectSubmissionHw<GfxFamily, Dispatcher>::updateTagValueFail;
        }
    }

    if (requireMonitorFence) {
        return this->updateTagValueImpl(this->currentRingBuffer);
    }
    MonitoredFence &currentFence = osContextWin->getResidencyController().getMonitoredFence();
    return currentFence.currentFenceValue;
}

template <typename GfxFamily, typename Dispatcher>
bool WddmDirectSubmission<GfxFamily, Dispatcher>::dispatchMonitorFenceRequired(bool requireMonitorFence) {
    return !this->disableMonitorFence || requireMonitorFence;
}

template <typename GfxFamily, typename Dispatcher>
uint64_t WddmDirectSubmission<GfxFamily, Dispatcher>::updateTagValueImpl(uint32_t completionBufferIndex) {
    MonitoredFence &currentFence = osContextWin->getResidencyController().getMonitoredFence();

    currentFence.lastSubmittedFence = currentFence.currentFenceValue;
    currentFence.currentFenceValue++;
    this->ringBuffers[completionBufferIndex].completionFence = currentFence.lastSubmittedFence;

    return currentFence.lastSubmittedFence;
}
template <typename GfxFamily, typename Dispatcher>
void WddmDirectSubmission<GfxFamily, Dispatcher>::updateTagValueImplForSwitchRingBuffer(uint32_t completionBufferIndex) {
    this->ringBufferEndCompletionTagData.tagValue++;
    this->ringBuffers[completionBufferIndex].completionFenceForSwitch = this->ringBufferEndCompletionTagData.tagValue;
}

template <typename GfxFamily, typename Dispatcher>
void WddmDirectSubmission<GfxFamily, Dispatcher>::handleCompletionFence(uint64_t completionValue, MonitoredFence &fence) {
    wddm->waitFromCpu(completionValue, fence, false);
}

template <typename GfxFamily, typename Dispatcher>
void WddmDirectSubmission<GfxFamily, Dispatcher>::getTagAddressValue(TagData &tagData) {
    MonitoredFence &currentFence = osContextWin->getResidencyController().getMonitoredFence();
    auto gmmHelper = wddm->getRootDeviceEnvironment().getGmmHelper();

    tagData.tagAddress = gmmHelper->canonize(currentFence.gpuAddress);
    tagData.tagValue = currentFence.currentFenceValue;
}

template <typename GfxFamily, typename Dispatcher>
void WddmDirectSubmission<GfxFamily, Dispatcher>::getTagAddressValueForRingSwitch(TagData &tagData) {
    tagData.tagAddress = this->ringBufferEndCompletionTagData.tagAddress;
    tagData.tagValue = this->ringBufferEndCompletionTagData.tagValue + 1;
}

template <typename GfxFamily, typename Dispatcher>
void WddmDirectSubmission<GfxFamily, Dispatcher>::dispatchStopRingBufferSection() {
    TagData currentTagData = {};
    getTagAddressValueForRingSwitch(currentTagData);
    Dispatcher::dispatchMonitorFence(this->ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, this->rootDeviceEnvironment, this->partitionedMode, this->dcFlushRequired, this->notifyKmdDuringMonitorFence);
    getTagAddressValue(currentTagData);
    Dispatcher::dispatchMonitorFence(this->ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, this->rootDeviceEnvironment, this->partitionedMode, this->dcFlushRequired, this->notifyKmdDuringMonitorFence);
}

template <typename GfxFamily, typename Dispatcher>
size_t WddmDirectSubmission<GfxFamily, Dispatcher>::dispatchStopRingBufferSectionSize() {
    return 2 * Dispatcher::getSizeMonitorFence(this->rootDeviceEnvironment);
}
template <typename GfxFamily, typename Dispatcher>
inline bool WddmDirectSubmission<GfxFamily, Dispatcher>::isCompleted(uint32_t ringBufferIndex) {
    auto taskCount = this->ringBuffers[ringBufferIndex].completionFenceForSwitch;
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
void WddmDirectSubmission<GfxFamily, Dispatcher>::updateMonitorFenceValueForResidencyList(ResidencyContainer *allocationsForResidency) {
    if (allocationsForResidency == nullptr) {
        return;
    }
    const auto currentFence = osContextWin->getResidencyController().getMonitoredFence().currentFenceValue;
    auto contextId = osContextWin->getContextId();
    for (uint32_t i = 0; i < allocationsForResidency->size(); i++) {
        WddmAllocation *allocation = static_cast<WddmAllocation *>((*allocationsForResidency)[i]);
        // Update fence value not to early destroy / evict allocation
        allocation->updateCompletionDataForAllocationAndFragments(currentFence, contextId);
    }
}

/**
 * @brief Unblocks pipeline by waiting for paging fence value and signals semaphore with latest value.
 * Signals semaphore with latest fence value instead of passed one to unblock immediately subsequent semaphores.
 *
 * @param[in] pagingFenceValue paging fence submitted from user thread to wait for.
 */
template <typename GfxFamily, typename Dispatcher>
inline void WddmDirectSubmission<GfxFamily, Dispatcher>::unblockPagingFenceSemaphore(uint64_t pagingFenceValue) {
    this->wddm->waitOnPagingFenceFromCpu(pagingFenceValue, true);

    typename DirectSubmissionHw<GfxFamily, Dispatcher>::SemaphoreFenceHelper fence(*this);
    this->semaphoreData->pagingFenceCounter = static_cast<uint32_t>(*this->wddm->getPagingFenceAddress());
}

template <typename GfxFamily, typename Dispatcher>
inline void WddmDirectSubmission<GfxFamily, Dispatcher>::makeGlobalFenceAlwaysResident() {
    if (this->globalFenceAllocation != nullptr) {
        DirectSubmissionAllocations allocations;
        allocations.push_back(this->globalFenceAllocation);
        UNRECOVERABLE_IF(!this->makeResourcesResident(allocations));
    }
}

} // namespace NEO
