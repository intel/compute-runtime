/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "core/helpers/string.h"
#include "core/memory_manager/graphics_allocation.h"
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/flat_batch_buffer_helper_hw.h"
#include "runtime/helpers/flush_stamp.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/os_context.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"

#include "gmock/gmock.h"

#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

using namespace NEO;

template <typename GfxFamily>
class MockCsrBase : public UltCommandStreamReceiver<GfxFamily> {
  public:
    using BaseUltCsrClass = UltCommandStreamReceiver<GfxFamily>;
    using BaseUltCsrClass::BaseUltCsrClass;

    MockCsrBase() = delete;

    MockCsrBase(int32_t &execStamp, ExecutionEnvironment &executionEnvironment)
        : BaseUltCsrClass(executionEnvironment), executionStamp(&execStamp), flushTaskStamp(-1) {
    }

    void makeResident(GraphicsAllocation &gfxAllocation) override {
        madeResidentGfxAllocations.push_back(&gfxAllocation);
        if (this->getMemoryManager()) {
            this->getResidencyAllocations().push_back(&gfxAllocation);
        }
        gfxAllocation.updateResidencyTaskCount(this->taskCount, this->osContext->getContextId());
    }
    void makeNonResident(GraphicsAllocation &gfxAllocation) override {
        madeNonResidentGfxAllocations.push_back(&gfxAllocation);
    }

    uint32_t peekThreadArbitrationPolicy() { return this->requiredThreadArbitrationPolicy; }

    bool isMadeResident(GraphicsAllocation *gfxAllocation) {
        for (GraphicsAllocation *gfxAlloc : madeResidentGfxAllocations) {
            if (gfxAlloc == gfxAllocation)
                return true;
        }
        return false;
    }

    bool isMadeNonResident(GraphicsAllocation *gfxAllocation) {
        for (GraphicsAllocation *gfxAlloc : madeNonResidentGfxAllocations) {
            if (gfxAlloc == gfxAllocation)
                return true;
        }
        return false;
    }

    bool getGSBAFor32BitProgrammed() {
        return this->GSBAFor32BitProgrammed;
    }

    void processEviction() override {
        processEvictionCalled = true;
    }

    void waitForTaskCountAndCleanAllocationList(uint32_t requiredTaskCount, uint32_t allocationUsage) override {
        waitForTaskCountRequiredTaskCount = requiredTaskCount;
        BaseUltCsrClass::waitForTaskCountAndCleanAllocationList(requiredTaskCount, allocationUsage);
    }

    ResidencyContainer madeResidentGfxAllocations;
    ResidencyContainer madeNonResidentGfxAllocations;
    int32_t *executionStamp;
    int32_t flushTaskStamp;
    bool processEvictionCalled = false;
    uint32_t waitForTaskCountRequiredTaskCount = 0;
};

template <typename GfxFamily>
using MockCsrHw = MockCsrBase<GfxFamily>;

template <typename GfxFamily>
class MockCsrAub : public MockCsrBase<GfxFamily> {
  public:
    MockCsrAub(int32_t &execStamp, ExecutionEnvironment &executionEnvironment) : MockCsrBase<GfxFamily>(execStamp, executionEnvironment) {}
    CommandStreamReceiverType getType() override {
        return CommandStreamReceiverType::CSR_AUB;
    }
};

template <typename GfxFamily>
class MockCsr : public MockCsrBase<GfxFamily> {
  public:
    using BaseClass = MockCsrBase<GfxFamily>;
    using CommandStreamReceiver::mediaVfeStateDirty;

    MockCsr() = delete;
    MockCsr(const HardwareInfo &hwInfoIn) = delete;
    MockCsr(int32_t &execStamp, ExecutionEnvironment &executionEnvironment) : BaseClass(execStamp, executionEnvironment) {
    }

    FlushStamp flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        return 0;
    }

    CompletionStamp flushTask(
        LinearStream &commandStream,
        size_t commandStreamStart,
        const IndirectHeap &dsh,
        const IndirectHeap &ioh,
        const IndirectHeap &ssh,
        uint32_t taskLevel,
        DispatchFlags &dispatchFlags,
        Device &device) override {
        this->flushTaskStamp = *this->executionStamp;
        (*this->executionStamp)++;
        slmUsedInLastFlushTask = dispatchFlags.useSLM;
        this->latestSentTaskCount = ++this->taskCount;
        lastTaskLevelToFlushTask = taskLevel;

        return CommandStreamReceiverHw<GfxFamily>::flushTask(
            commandStream,
            commandStreamStart,
            dsh,
            ioh,
            ssh,
            taskLevel,
            dispatchFlags,
            device);
    }

    bool peekMediaVfeStateDirty() const { return mediaVfeStateDirty; }

    bool slmUsedInLastFlushTask = false;
    uint32_t lastTaskLevelToFlushTask = 0;
};

