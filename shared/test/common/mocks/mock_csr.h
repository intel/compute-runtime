/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include <vector>

using namespace NEO;

template <typename GfxFamily>
class MockCsrBase : public UltCommandStreamReceiver<GfxFamily> {
  public:
    using BaseUltCsrClass = UltCommandStreamReceiver<GfxFamily>;
    using BaseUltCsrClass::BaseUltCsrClass;
    using BaseUltCsrClass::debugPauseStateLock;
    using BaseUltCsrClass::preemptionAllocation;

    MockCsrBase() = delete;

    MockCsrBase(int32_t &execStamp,
                ExecutionEnvironment &executionEnvironment,
                uint32_t rootDeviceIndex,
                const DeviceBitfield deviceBitfield)
        : BaseUltCsrClass(executionEnvironment, rootDeviceIndex, deviceBitfield), executionStamp(&execStamp), flushTaskStamp(-1) {
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

    int32_t peekThreadArbitrationPolicy() { return this->streamProperties.stateComputeMode.threadArbitrationPolicy.value; }

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
    MockCsrAub(int32_t &execStamp,
               ExecutionEnvironment &executionEnvironment,
               uint32_t rootDeviceIndex,
               const DeviceBitfield deviceBitfield)
        : MockCsrBase<GfxFamily>(execStamp, executionEnvironment, rootDeviceIndex, deviceBitfield) {}
    CommandStreamReceiverType getType() override {
        return CommandStreamReceiverType::CSR_AUB;
    }
};

template <typename GfxFamily>
class MockCsr : public MockCsrBase<GfxFamily> {
  public:
    using BaseClass = MockCsrBase<GfxFamily>;
    using CommandStreamReceiver::mediaVfeStateDirty;
    using MockCsrBase<GfxFamily>::lastAdditionalKernelExecInfo;

    MockCsr() = delete;
    MockCsr(const HardwareInfo &hwInfoIn) = delete;
    MockCsr(int32_t &execStamp,
            ExecutionEnvironment &executionEnvironment,
            uint32_t rootDeviceIndex,
            const DeviceBitfield deviceBitfield)
        : BaseClass(execStamp, executionEnvironment, rootDeviceIndex, deviceBitfield) {
    }

    SubmissionStatus flush(BatchBuffer &batchBuffer, ResidencyContainer &allocationsForResidency) override {
        return SubmissionStatus::SUCCESS;
    }

    CompletionStamp flushTask(
        LinearStream &commandStream,
        size_t commandStreamStart,
        const IndirectHeap *dsh,
        const IndirectHeap *ioh,
        const IndirectHeap *ssh,
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

    ADDMETHOD_NOBASE(removePatchInfoData, bool, true, (uint64_t targetLocation));
    ADDMETHOD_NOBASE(registerCommandChunk, bool, true, (CommandChunk & commandChunk));
    ADDMETHOD_NOBASE(registerBatchBufferStartAddress, bool, true, (uint64_t commandAddress, uint64_t startAddress));

    GraphicsAllocation *flattenBatchBuffer(uint32_t rootDeviceIndex,
                                           BatchBuffer &batchBuffer,
                                           size_t &sizeBatchBuffer,
                                           DispatchMode dispatchMode,
                                           DeviceBitfield deviceBitfield) override {
        flattenBatchBufferCalled++;
        flattenBatchBufferParamsPassed.push_back({rootDeviceIndex, batchBuffer, sizeBatchBuffer, dispatchMode, deviceBitfield});
        return flattenBatchBufferResult;
    }

    struct FlattenBatchBufferParams {
        uint32_t rootDeviceIndex = 0u;
        BatchBuffer batchBuffer = {};
        size_t sizeBatchBuffer = 0u;
        DispatchMode dispatchMode = DispatchMode::DeviceDefault;
        DeviceBitfield deviceBitfield = {};
    };

    StackVec<FlattenBatchBufferParams, 1> flattenBatchBufferParamsPassed{};
    uint32_t flattenBatchBufferCalled = 0u;
    GraphicsAllocation *flattenBatchBufferResult = nullptr;

    bool setPatchInfoData(const PatchInfoData &data) override {
        setPatchInfoDataCalled++;
        patchInfoDataVector.push_back(data);
        return setPatchInfoDataResult;
    }

    std::vector<PatchInfoData> patchInfoDataVector{};
    uint32_t setPatchInfoDataCalled = 0u;
    bool setPatchInfoDataResult = true;
};
