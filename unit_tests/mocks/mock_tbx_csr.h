/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/aub/aub_center.h"
#include "runtime/command_stream/preemption.h"
#include "runtime/command_stream/tbx_command_stream_receiver_hw.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/hw_info.h"

#include "gmock/gmock.h"

#include <string>

namespace OCLRT {

template <typename GfxFamily>
class MockTbxCsrToTestWaitBeforeMakingNonResident : public TbxCommandStreamReceiverHw<GfxFamily> {
  public:
    using CommandStreamReceiver::latestFlushedTaskCount;
    MockTbxCsrToTestWaitBeforeMakingNonResident(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment)
        : TbxCommandStreamReceiverHw<GfxFamily>(hwInfoIn, executionEnvironment) {}

    void makeCoherent(GraphicsAllocation &gfxAllocation) override {
        auto tagAddress = reinterpret_cast<uint32_t *>(gfxAllocation.getUnderlyingBuffer());
        *tagAddress = this->latestFlushedTaskCount;
        makeCoherentCalled = true;
    }
    bool makeCoherentCalled = false;
};

template <typename GfxFamily>
class MockTbxCsr : public TbxCommandStreamReceiverHw<GfxFamily> {
  public:
    using TbxCommandStreamReceiverHw<GfxFamily>::writeMemory;
    MockTbxCsr(const HardwareInfo &hwInfoIn, ExecutionEnvironment &executionEnvironment)
        : TbxCommandStreamReceiverHw<GfxFamily>(hwInfoIn, executionEnvironment) {}

    void initializeEngine() {
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
    void submitBatchBuffer(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits) override {
        TbxCommandStreamReceiverHw<GfxFamily>::submitBatchBuffer(batchBufferGpuAddress, batchBuffer, batchBufferSize, memoryBank, entryBits);
        submitBatchBufferCalled = true;
    }
    void pollForCompletion() override {
        TbxCommandStreamReceiverHw<GfxFamily>::pollForCompletion();
        pollForCompletionCalled = true;
    }
    void makeCoherent(GraphicsAllocation &gfxAllocation) override {
        TbxCommandStreamReceiverHw<GfxFamily>::makeCoherent(gfxAllocation);
        makeCoherentCalled = true;
    }
    bool initializeEngineCalled = false;
    bool writeMemoryWithAubManagerCalled = false;
    bool writeMemoryCalled = false;
    bool submitBatchBufferCalled = false;
    bool pollForCompletionCalled = false;
    bool expectMemoryEqualCalled = false;
    bool expectMemoryNotEqualCalled = false;
    bool makeCoherentCalled = false;
};

struct TbxExecutionEnvironment {
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    GraphicsAllocation *commandBuffer = nullptr;
    template <typename CsrType>
    CsrType *getCsr() {
        return static_cast<CsrType *>(executionEnvironment->commandStreamReceivers[0][0].get());
    }
    ~TbxExecutionEnvironment() {
        if (commandBuffer) {
            executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
        }
    }
};

template <typename CsrType>
std::unique_ptr<TbxExecutionEnvironment> getEnvironment(bool createTagAllocation, bool allocateCommandBuffer) {
    std::unique_ptr<ExecutionEnvironment> executionEnvironment(new ExecutionEnvironment);
    executionEnvironment->aubCenter.reset(new AubCenter());

    executionEnvironment->commandStreamReceivers.resize(1);
    executionEnvironment->commandStreamReceivers[0].push_back(std::make_unique<CsrType>(*platformDevices[0], *executionEnvironment));
    executionEnvironment->initializeMemoryManager(false, false);
    if (createTagAllocation) {
        executionEnvironment->commandStreamReceivers[0][0]->initializeTagAllocation();
    }

    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(executionEnvironment->commandStreamReceivers[0][0].get(),
                                                                                     getChosenEngineType(*platformDevices[0]), 1, PreemptionHelper::getDefaultPreemptionMode(*platformDevices[0]));
    executionEnvironment->commandStreamReceivers[0][0]->setupContext(*osContext);

    std::unique_ptr<TbxExecutionEnvironment> tbxExecutionEnvironment(new TbxExecutionEnvironment);
    if (allocateCommandBuffer) {
        tbxExecutionEnvironment->commandBuffer = executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    }
    tbxExecutionEnvironment->executionEnvironment = std::move(executionEnvironment);
    return tbxExecutionEnvironment;
}
} // namespace OCLRT