template <typename GfxFamily>
class MockCsrHw2 : public CommandStreamReceiverHw<GfxFamily> {
  public:
    using CommandStreamReceiverHw<GfxFamily>::flushStamp;
    using CommandStreamReceiverHw<GfxFamily>::programL3;
    using CommandStreamReceiverHw<GfxFamily>::csrSizeRequestFlags;
    using CommandStreamReceiverHw<GfxFamily>::programVFEState;
    using CommandStreamReceiver::commandStream;
    using CommandStreamReceiver::dispatchMode;
    using CommandStreamReceiver::isPreambleSent;
    using CommandStreamReceiver::lastSentCoherencyRequest;
    using CommandStreamReceiver::mediaVfeStateDirty;
    using CommandStreamReceiver::nTo1SubmissionModelEnabled;
    using CommandStreamReceiver::requiredScratchSize;
    using CommandStreamReceiver::taskCount;
    using CommandStreamReceiver::taskLevel;
    using CommandStreamReceiver::timestampPacketWriteEnabled;

    MockCsrHw2(ExecutionEnvironment &executionEnvironment) : CommandStreamReceiverHw<GfxFamily>::CommandStreamReceiverHw(executionEnvironment) {}

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

    FlushStamp flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        flushCalledCount++;
        recordedCommandBuffer->batchBuffer = batchBuffer;
        copyOfAllocations = allocationsForResidency;
        flushStamp->setStamp(flushStamp->peekStamp() + 1);
        return flushStamp->peekStamp();
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

    void blitBuffer(const BlitProperties &blitProperites) override {
        if (!skipBlitCalls) {
            CommandStreamReceiverHw<GfxFamily>::blitBuffer(blitProperites);
        }
    }

    bool skipBlitCalls = false;
    bool storeFlushedTaskStream = false;
    std::unique_ptr<uint8_t> storedTaskStream;
    size_t storedTaskStreamSize = 0;

    int flushCalledCount = 0;
    std::unique_ptr<CommandBuffer> recordedCommandBuffer = nullptr;
    ResidencyContainer copyOfAllocations;
    DispatchFlags passedDispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
};

template <typename GfxFamily>
class MockFlatBatchBufferHelper : public FlatBatchBufferHelperHw<GfxFamily> {
  public:
    using FlatBatchBufferHelperHw<GfxFamily>::FlatBatchBufferHelperHw;
    MOCK_METHOD1(setPatchInfoData, bool(const PatchInfoData &));
    MOCK_METHOD1(removePatchInfoData, bool(uint64_t));
    MOCK_METHOD1(registerCommandChunk, bool(CommandChunk &));
    MOCK_METHOD2(registerBatchBufferStartAddress, bool(uint64_t, uint64_t));
    MOCK_METHOD3(flattenBatchBuffer,
                 GraphicsAllocation *(BatchBuffer &batchBuffer, size_t &sizeBatchBuffer, DispatchMode dispatchMode));
};

class MockCommandStreamReceiver : public CommandStreamReceiver {
  public:
    using CommandStreamReceiver::CommandStreamReceiver;
    using CommandStreamReceiver::deviceIndex;
    using CommandStreamReceiver::getDeviceIndexForAllocation;
    using CommandStreamReceiver::internalAllocationStorage;
    using CommandStreamReceiver::latestFlushedTaskCount;
    using CommandStreamReceiver::latestSentTaskCount;
    using CommandStreamReceiver::tagAddress;
    std::vector<char> instructionHeapReserveredData;
    int *flushBatchedSubmissionsCallCounter = nullptr;
    uint32_t waitForCompletionWithTimeoutCalled = 0;
    bool multiOsContextCapable = false;

    ~MockCommandStreamReceiver() {
    }

    bool waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMicroseconds, uint32_t taskCountToWait) override {
        waitForCompletionWithTimeoutCalled++;
        return true;
    }
    FlushStamp flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override;

    bool isMultiOsContextCapable() const { return multiOsContextCapable; }

    CompletionStamp flushTask(
        LinearStream &commandStream,
        size_t commandStreamStart,
        const IndirectHeap &dsh,
        const IndirectHeap &ioh,
        const IndirectHeap &ssh,
        uint32_t taskLevel,
        DispatchFlags &dispatchFlags,
        Device &device) override;

    void flushBatchedSubmissions() override {
        if (flushBatchedSubmissionsCallCounter) {
            (*flushBatchedSubmissionsCallCounter)++;
        }
    }

    void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool quickKmdSleep, bool forcePowerSavingMode) override {
    }

    void blitBuffer(const BlitProperties &blitProperites) override{};

    void setOSInterface(OSInterface *osInterface);

    CommandStreamReceiverType getType() override {
        return CommandStreamReceiverType::CSR_HW;
    }
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
