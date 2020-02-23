/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/windows/wddm_direct_submission.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/windows/os_context_win.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"
#include "shared/source/os_interface/windows/wddm/wddm_interface.h"
#include "shared/source/os_interface/windows/wddm_allocation.h"
#include "shared/source/os_interface/windows/wddm_memory_operations_handler.h"
#include "shared/source/utilities/arrayref.h"

namespace NEO {

// Initialize COMMAND_BUFFER_HEADER         Type PatchList  Streamer Perf Tag
DECLARE_COMMAND_BUFFER(CommandBufferHeader, UMD_OCL, FALSE, FALSE, PERFTAG_OCL);

template <typename GfxFamily>
WddmDirectSubmission<GfxFamily>::WddmDirectSubmission(Device &device,
                                                      std::unique_ptr<Dispatcher> cmdDispatcher,
                                                      OsContext &osContext)
    : DirectSubmissionHw<GfxFamily>(device, std::move(cmdDispatcher), osContext) {
    osContextWin = reinterpret_cast<OsContextWin *>(&osContext);
    wddm = osContextWin->getWddm();
    commandBufferHeader = std::make_unique<COMMAND_BUFFER_HEADER_REC>();
    *(commandBufferHeader.get()) = CommandBufferHeader;
    if (device.getPreemptionMode() != PreemptionMode::Disabled) {
        commandBufferHeader->NeedsMidBatchPreEmptionSupport = true;
    }
}

template <typename GfxFamily>
WddmDirectSubmission<GfxFamily>::~WddmDirectSubmission() {
    if (ringStart) {
        stopRingBuffer();
        WddmDirectSubmission<GfxFamily>::handleCompletionRingBuffer(ringFence.lastSubmittedFence, ringFence);
    }
    deallocateResources();
    wddm->getWddmInterface()->destroyMonitorFence(ringFence);
}

template <typename GfxFamily>
bool WddmDirectSubmission<GfxFamily>::allocateOsResources(DirectSubmissionAllocations &allocations) {
    //for now only WDDM2.0
    UNRECOVERABLE_IF(wddm->getWddmVersion() != WddmVersion::WDDM_2_0);

    WddmMemoryOperationsHandler *memoryInterface =
        static_cast<WddmMemoryOperationsHandler *>(device.getRootDeviceEnvironment().memoryOperationsInterface.get());

    bool ret = wddm->getWddmInterface()->createMonitoredFence(ringFence);
    ringFence.currentFenceValue = 1;
    if (ret) {
        ret = memoryInterface->makeResident(ArrayRef<GraphicsAllocation *>(allocations)) == MemoryOperationsStatus::SUCCESS;
    }

    return ret;
}

template <typename GfxFamily>
bool WddmDirectSubmission<GfxFamily>::submit(uint64_t gpuAddress, size_t size) {
    COMMAND_BUFFER_HEADER *pHeader = reinterpret_cast<COMMAND_BUFFER_HEADER *>(commandBufferHeader.get());
    pHeader->RequiresCoherency = false;

    pHeader->UmdRequestedSliceState = 0;
    pHeader->UmdRequestedEUCount = wddm->getGtSysInfo()->EUCount / wddm->getGtSysInfo()->SubSliceCount;

    pHeader->UmdRequestedSubsliceCount = 0;
    pHeader->NeedsMidBatchPreEmptionSupport = true;

    WddmSubmitArguments submitArgs = {};
    submitArgs.contextHandle = osContextWin->getWddmContextHandle();
    submitArgs.hwQueueHandle = osContextWin->getHwQueue().handle;
    submitArgs.monitorFence = &ringFence;

    return wddm->submit(gpuAddress, size, pHeader, submitArgs);
}

template <typename GfxFamily>
bool WddmDirectSubmission<GfxFamily>::handleResidency() {
    wddm->waitOnPagingFenceFromCpu();
    return true;
}

template <typename GfxFamily>
uint64_t WddmDirectSubmission<GfxFamily>::switchRingBuffers() {
    GraphicsAllocation *nextRingBuffer = switchRingBuffersAllocations();
    void *flushPtr = ringCommandStream.getSpace(0);
    uint64_t currentBufferGpuVa = getCommandBufferPositionGpuAddress(flushPtr);

    if (ringStart) {
        dispatchSwitchRingBufferSection(nextRingBuffer->getGpuAddress());
        cpuCachelineFlush(flushPtr, getSizeSwitchRingBufferSection());
    }

    ringCommandStream.replaceBuffer(nextRingBuffer->getUnderlyingBuffer(), ringCommandStream.getMaxAvailableSpace());
    ringCommandStream.replaceGraphicsAllocation(nextRingBuffer);

    if (ringStart) {
        if (completionRingBuffers[currentRingBuffer] != 0) {
            MonitoredFence &currentFence = osContextWin->getResidencyController().getMonitoredFence();
            handleCompletionRingBuffer(completionRingBuffers[currentRingBuffer], currentFence);
        }
    }

    return currentBufferGpuVa;
}

template <typename GfxFamily>
uint64_t WddmDirectSubmission<GfxFamily>::updateTagValue() {
    MonitoredFence &currentFence = osContextWin->getResidencyController().getMonitoredFence();

    currentFence.lastSubmittedFence = currentFence.currentFenceValue;
    currentFence.currentFenceValue++;
    completionRingBuffers[currentRingBuffer] = currentFence.lastSubmittedFence;

    return currentFence.lastSubmittedFence;
}

template <typename GfxFamily>
void WddmDirectSubmission<GfxFamily>::handleCompletionRingBuffer(uint64_t completionValue, MonitoredFence &fence) {
    wddm->waitFromCpu(completionValue, fence);
}

template <typename GfxFamily>
void WddmDirectSubmission<GfxFamily>::getTagAddressValue(TagData &tagData) {
    MonitoredFence &currentFence = osContextWin->getResidencyController().getMonitoredFence();

    tagData.tagAddress = GmmHelper::canonize(currentFence.gpuAddress);
    tagData.tagValue = currentFence.currentFenceValue;
}

} // namespace NEO
