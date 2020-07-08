/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/unit_test/mocks/mock_command_stream_receiver.h"

#include "opencl/test/unit_test/libult/ult_command_stream_receiver.h"

#include "gmock/gmock.h"

#include <vector>

using namespace NEO;

template <typename GfxFamily>
class MockCsrBase : public UltCommandStreamReceiver<GfxFamily> {
  public:
    using BaseUltCsrClass = UltCommandStreamReceiver<GfxFamily>;
    using BaseUltCsrClass::BaseUltCsrClass;

    MockCsrBase() = delete;

    MockCsrBase(int32_t &execStamp, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex)
        : BaseUltCsrClass(executionEnvironment, rootDeviceIndex), executionStamp(&execStamp), flushTaskStamp(-1) {
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

    ResidencyContainer madeResidentGfxAllocations;
    ResidencyContainer madeNonResidentGfxAllocations;
    int32_t *executionStamp;
    int32_t flushTaskStamp;
    bool processEvictionCalled = false;
};

template <typename GfxFamily>
using MockCsrHw = MockCsrBase<GfxFamily>;

template <typename GfxFamily>
class MockCsrAub : public MockCsrBase<GfxFamily> {
  public:
    MockCsrAub(int32_t &execStamp, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) : MockCsrBase<GfxFamily>(execStamp, executionEnvironment, rootDeviceIndex) {}
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
    MockCsr(int32_t &execStamp, ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) : BaseClass(execStamp, executionEnvironment, rootDeviceIndex) {
    }

    bool flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        return true;
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
class MockFlatBatchBufferHelper : public FlatBatchBufferHelperHw<GfxFamily> {
  public:
    using FlatBatchBufferHelperHw<GfxFamily>::FlatBatchBufferHelperHw;
    MOCK_METHOD(bool, setPatchInfoData, (const PatchInfoData &), (override));
    MOCK_METHOD(bool, removePatchInfoData, (uint64_t), (override));
    MOCK_METHOD(bool, registerCommandChunk, (CommandChunk &), (override));
    MOCK_METHOD(bool, registerBatchBufferStartAddress, (uint64_t, uint64_t), (override));
    MOCK_METHOD(GraphicsAllocation *,
                flattenBatchBuffer,
                (uint32_t rootDeviceIndex, BatchBuffer &batchBuffer, size_t &sizeBatchBuffer, DispatchMode dispatchMode, DeviceBitfield deviceBitfield),
                (override));
};
