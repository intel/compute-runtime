/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

// Need to suppress warining 4005 caused by hw_cmds.h and wddm.h order.
// Current order must be preserved due to two versions of igfxfmid.h
#pragma warning(push)
#pragma warning(disable : 4005)
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/direct_submission/windows/wddm_direct_submission.h"
#include "shared/source/gmm_helper/page_table_mngr.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/helpers/windows/gmm_callbacks.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm_residency_logger.h"
#include "shared/source/os_interface/windows/wddm_device_command_stream.h"

#pragma warning(pop)

#include "shared/source/command_stream/submissions_aggregator.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm_memory_manager.h"

namespace NEO {

// Initialize COMMAND_BUFFER_HEADER         Type PatchList  Streamer Perf Tag
DECLARE_COMMAND_BUFFER(CommandBufferHeader, UMD_OCL, FALSE, FALSE, PERFTAG_OCL);

template <typename GfxFamily>
WddmCommandStreamReceiver<GfxFamily>::WddmCommandStreamReceiver(ExecutionEnvironment &executionEnvironment,
                                                                uint32_t rootDeviceIndex,
                                                                const DeviceBitfield deviceBitfield)
    : BaseClass(executionEnvironment, rootDeviceIndex, deviceBitfield) {

    notifyAubCaptureImpl = DeviceCallbacks<GfxFamily>::notifyAubCapture;
    this->wddm = executionEnvironment.rootDeviceEnvironments[rootDeviceIndex]->osInterface->getDriverModel()->as<Wddm>();

    PreemptionMode preemptionMode = PreemptionHelper::getDefaultPreemptionMode(this->peekHwInfo());

    commandBufferHeader = new COMMAND_BUFFER_HEADER;
    *commandBufferHeader = CommandBufferHeader;

    if (preemptionMode != PreemptionMode::Disabled) {
        commandBufferHeader->NeedsMidBatchPreEmptionSupport = true;
    }

    this->dispatchMode = DispatchMode::batchedDispatch;

    if (ApiSpecificConfig::getApiType() == ApiSpecificConfig::L0) {
        this->dispatchMode = DispatchMode::immediateDispatch;
    }

    if (debugManager.flags.CsrDispatchMode.get()) {
        this->dispatchMode = (DispatchMode)debugManager.flags.CsrDispatchMode.get();
    }
}

template <typename GfxFamily>
WddmCommandStreamReceiver<GfxFamily>::~WddmCommandStreamReceiver() {
    if (commandBufferHeader)
        delete commandBufferHeader;
}

template <typename GfxFamily>
SubmissionStatus WddmCommandStreamReceiver<GfxFamily>::flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) {
    this->printDeviceIndex();
    auto commandStreamAddress = ptrOffset(batchBuffer.commandBufferAllocation->getGpuAddress(), batchBuffer.startOffset);

    allocationsForResidency.push_back(batchBuffer.commandBufferAllocation);
    batchBuffer.commandBufferAllocation->updateResidencyTaskCount(this->taskCount, this->osContext->getContextId());
    perfLogResidencyVariadicLog(wddm->getResidencyLogger(), "Wddm CSR processing residency set: %zu\n", allocationsForResidency.size());

    auto submissionStatus = this->processResidency(allocationsForResidency, 0u);
    if (submissionStatus != SubmissionStatus::success) {
        return submissionStatus;
    }
    batchBuffer.allocationsForResidency = &allocationsForResidency;

    auto mustWaitForResidency = this->requiresBlockingResidencyHandling;
    mustWaitForResidency |= !this->executionEnvironment.directSubmissionController.get();
    batchBuffer.pagingFenceSemInfo.requiresBlockingResidencyHandling = mustWaitForResidency;

    auto pagingFenceValue = this->wddm->getCurrentPagingFenceValue();
    if (this->validForEnqueuePagingFence(pagingFenceValue)) {
        auto waitEnqueued = this->enqueueWaitForPagingFence(pagingFenceValue);
        if (waitEnqueued) {
            batchBuffer.pagingFenceSemInfo.pagingFenceValue = pagingFenceValue;
            this->lastEnqueuedPagingFenceValue = pagingFenceValue;
        }
    }

    if (this->directSubmission.get()) {
        auto ret = this->directSubmission->dispatchCommandBuffer(batchBuffer, *(this->flushStamp.get()));
        if (ret == false) {
            return SubmissionStatus::failed;
        }
        return SubmissionStatus::success;
    }
    if (this->blitterDirectSubmission.get()) {
        auto ret = this->blitterDirectSubmission->dispatchCommandBuffer(batchBuffer, *(this->flushStamp.get()));
        if (ret == false) {
            return SubmissionStatus::failed;
        }
        return SubmissionStatus::success;
    }

    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandBufferHeader);
    pHeader->RequiresCoherency = false;

    pHeader->UmdRequestedSliceState = 0;
    pHeader->UmdRequestedEUCount = wddm->getRequestedEUCount();

    const uint32_t maxRequestedSubsliceCount = 7;
    switch (batchBuffer.throttle) {
    case QueueThrottle::LOW:
    case QueueThrottle::MEDIUM:
        pHeader->UmdRequestedSubsliceCount = 0;
        break;
    case QueueThrottle::HIGH:
        pHeader->UmdRequestedSubsliceCount = (wddm->getGtSysInfo()->SubSliceCount <= maxRequestedSubsliceCount) ? wddm->getGtSysInfo()->SubSliceCount : 0;
        break;
    }

    if (wddm->isKmDafEnabled()) {
        this->kmDafLockAllocations(allocationsForResidency);
    }

    auto osContextWin = static_cast<OsContextWin *>(this->osContext);
    WddmSubmitArguments submitArgs = {};
    submitArgs.contextHandle = osContextWin->getWddmContextHandle();
    submitArgs.hwQueueHandle = osContextWin->getHwQueue().handle;
    submitArgs.monitorFence = &osContextWin->getResidencyController().getMonitoredFence();
    auto status = wddm->submit(commandStreamAddress, batchBuffer.usedSize - batchBuffer.startOffset, commandBufferHeader, submitArgs);

    this->flushStamp->setStamp(submitArgs.monitorFence->lastSubmittedFence);
    if (status == false) {
        return SubmissionStatus::failed;
    }

    return SubmissionStatus::success;
}

