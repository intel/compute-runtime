/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/command_stream_receiver.h"
#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/flat_batch_buffer_helper_hw.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/string.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/os_interface/os_context.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"

#include "gtest/gtest.h"

#include <optional>
#include <vector>

using namespace NEO;

class MockCommandStreamReceiver : public CommandStreamReceiver {
  public:
    using CommandStreamReceiver::activePartitions;
    using CommandStreamReceiver::baseWaitFunction;
    using CommandStreamReceiver::checkForNewResources;
    using CommandStreamReceiver::checkImplicitFlushForGpuIdle;
    using CommandStreamReceiver::CommandStreamReceiver;
    using CommandStreamReceiver::globalFenceAllocation;
    using CommandStreamReceiver::internalAllocationStorage;
    using CommandStreamReceiver::latestFlushedTaskCount;
    using CommandStreamReceiver::latestSentTaskCount;
    using CommandStreamReceiver::newResources;
    using CommandStreamReceiver::osContext;
    using CommandStreamReceiver::postSyncWriteOffset;
    using CommandStreamReceiver::preemptionAllocation;
    using CommandStreamReceiver::tagAddress;
    using CommandStreamReceiver::tagsMultiAllocation;
    using CommandStreamReceiver::taskCount;
    using CommandStreamReceiver::useGpuIdleImplicitFlush;
    using CommandStreamReceiver::useNewResourceImplicitFlush;

    MockCommandStreamReceiver(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex, const DeviceBitfield deviceBitfield)
        : CommandStreamReceiver(executionEnvironment, rootDeviceIndex, deviceBitfield) {
        CommandStreamReceiver::tagAddress = &mockTagAddress[0];
        memset(const_cast<uint32_t *>(CommandStreamReceiver::tagAddress), 0xFFFFFFFF, tagSize * sizeof(uint32_t));
    }

    WaitStatus waitForCompletionWithTimeout(const WaitParams &params, uint32_t taskCountToWait) override {
        waitForCompletionWithTimeoutCalled++;
        return waitForCompletionWithTimeoutReturnValue;
    }
    SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;

    void flushTagUpdate() override{};
    void flushNonKernelTask(GraphicsAllocation *eventAlloc, uint64_t immediateGpuAddress, uint64_t immediateData, PipeControlArgs &args, bool isWaitOnEvents, bool startOfDispatch, bool endOfDispatch) override{};
    void updateTagFromWait() override{};
    bool isUpdateTagFromWaitEnabled() override { return false; };

    bool isMultiOsContextCapable() const override { return multiOsContextCapable; }

    bool isGpuHangDetected() const override {
        if (isGpuHangDetectedReturnValue.has_value()) {
            return *isGpuHangDetectedReturnValue;
        } else {
            return CommandStreamReceiver::isGpuHangDetected();
        }
    }

    bool testTaskCountReady(volatile uint32_t *pollAddress, uint32_t taskCountToWait) override {
        if (testTaskCountReadyReturnValue.has_value()) {
            return *testTaskCountReadyReturnValue;
        } else {
            return CommandStreamReceiver::testTaskCountReady(pollAddress, taskCountToWait);
        }
    }

    MemoryCompressionState getMemoryCompressionState(bool auxTranslationRequired, const HardwareInfo &hwInfo) const override {
        return MemoryCompressionState::NotApplicable;
    };

    TagAllocatorBase *getTimestampPacketAllocator() override { return nullptr; }

    CompletionStamp flushTask(
        LinearStream &commandStream,
        size_t commandStreamStart,
        const IndirectHeap *dsh,
        const IndirectHeap *ioh,
        const IndirectHeap *ssh,
        uint32_t taskLevel,
        DispatchFlags &dispatchFlags,
        Device &device) override;

    bool flushBatchedSubmissions() override {
        if (flushBatchedSubmissionsCallCounter) {
            (*flushBatchedSubmissionsCallCounter)++;
        }
        return true;
    }

