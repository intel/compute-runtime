/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/unit_test/helpers/dispatch_flags_helper.h"

#include "gmock/gmock.h"

#include <vector>

using namespace NEO;

class MockCommandStreamReceiver : public CommandStreamReceiver {
  public:
    using CommandStreamReceiver::checkForNewResources;
    using CommandStreamReceiver::checkImplicitFlushForGpuIdle;
    using CommandStreamReceiver::CommandStreamReceiver;
    using CommandStreamReceiver::globalFenceAllocation;
    using CommandStreamReceiver::internalAllocationStorage;
    using CommandStreamReceiver::latestFlushedTaskCount;
    using CommandStreamReceiver::latestSentTaskCount;
    using CommandStreamReceiver::newResources;
    using CommandStreamReceiver::requiredThreadArbitrationPolicy;
    using CommandStreamReceiver::tagAddress;
    using CommandStreamReceiver::taskCount;
    using CommandStreamReceiver::useGpuIdleImplicitFlush;
    using CommandStreamReceiver::useNewResourceImplicitFlush;

    bool waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMicroseconds, uint32_t taskCountToWait) override {
        waitForCompletionWithTimeoutCalled++;
        return true;
    }
    bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;

    bool isMultiOsContextCapable() const override { return multiOsContextCapable; }

    CompletionStamp flushTask(
        LinearStream &commandStream,
        size_t commandStreamStart,
        const IndirectHeap &dsh,
        const IndirectHeap &ioh,
        const IndirectHeap &ssh,
        uint32_t taskLevel,
        DispatchFlags &dispatchFlags,
        Device &device) override;

    bool flushBatchedSubmissions() override {
        if (flushBatchedSubmissionsCallCounter) {
            (*flushBatchedSubmissionsCallCounter)++;
        }
        return true;
    }

    void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool quickKmdSleep, bool forcePowerSavingMode) override {
    }

    uint32_t blitBuffer(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, bool profilingEnabled) override { return taskCount; };

    CommandStreamReceiverType getType() override {
        return CommandStreamReceiverType::CSR_HW;
    }

    void downloadAllocations() override {
        downloadAllocationsCalled = true;
    }

    void programHardwareContext(LinearStream &cmdStream) override {
        programHardwareContextCalled = true;
    }
    size_t getCmdsSizeForHardwareContext() const override {
        return 0;
    }

    volatile uint32_t *getTagAddress() const override {
        if (callParentGetTagAddress) {
            return CommandStreamReceiver::getTagAddress();
        }
        return const_cast<volatile uint32_t *>(&mockTagAddress);
    }

    std::vector<char> instructionHeapReserveredData;
    int *flushBatchedSubmissionsCallCounter = nullptr;
    uint32_t waitForCompletionWithTimeoutCalled = 0;
    uint32_t mockTagAddress = 0;
    bool multiOsContextCapable = false;
    bool downloadAllocationsCalled = false;
    bool programHardwareContextCalled = false;
    bool callParentGetTagAddress = true;
};

template <typename GfxFamily>
class MockCsrHw2 : public CommandStreamReceiverHw<GfxFamily> {
  public:
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw;
    using CommandStreamReceiverHw<GfxFamily>::csrSizeRequestFlags;
    using CommandStreamReceiverHw<GfxFamily>::flushStamp;
    using CommandStreamReceiverHw<GfxFamily>::programL3;
    using CommandStreamReceiverHw<GfxFamily>::programVFEState;
    using CommandStreamReceiver::commandStream;
    using CommandStreamReceiver::dispatchMode;
    using CommandStreamReceiver::globalFenceAllocation;
    using CommandStreamReceiver::isPreambleSent;
    using CommandStreamReceiver::lastSentCoherencyRequest;
    using CommandStreamReceiver::mediaVfeStateDirty;
    using CommandStreamReceiver::nTo1SubmissionModelEnabled;
    using CommandStreamReceiver::pageTableManagerInitialized;
    using CommandStreamReceiver::requiredScratchSize;
    using CommandStreamReceiver::requiredThreadArbitrationPolicy;
    using CommandStreamReceiver::taskCount;
    using CommandStreamReceiver::taskLevel;
    using CommandStreamReceiver::timestampPacketWriteEnabled;
    using CommandStreamReceiver::useGpuIdleImplicitFlush;
    using CommandStreamReceiver::useNewResourceImplicitFlush;

    MockCsrHw2(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex, const DeviceBitfield deviceBitfield)
        : CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw(executionEnvironment, rootDeviceIndex, deviceBitfield) {}

    SubmissionAggregator *peekSubmissionAggregator() {
        return this->submissionAggregator.get();
    }

    void overrideSubmissionAggregator(SubmissionAggregator *newSubmissionsAggregator) {
        this->submissionAggregator.reset(newSubmissionsAggregator);
    }

    uint64_t peekTotalMemoryUsed() {
        return this->totalMemoryUsed;
    }

    bool peekMediaVfeStateDirty() const { return mediaVfeStateDirty; }

    bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        flushCalledCount++;
        if (recordedCommandBuffer) {
            recordedCommandBuffer->batchBuffer = batchBuffer;
        }
        copyOfAllocations = allocationsForResidency;
        flushStamp->setStamp(flushStamp->peekStamp() + 1);
        return true;
    }

    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap &dsh, const IndirectHeap &ioh,
                              const IndirectHeap &ssh, uint32_t taskLevel, DispatchFlags &dispatchFlags, Device &device) override {
        passedDispatchFlags = dispatchFlags;

        recordedCommandBuffer = std::unique_ptr<CommandBuffer>(new CommandBuffer(device));
        auto completionStamp = CommandStreamReceiverHw<GfxFamily>::flushTask(commandStream, commandStreamStart,
                                                                             dsh, ioh, ssh, taskLevel, dispatchFlags, device);

        if (storeFlushedTaskStream && commandStream.getUsed() > commandStreamStart) {
            storedTaskStreamSize = commandStream.getUsed() - commandStreamStart;
            // Overfetch to allow command parser verify if "big" command is programmed at the end of allocation
            auto overfetchedSize = storedTaskStreamSize + MemoryConstants::cacheLineSize;
            storedTaskStream.reset(new uint8_t[overfetchedSize]);
            memset(storedTaskStream.get(), 0, overfetchedSize);
            memcpy_s(storedTaskStream.get(), storedTaskStreamSize,
                     ptrOffset(commandStream.getCpuBase(), commandStreamStart), storedTaskStreamSize);
        }

        return completionStamp;
    }

    uint32_t blitBuffer(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, bool profilingEnabled) override {
        if (!skipBlitCalls) {
            return CommandStreamReceiverHw<GfxFamily>::blitBuffer(blitPropertiesContainer, blocking, profilingEnabled);
        }
        return taskCount;
    }

    void programHardwareContext(LinearStream &cmdStream) override {
        programHardwareContextCalled = true;
    }

    bool skipBlitCalls = false;
    bool storeFlushedTaskStream = false;
    std::unique_ptr<uint8_t> storedTaskStream;
    size_t storedTaskStreamSize = 0;

    int flushCalledCount = 0;
    std::unique_ptr<CommandBuffer> recordedCommandBuffer = nullptr;
    ResidencyContainer copyOfAllocations;
    DispatchFlags passedDispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    bool programHardwareContextCalled = false;
};
