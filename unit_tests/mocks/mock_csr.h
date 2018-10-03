/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/command_stream_receiver.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/flat_batch_buffer_helper_hw.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/flush_stamp.h"
#include "runtime/helpers/string.h"
#include "unit_tests/libult/ult_command_stream_receiver.h"
#include "gmock/gmock.h"
#include <vector>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

using namespace OCLRT;

template <typename GfxFamily>
class MockCsrBase : public UltCommandStreamReceiver<GfxFamily> {
  public:
    using BaseUltCsrClass = UltCommandStreamReceiver<GfxFamily>;

    MockCsrBase() = delete;

    MockCsrBase(int32_t &execStamp, ExecutionEnvironment &executionEnvironment)
        : BaseUltCsrClass(*platformDevices[0], executionEnvironment), executionStamp(&execStamp), flushTaskStamp(-1) {
    }

    MockCsrBase(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment) : BaseUltCsrClass(hwInfoIn, executionEnvironment) {
    }

    void makeResident(GraphicsAllocation &gfxAllocation) override {
        madeResidentGfxAllocations.push_back(&gfxAllocation);
        if (this->getMemoryManager()) {
            this->getResidencyAllocations().push_back(&gfxAllocation);
        }
        gfxAllocation.residencyTaskCount[this->deviceIndex] = this->taskCount;
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

    void processEviction(OsContext &osContext) override {
        processEvictionCalled = true;
    }

    void waitForTaskCountAndCleanAllocationList(uint32_t requiredTaskCount, uint32_t allocationType) override {
        waitForTaskCountRequiredTaskCount = requiredTaskCount;
        BaseUltCsrClass::waitForTaskCountAndCleanAllocationList(requiredTaskCount, allocationType);
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
class MockCsr : public MockCsrBase<GfxFamily> {
  public:
    using BaseClass = MockCsrBase<GfxFamily>;
    using CommandStreamReceiver::mediaVfeStateDirty;

    MockCsr() = delete;
    MockCsr(const HardwareInfo &hwInfoIn) = delete;
    MockCsr(int32_t &execStamp, ExecutionEnvironment &executionEnvironment) : BaseClass(execStamp, executionEnvironment) {
    }

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer &allocationsForResidency, OsContext &osContext) override {
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
    using CommandStreamReceiver::commandStream;
    using CommandStreamReceiver::dispatchMode;
    using CommandStreamReceiver::isPreambleSent;
    using CommandStreamReceiver::lastSentCoherencyRequest;
    using CommandStreamReceiver::mediaVfeStateDirty;
    using CommandStreamReceiver::taskCount;
    using CommandStreamReceiver::taskLevel;
    using CommandStreamReceiver::timestampPacketWriteEnabled;

    MockCsrHw2(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment) : CommandStreamReceiverHw<GfxFamily>(hwInfoIn, executionEnvironment) {}

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

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType,
                     ResidencyContainer &allocationsForResidency, OsContext &osContext) override {
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
        return CommandStreamReceiverHw<GfxFamily>::flushTask(commandStream, commandStreamStart,
                                                             dsh, ioh, ssh, taskLevel, dispatchFlags, device);
    }

    int flushCalledCount = 0;
    std::unique_ptr<CommandBuffer> recordedCommandBuffer = nullptr;
    ResidencyContainer copyOfAllocations;
    DispatchFlags passedDispatchFlags = {};
};

template <typename GfxFamily>
class MockFlatBatchBufferHelper : public FlatBatchBufferHelperHw<GfxFamily> {
  public:
    MockFlatBatchBufferHelper(MemoryManager *memoryManager) : FlatBatchBufferHelperHw<GfxFamily>(memoryManager) {}
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
    using CommandStreamReceiver::latestFlushedTaskCount;
    using CommandStreamReceiver::latestSentTaskCount;
    using CommandStreamReceiver::tagAddress;
    std::vector<char> instructionHeapReserveredData;
    int *flushBatchedSubmissionsCallCounter = nullptr;

    std::unique_ptr<ExecutionEnvironment> mockExecutionEnvironment;

    MockCommandStreamReceiver() : CommandStreamReceiver(*(new ExecutionEnvironment)) {
        mockExecutionEnvironment.reset(&this->executionEnvironment);
    }

    ~MockCommandStreamReceiver() {
    }

    FlushStamp flush(BatchBuffer &batchBuffer, EngineType engineType, ResidencyContainer &allocationsForResidency, OsContext &osContext) override;

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

    void waitForTaskCountWithKmdNotifyFallback(uint32_t taskCountToWait, FlushStamp flushStampToWait, bool quickKmdSleep, OsContext &osContext) override {
    }

    void addPipeControl(LinearStream &commandStream, bool dcFlush) override {
    }

    void setOSInterface(OSInterface *osInterface);

    CommandStreamReceiverType getType() override {
        return CommandStreamReceiverType::CSR_HW;
    }
};

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
