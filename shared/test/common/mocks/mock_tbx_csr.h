/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/aub/aub_center.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/tbx_command_stream_receiver_hw.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/hw_info.h"

#include "gmock/gmock.h"

#include <string>

namespace NEO {

template <typename GfxFamily>
class MockTbxCsr : public TbxCommandStreamReceiverHw<GfxFamily> {
  public:
    using TbxCommandStreamReceiverHw<GfxFamily>::writeMemory;
    using TbxCommandStreamReceiverHw<GfxFamily>::allocationsForDownload;
    MockTbxCsr(ExecutionEnvironment &executionEnvironment, const DeviceBitfield deviceBitfield)
        : TbxCommandStreamReceiverHw<GfxFamily>(executionEnvironment, 0, deviceBitfield) {}

    void initializeEngine() override {
        TbxCommandStreamReceiverHw<GfxFamily>::initializeEngine();
        initializeEngineCalled = true;
    }

    void writeMemoryWithAubManager(GraphicsAllocation &graphicsAllocation) override {
        CommandStreamReceiverSimulatedHw<GfxFamily>::writeMemoryWithAubManager(graphicsAllocation);
        writeMemoryWithAubManagerCalled = true;
    }

    void writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) override {
        TbxCommandStreamReceiverHw<GfxFamily>::writeMemory(gpuAddress, cpuAddress, size, memoryBank, entryBits);
        writeMemoryCalled = true;
    }
    void submitBatchBufferTbx(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits, bool overrideRingHead) override {
        TbxCommandStreamReceiverHw<GfxFamily>::submitBatchBufferTbx(batchBufferGpuAddress, batchBuffer, batchBufferSize, memoryBank, entryBits, overrideRingHead);
        overrideRingHeadPassed = overrideRingHead;
        submitBatchBufferCalled = true;
    }
    void pollForCompletion() override {
        TbxCommandStreamReceiverHw<GfxFamily>::pollForCompletion();
        pollForCompletionCalled = true;
    }
    void downloadAllocation(GraphicsAllocation &gfxAllocation) override {
        TbxCommandStreamReceiverHw<GfxFamily>::downloadAllocation(gfxAllocation);
        makeCoherentCalled = true;
    }
    void dumpAllocation(GraphicsAllocation &gfxAllocation) override {
        TbxCommandStreamReceiverHw<GfxFamily>::dumpAllocation(gfxAllocation);
        dumpAllocationCalled = true;
    }
    bool initializeEngineCalled = false;
    bool writeMemoryWithAubManagerCalled = false;
    bool writeMemoryCalled = false;
    bool submitBatchBufferCalled = false;
    bool overrideRingHeadPassed = false;
    bool pollForCompletionCalled = false;
    bool expectMemoryEqualCalled = false;
    bool expectMemoryNotEqualCalled = false;
    bool makeCoherentCalled = false;
    bool dumpAllocationCalled = false;
};

template <typename GfxFamily>
struct MockTbxCsrRegisterDownloadedAllocations : TbxCommandStreamReceiverHw<GfxFamily> {
    using CommandStreamReceiver::latestFlushedTaskCount;
    using CommandStreamReceiver::tagsMultiAllocation;
    using TbxCommandStreamReceiverHw<GfxFamily>::TbxCommandStreamReceiverHw;
    using TbxCommandStreamReceiverHw<GfxFamily>::flushSubmissionsAndDownloadAllocations;
    void downloadAllocation(GraphicsAllocation &gfxAllocation) override {
        *reinterpret_cast<uint32_t *>(CommandStreamReceiver::getTagAllocation()->getUnderlyingBuffer()) = this->latestFlushedTaskCount;
        downloadedAllocations.insert(&gfxAllocation);
    }
    bool flushBatchedSubmissions() override {
        flushBatchedSubmissionsCalled = true;
        return true;
    }
    void flushTagUpdate() override {
        flushTagCalled = true;
    }

    std::unique_lock<CommandStreamReceiver::MutexType> obtainUniqueOwnership() override {
        obtainUniqueOwnershipCalled++;
        return TbxCommandStreamReceiverHw<GfxFamily>::obtainUniqueOwnership();
    }
    std::set<GraphicsAllocation *> downloadedAllocations;
    bool flushBatchedSubmissionsCalled = false;
    bool flushTagCalled = false;
    size_t obtainUniqueOwnershipCalled = 0;
};
} // namespace NEO