    WaitStatus waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool quickKmdSleep, QueueThrottle throttle) override {
        return WaitStatus::Ready;
    }

    WaitStatus waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool quickKmdSleep, bool forcePowerSavingMode) {
        return WaitStatus::Ready;
    }

    uint32_t flushBcsTask(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, bool profilingEnabled, Device &device) override { return taskCount; };

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

    void programComputeBarrierCommand(LinearStream &cmdStream) override {
        programComputeBarrierCommandCalled = true;
    }
    size_t getCmdsSizeForComputeBarrierCommand() const override {
        return 0;
    }

    bool createPreemptionAllocation() override {
        if (createPreemptionAllocationParentCall) {
            return CommandStreamReceiver::createPreemptionAllocation();
        }
        return createPreemptionAllocationReturn;
    }

    GraphicsAllocation *getClearColorAllocation() override { return nullptr; }
    void makeResident(GraphicsAllocation &gfxAllocation) override {
        makeResidentCalledTimes++;
    }

    std::unique_lock<CommandStreamReceiver::MutexType> obtainHostPtrSurfaceCreationLock() override {
        ++hostPtrSurfaceCreationMutexLockCount;
        return CommandStreamReceiver::obtainHostPtrSurfaceCreationLock();
    }

    void postInitFlagsSetup() override {}

    static constexpr size_t tagSize = 256;
    static volatile uint32_t mockTagAddress[tagSize];
    std::vector<char> instructionHeapReserveredData;
    int *flushBatchedSubmissionsCallCounter = nullptr;
    uint32_t waitForCompletionWithTimeoutCalled = 0;
    uint32_t makeResidentCalledTimes = 0;
    int hostPtrSurfaceCreationMutexLockCount = 0;
    bool multiOsContextCapable = false;
    bool memoryCompressionEnabled = false;
    bool downloadAllocationsCalled = false;
    bool programHardwareContextCalled = false;
    bool createPreemptionAllocationReturn = true;
    bool createPreemptionAllocationParentCall = false;
    bool programComputeBarrierCommandCalled = false;
    std::optional<bool> isGpuHangDetectedReturnValue{};
    std::optional<bool> testTaskCountReadyReturnValue{};
    WaitStatus waitForCompletionWithTimeoutReturnValue{WaitStatus::Ready};
};

class MockCommandStreamReceiverWithFailingSubmitBatch : public MockCommandStreamReceiver {
  public:
    MockCommandStreamReceiverWithFailingSubmitBatch(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex, const DeviceBitfield deviceBitfield)
        : MockCommandStreamReceiver(executionEnvironment, rootDeviceIndex, deviceBitfield) {}
    SubmissionStatus submitBatchBuffer(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        return SubmissionStatus::FAILED;
    }
};

class MockCommandStreamReceiverWithOutOfMemorySubmitBatch : public MockCommandStreamReceiver {
  public:
    MockCommandStreamReceiverWithOutOfMemorySubmitBatch(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex, const DeviceBitfield deviceBitfield)
        : MockCommandStreamReceiver(executionEnvironment, rootDeviceIndex, deviceBitfield) {}
    SubmissionStatus submitBatchBuffer(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        return SubmissionStatus::OUT_OF_MEMORY;
    }
};

template <typename GfxFamily>
class MockCsrHw2 : public CommandStreamReceiverHw<GfxFamily> {
  public:
    using CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw;
    using CommandStreamReceiverHw<GfxFamily>::csrSizeRequestFlags;
    using CommandStreamReceiverHw<GfxFamily>::flushStamp;
    using CommandStreamReceiverHw<GfxFamily>::postInitFlagsSetup;
    using CommandStreamReceiverHw<GfxFamily>::programL3;
    using CommandStreamReceiverHw<GfxFamily>::programVFEState;
    using CommandStreamReceiver::activePartitions;
    using CommandStreamReceiver::activePartitionsConfig;
    using CommandStreamReceiver::clearColorAllocation;
    using CommandStreamReceiver::commandStream;
    using CommandStreamReceiver::dispatchMode;
    using CommandStreamReceiver::globalFenceAllocation;
    using CommandStreamReceiver::isPreambleSent;
    using CommandStreamReceiver::latestFlushedTaskCount;
    using CommandStreamReceiver::mediaVfeStateDirty;
    using CommandStreamReceiver::nTo1SubmissionModelEnabled;
    using CommandStreamReceiver::pageTableManagerInitialized;
    using CommandStreamReceiver::postSyncWriteOffset;
    using CommandStreamReceiver::requiredScratchSize;
    using CommandStreamReceiver::streamProperties;
    using CommandStreamReceiver::tagAddress;
    using CommandStreamReceiver::taskCount;
    using CommandStreamReceiver::taskLevel;
    using CommandStreamReceiver::timestampPacketWriteEnabled;
    using CommandStreamReceiver::useGpuIdleImplicitFlush;
    using CommandStreamReceiver::useNewResourceImplicitFlush;

    MockCsrHw2(ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex, const DeviceBitfield deviceBitfield)
        : CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw(executionEnvironment, rootDeviceIndex, deviceBitfield) {
    }

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

    SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        flushCalledCount++;
        if (recordedCommandBuffer) {
            recordedCommandBuffer->batchBuffer = batchBuffer;
        }
        copyOfAllocations = allocationsForResidency;
        flushStamp->setStamp(flushStamp->peekStamp() + 1);
        return SubmissionStatus::SUCCESS;
    }

    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap *dsh, const IndirectHeap *ioh,
                              const IndirectHeap *ssh, uint32_t taskLevel, DispatchFlags &dispatchFlags, Device &device) override {
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

    uint32_t flushBcsTask(const BlitPropertiesContainer &blitPropertiesContainer, bool blocking, bool profilingEnabled, Device &device) override {
        if (!skipBlitCalls) {
            return CommandStreamReceiverHw<GfxFamily>::flushBcsTask(blitPropertiesContainer, blocking, profilingEnabled, device);
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
