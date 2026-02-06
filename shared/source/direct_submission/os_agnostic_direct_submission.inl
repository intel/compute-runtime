/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_simulated_hw.h"
#include "shared/source/device/device.h"
#include "shared/source/direct_submission/os_agnostic_direct_submission.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/memory_operations_handler.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/source/utilities/arrayref.h"
namespace NEO {

template <typename GfxFamily, typename Dispatcher>
OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::OsAgnosticDirectSubmission(const DirectSubmissionInputParams &inputParams)
    : DirectSubmissionHw<GfxFamily, Dispatcher>(inputParams) {

    this->completionFenceAllocation = inputParams.completionFenceAllocation;

    if (this->completionFenceAllocation) {
        if (this->miMemFenceRequired) {
            this->gpuVaForAdditionalSynchronizationWA = this->completionFenceAllocation->getGpuAddress() + 8u;
        }
    }
}

template <typename GfxFamily, typename Dispatcher>
OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::~OsAgnosticDirectSubmission() {
    if (this->ringStart) {
        this->stopRingBuffer(true);
    }
    this->deallocateResources();
}

template <typename GfxFamily, typename Dispatcher>
TaskCountType *OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::getCompletionValuePointer() {
    return &this->completionFenceValue;
}

template <typename GfxFamily, typename Dispatcher>
void OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::ensureRingCompletion() {
    const_cast<CommandStreamReceiver &>(this->csr).pollForCompletion(false);
}

template <typename GfxFamily, typename Dispatcher>
bool OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::allocateOsResources() {

    DirectSubmissionHw<GfxFamily, Dispatcher>::allocateOsResources();
    this->currentTagData.tagAddress = this->semaphoreGpuVa + offsetof(RingSemaphoreData, tagAllocation);
    this->currentTagData.tagValue = 0u;
    return true;
}

template <typename GfxFamily, typename Dispatcher>
bool OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::submit(uint64_t gpuAddress, size_t size, const ResidencyContainer *allocationsForResidency) {

    auto *csrSimulatedHw = reinterpret_cast<const CommandStreamReceiverSimulatedHw<GfxFamily> *>(&this->csr);
    if (size) {
        auto currentBase = this->ringCommandStream.getGraphicsAllocation()->getGpuAddress();
        auto offset = ptrDiff(gpuAddress, currentBase);
        auto batchBuffer = ptrOffset(this->ringCommandStream.getGraphicsAllocation()->getUnderlyingBuffer(), static_cast<size_t>(offset));
        auto memoryBank = csrSimulatedHw->getMemoryBank(this->ringCommandStream.getGraphicsAllocation());

        csrSimulatedHw->submit(*this->ringCommandStream.getGraphicsAllocation(), gpuAddress, batchBuffer, size, memoryBank, MemoryConstants::pageSize64k, false, allocationsForResidency);
    }
    return true;
}

template <typename GfxFamily, typename Dispatcher>
void OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::unblockGpu() {
    DirectSubmissionHw<GfxFamily, Dispatcher>::unblockGpu();
    const_cast<CommandStreamReceiver &>(this->csr).writeMemory(*this->semaphores, true, offsetof(RingSemaphoreData, queueWorkCount), sizeof(RingSemaphoreData::queueWorkCount));
}

template <typename GfxFamily, typename Dispatcher>
bool OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::handleResidency(const ResidencyContainer *allocationsForResidency) {

    if (allocationsForResidency) {
        auto *csrSimulatedHw = reinterpret_cast<const CommandStreamReceiverSimulatedHw<GfxFamily> *>(&this->csr);
        csrSimulatedHw->uploadRingAndCommandBuffers(*this->ringCommandStream.getGraphicsAllocation(), this->ringCommandStream.getGraphicsAllocation()->getGpuAddress(), this->ringCommandStream.getUsed(), allocationsForResidency);
    }
    return true;
}

template <typename GfxFamily, typename Dispatcher>
void OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::handleSwitchRingBuffers(ResidencyContainer *allocationsForResidency) {
}

template <typename GfxFamily, typename Dispatcher>
uint64_t OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::updateTagValue(bool requireMonitorFence) {
    return 0;
}

template <typename GfxFamily, typename Dispatcher>
bool OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::dispatchMonitorFenceRequired(bool requireMonitorFence) {
    return requireMonitorFence;
}

template <typename GfxFamily, typename Dispatcher>
void OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::getTagAddressValue(TagData &tagData) {
    tagData.tagAddress = this->currentTagData.tagAddress;
    tagData.tagValue = this->currentTagData.tagValue + 1;
}

template <typename GfxFamily, typename Dispatcher>
void OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::getTagAddressValueForRingSwitch(TagData &tagData) {
    getTagAddressValue(tagData);
}

template <typename GfxFamily, typename Dispatcher>
void OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::dispatchStopRingBufferSection() {
    TagData currentTagData = {};
    getTagAddressValue(currentTagData);
    Dispatcher::dispatchMonitorFence(this->ringCommandStream, currentTagData.tagAddress, currentTagData.tagValue, this->rootDeviceEnvironment, this->partitionedMode, this->dcFlushRequired, this->notifyKmdDuringMonitorFence);
}

template <typename GfxFamily, typename Dispatcher>
size_t OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::dispatchStopRingBufferSectionSize() {
    return Dispatcher::getSizeMonitorFence(this->rootDeviceEnvironment);
}

template <typename GfxFamily, typename Dispatcher>
inline bool OsAgnosticDirectSubmission<GfxFamily, Dispatcher>::isCompleted(uint32_t ringBufferIndex) {
    auto taskCount = this->ringBuffers[ringBufferIndex].completionFence;
    auto pollAddress = this->tagAddress;

    const_cast<CommandStreamReceiver &>(this->csr).downloadAllocation(*this->ringBuffers[ringBufferIndex].ringBuffer);
    for (uint32_t i = 0; i < this->activeTiles; i++) {
        if (*pollAddress < taskCount) {
            return false;
        }
        pollAddress = ptrOffset(pollAddress, this->immWritePostSyncOffset);
    }
    return true;
}

} // namespace NEO