template <typename GfxFamily>
SubmissionStatus WddmCommandStreamReceiver<GfxFamily>::processResidency(ResidencyContainer &allocationsForResidency, uint32_t handleId) {
    return static_cast<OsContextWin *>(this->osContext)->getResidencyController().makeResidentResidencyAllocations(allocationsForResidency, this->requiresBlockingResidencyHandling) ? SubmissionStatus::success : SubmissionStatus::outOfMemory;
}

template <typename GfxFamily>
void WddmCommandStreamReceiver<GfxFamily>::processEviction() {
}

template <typename GfxFamily>
WddmMemoryManager *WddmCommandStreamReceiver<GfxFamily>::getMemoryManager() const {
    return static_cast<WddmMemoryManager *>(CommandStreamReceiver::getMemoryManager());
}

template <typename GfxFamily>
bool WddmCommandStreamReceiver<GfxFamily>::waitForFlushStamp(FlushStamp &flushStampToWait) {
    return wddm->waitFromCpu(flushStampToWait, static_cast<OsContextWin *>(this->osContext)->getResidencyController().getMonitoredFence(), false);
}

template <typename GfxFamily>
bool WddmCommandStreamReceiver<GfxFamily>::isTlbFlushRequiredForStateCacheFlush() {
    return true;
}

template <typename GfxFamily>
GmmPageTableMngr *WddmCommandStreamReceiver<GfxFamily>::createPageTableManager() {
    GMM_TRANSLATIONTABLE_CALLBACKS ttCallbacks = {};
    ttCallbacks.pfWriteL3Adr = TTCallbacks<GfxFamily>::writeL3Address;

    auto rootDeviceEnvironment = this->executionEnvironment.rootDeviceEnvironments[this->rootDeviceIndex].get();

    GmmPageTableMngr *gmmPageTableMngr = GmmPageTableMngr::create(rootDeviceEnvironment->getGmmClientContext(), TT_TYPE::AUXTT, &ttCallbacks);
    gmmPageTableMngr->setCsrHandle(this);
    this->pageTableManager.reset(gmmPageTableMngr);
    return gmmPageTableMngr;
}

template <typename GfxFamily>
void WddmCommandStreamReceiver<GfxFamily>::flushMonitorFence(bool notifyKmd) {
    if (this->directSubmission.get()) {
        this->directSubmission->flushMonitorFence(notifyKmd);
    } else if (this->blitterDirectSubmission.get()) {
        this->blitterDirectSubmission->flushMonitorFence(notifyKmd);
    }
}
template <typename GfxFamily>
void WddmCommandStreamReceiver<GfxFamily>::kmDafLockAllocations(ResidencyContainer &allocationsForResidency) {
    for (auto &graphicsAllocation : allocationsForResidency) {
        if ((AllocationType::linearStream == graphicsAllocation->getAllocationType()) ||
            (AllocationType::fillPattern == graphicsAllocation->getAllocationType()) ||
            (AllocationType::commandBuffer == graphicsAllocation->getAllocationType())) {
            wddm->kmDafLock(static_cast<WddmAllocation *>(graphicsAllocation)->getDefaultHandle());
        }
    }
}

template <typename GfxFamily>
CommandStreamReceiver *createWddmDeviceCommandStreamReceiver(bool withAubDump,
                                                             ExecutionEnvironment &executionEnvironment,
                                                             uint32_t rootDeviceIndex,
                                                             const DeviceBitfield deviceBitfield) {
    if (withAubDump) {
        return new CommandStreamReceiverWithAUBDump<WddmCommandStreamReceiver<GfxFamily>>(ApiSpecificConfig::getName(),
                                                                                          executionEnvironment,
                                                                                          rootDeviceIndex,
                                                                                          deviceBitfield);
    } else {
        return new WddmCommandStreamReceiver<GfxFamily>(executionEnvironment, rootDeviceIndex, deviceBitfield);
    }
}

template <typename GfxFamily>
void WddmCommandStreamReceiver<GfxFamily>::setupContext(OsContext &osContext) {
    this->osContext = &osContext;
    static_cast<OsContextWin *>(this->osContext)->getResidencyController().setCommandStreamReceiver(this);
}

template <typename GfxFamily>
void WddmCommandStreamReceiver<GfxFamily>::addToEvictionContainer(GraphicsAllocation &gfxAllocation) {
    // Eviction allocations are shared with trim callback thread.
    auto lock = static_cast<OsContextWin *>(this->osContext)->getResidencyController().acquireLock();
    this->getEvictionAllocations().push_back(&gfxAllocation);
}

template <typename GfxFamily>
bool WddmCommandStreamReceiver<GfxFamily>::validForEnqueuePagingFence(uint64_t pagingFenceValue) const {
    return !this->requiresBlockingResidencyHandling &&
           pagingFenceValue > *this->wddm->getPagingFenceAddress() &&
           pagingFenceValue > this->lastEnqueuedPagingFenceValue;
}

} // namespace NEO
