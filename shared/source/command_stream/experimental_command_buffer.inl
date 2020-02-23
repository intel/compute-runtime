/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/experimental_command_buffer.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {

template <typename GfxFamily>
void ExperimentalCommandBuffer::injectBufferStart(LinearStream &parentStream, size_t cmdBufferOffset) {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    auto pCmd = parentStream.getSpaceForCmd<MI_BATCH_BUFFER_START>();
    auto commandStreamReceiverHw = static_cast<CommandStreamReceiverHw<GfxFamily> *>(commandStreamReceiver);
    commandStreamReceiverHw->addBatchBufferStart(pCmd, currentStream->getGraphicsAllocation()->getGpuAddress() + cmdBufferOffset, true);
}

template <typename GfxFamily>
size_t ExperimentalCommandBuffer::getRequiredInjectionSize() noexcept {
    using MI_BATCH_BUFFER_START = typename GfxFamily::MI_BATCH_BUFFER_START;
    return sizeof(MI_BATCH_BUFFER_START);
}

template <typename GfxFamily>
size_t ExperimentalCommandBuffer::programExperimentalCommandBuffer() {
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    getCS(getTotalExperimentalSize<GfxFamily>());

    size_t returnOffset = currentStream->getUsed();

    //begin timestamp
    addTimeStampPipeControl<GfxFamily>();

    addExperimentalCommands<GfxFamily>();

    //end timestamp
    addTimeStampPipeControl<GfxFamily>();

    //end
    auto pCmd = currentStream->getSpaceForCmd<MI_BATCH_BUFFER_END>();
    *pCmd = GfxFamily::cmdInitBatchBufferEnd;

    return returnOffset;
}

template <typename GfxFamily>
size_t ExperimentalCommandBuffer::getTotalExperimentalSize() noexcept {
    using MI_BATCH_BUFFER_END = typename GfxFamily::MI_BATCH_BUFFER_END;

    size_t size = sizeof(MI_BATCH_BUFFER_END) + getTimeStampPipeControlSize<GfxFamily>() + getExperimentalCommandsSize<GfxFamily>();
    return size;
}

template <typename GfxFamily>
size_t ExperimentalCommandBuffer::getTimeStampPipeControlSize() noexcept {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    // Two P_C for timestamps
    return 2 * MemorySynchronizationCommands<GfxFamily>::getSizeForPipeControlWithPostSyncOperation(
                   *commandStreamReceiver->peekExecutionEnvironment().rootDeviceEnvironments[commandStreamReceiver->getRootDeviceIndex()]->getHardwareInfo());
}

template <typename GfxFamily>
void ExperimentalCommandBuffer::addTimeStampPipeControl() {
    using PIPE_CONTROL = typename GfxFamily::PIPE_CONTROL;

    uint64_t timeStampAddress = timestamps->getGpuAddress() + timestampsOffset;

    MemorySynchronizationCommands<GfxFamily>::obtainPipeControlAndProgramPostSyncOperation(
        *currentStream, PIPE_CONTROL::POST_SYNC_OPERATION_WRITE_TIMESTAMP, timeStampAddress, 0llu,
        false, *commandStreamReceiver->peekExecutionEnvironment().rootDeviceEnvironments[commandStreamReceiver->getRootDeviceIndex()]->getHardwareInfo());

    //moving to next chunk
    timestampsOffset += sizeof(uint64_t);

    DEBUG_BREAK_IF(timestamps->getUnderlyingBufferSize() < timestampsOffset);
}

template <typename GfxFamily>
void ExperimentalCommandBuffer::addExperimentalCommands() {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;

    uint32_t *semaphoreData = reinterpret_cast<uint32_t *>(ptrOffset(experimentalAllocation->getUnderlyingBuffer(), experimentalAllocationOffset));
    *semaphoreData = 1;
    uint64_t gpuAddr = experimentalAllocation->getGpuAddress() + experimentalAllocationOffset;

    auto semaphoreCmd = currentStream->getSpaceForCmd<MI_SEMAPHORE_WAIT>();
    *semaphoreCmd = GfxFamily::cmdInitMiSemaphoreWait;
    semaphoreCmd->setCompareOperation(MI_SEMAPHORE_WAIT::COMPARE_OPERATION_SAD_EQUAL_SDD);
    semaphoreCmd->setSemaphoreDataDword(*semaphoreData);
    semaphoreCmd->setSemaphoreGraphicsAddress(gpuAddr);
}

template <typename GfxFamily>
size_t ExperimentalCommandBuffer::getExperimentalCommandsSize() noexcept {
    using MI_SEMAPHORE_WAIT = typename GfxFamily::MI_SEMAPHORE_WAIT;
    return sizeof(MI_SEMAPHORE_WAIT);
}

} // namespace NEO
