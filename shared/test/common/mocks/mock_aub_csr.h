/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/aub/aub_center.h"
#include "shared/source/command_stream/aub_command_stream_receiver_hw.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/wait_status.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/test_macros/mock_method_macros.h"

#include <string>

namespace NEO {
template <typename GfxFamily>
struct MockAubCsr : public AUBCommandStreamReceiverHw<GfxFamily> {
    using CommandStreamReceiverHw<GfxFamily>::defaultSshSize;
    using AUBCommandStreamReceiverHw<GfxFamily>::taskCount;
    using AUBCommandStreamReceiverHw<GfxFamily>::latestSentTaskCount;
    using AUBCommandStreamReceiverHw<GfxFamily>::pollForCompletionTaskCount;
    using AUBCommandStreamReceiverHw<GfxFamily>::getParametersForMemory;
    using AUBCommandStreamReceiverHw<GfxFamily>::writeMemory;
    using AUBCommandStreamReceiverHw<GfxFamily>::pollForCompletion;
    using AUBCommandStreamReceiverHw<GfxFamily>::AUBCommandStreamReceiverHw;

    MockAubCsr(ExecutionEnvironment &executionEnvironment,
               uint32_t rootDeviceIndex,
               const DeviceBitfield deviceBitfield)
        : AUBCommandStreamReceiverHw<GfxFamily>("", true, executionEnvironment, rootDeviceIndex, deviceBitfield) {}

    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh,
                              TaskCountType taskLevel, DispatchFlags &dispatchFlags, Device &device) override {
        recordedDispatchFlags = dispatchFlags;

        return AUBCommandStreamReceiverHw<GfxFamily>::flushTask(commandStream, commandStreamStart, dsh, ioh, ssh, taskLevel, dispatchFlags, device);
    }

    DispatchMode peekDispatchMode() const {
        return this->dispatchMode;
    }

    GraphicsAllocation *getTagAllocation() const {
        return this->tagAllocation;
    }

    bool flushBatchedSubmissions() override {
        flushBatchedSubmissionsCalled = true;
        return true;
    }
    void initProgrammingFlags() override {
        initProgrammingFlagsCalled = true;
    }
    void initializeEngine() override {
        AUBCommandStreamReceiverHw<GfxFamily>::initializeEngine();
        initializeEngineCalled = true;
    }
    void writeMemory(uint64_t gpuAddress, void *cpuAddress, size_t size, uint32_t memoryBank, uint64_t entryBits) override {
        AUBCommandStreamReceiverHw<GfxFamily>::writeMemory(gpuAddress, cpuAddress, size, memoryBank, entryBits);
        writeMemoryCalled = true;
    }
    bool writeMemory(GraphicsAllocation &gfxAllocation) override {
        writeMemoryGfxAllocCalled = true;
        return AUBCommandStreamReceiverHw<GfxFamily>::writeMemory(gfxAllocation);
    }
    void writeMMIO(uint32_t offset, uint32_t value) override {
        AUBCommandStreamReceiverHw<GfxFamily>::writeMMIO(offset, value);
        writeMMIOCalled = true;
    }
    void submitBatchBufferAub(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits) override {
        AUBCommandStreamReceiverHw<GfxFamily>::submitBatchBufferAub(batchBufferGpuAddress, batchBuffer, batchBufferSize, memoryBank, entryBits);
        submitBatchBufferCalled = true;
        batchBufferGpuAddressPassed = batchBufferGpuAddress;
    }

    void writeMemoryWithAubManager(GraphicsAllocation &graphicsAllocation, bool isChunkCopy, uint64_t gpuVaChunkOffset, size_t chunkSize) override {
        CommandStreamReceiverSimulatedHw<GfxFamily>::writeMemoryWithAubManager(graphicsAllocation, isChunkCopy, gpuVaChunkOffset, chunkSize);
        writeMemoryWithAubManagerCalled = true;
    }

    void pollForCompletion(bool skipTaskCountCheck) override {
        AUBCommandStreamReceiverHw<GfxFamily>::pollForCompletion(skipTaskCountCheck);
        pollForCompletionCalled = true;
        skipTaskCountCheckForCompletionPoll = skipTaskCountCheck;
    }

    bool expectMemoryEqual(void *gfxAddress, const void *srcAddress, size_t length) override {
        expectMemoryEqualCalled = true;
        return AUBCommandStreamReceiverHw<GfxFamily>::expectMemoryEqual(gfxAddress, srcAddress, length);
    }
    bool expectMemoryNotEqual(void *gfxAddress, const void *srcAddress, size_t length) override {
        expectMemoryNotEqualCalled = true;
        return AUBCommandStreamReceiverHw<GfxFamily>::expectMemoryNotEqual(gfxAddress, srcAddress, length);
    }
    bool expectMemoryCompressed(void *gfxAddress, const void *srcAddress, size_t length) override {
        expectMemoryCompressedCalled = true;
        return AUBCommandStreamReceiverHw<GfxFamily>::expectMemoryCompressed(gfxAddress, srcAddress, length);
    }
    WaitStatus waitForCompletionWithTimeout(const WaitParams &params, TaskCountType taskCountToWait) override {
        return NEO::WaitStatus::ready;
    }
    void addAubComment(const char *message) override {
        AUBCommandStreamReceiverHw<GfxFamily>::addAubComment(message);
        addAubCommentCalled = true;
    }
    void dumpAllocation(GraphicsAllocation &gfxAllocation) override {
        AUBCommandStreamReceiverHw<GfxFamily>::dumpAllocation(gfxAllocation);
        dumpAllocationCalled = true;
    }
    bool isMultiOsContextCapable() const override {
        return multiOsContextCapable;
    }

    DispatchFlags recordedDispatchFlags = DispatchFlagsHelper::createDefaultDispatchFlags();
    uint64_t batchBufferGpuAddressPassed = 0u;
    bool multiOsContextCapable = false;
    bool flushBatchedSubmissionsCalled = false;
    bool initProgrammingFlagsCalled = false;
    bool initializeEngineCalled = false;
    bool writeMemoryCalled = false;
    bool writeMemoryGfxAllocCalled = false;
    bool writeMemoryWithAubManagerCalled = false;
    bool writeMMIOCalled = false;
    bool submitBatchBufferCalled = false;
    bool pollForCompletionCalled = false;
    bool expectMemoryEqualCalled = false;
    bool expectMemoryNotEqualCalled = false;
    bool expectMemoryCompressedCalled = false;
    bool addAubCommentCalled = false;
    bool dumpAllocationCalled = false;
    bool skipTaskCountCheckForCompletionPoll = false;
    bool lockStreamCalled = false;

    std::unique_lock<std::mutex> lockStream() override {
        lockStreamCalled = true;
        return AUBCommandStreamReceiverHw<GfxFamily>::lockStream();
    }
    void initFile(const std::string &fileName) override {
        fileIsOpen = true;
        openFileName = fileName;
    }
    void closeFile() override {
        fileIsOpen = false;
        openFileName = "";
    }
    bool isFileOpen() const override {
        return fileIsOpen;
    }
    const std::string getFileName() override {
        return openFileName;
    }
    bool fileIsOpen = false;
    std::string openFileName = "";

    using CommandStreamReceiverHw<GfxFamily>::localMemoryEnabled;
};

struct AubExecutionEnvironment {
    std::unique_ptr<ExecutionEnvironment> executionEnvironment;
    GraphicsAllocation *commandBuffer = nullptr;
    std::unique_ptr<CommandStreamReceiver> commandStreamReceiver;
    template <typename CsrType>
    CsrType *getCsr() {
        return static_cast<CsrType *>(commandStreamReceiver.get());
    }
    ~AubExecutionEnvironment() {
        if (commandBuffer) {
            executionEnvironment->memoryManager->freeGraphicsMemory(commandBuffer);
        }
    }
};

template <typename CsrType>
std::unique_ptr<AubExecutionEnvironment> getEnvironment(bool createTagAllocation, bool allocateCommandBuffer, bool standalone) {
    std::unique_ptr<ExecutionEnvironment> executionEnvironment(new ExecutionEnvironment);
    DeviceBitfield deviceBitfield(1);
    executionEnvironment->prepareRootDeviceEnvironments(1);
    uint32_t rootDeviceIndex = 0u;
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfoAndInitHelpers(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->initGmm();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->aubCenter.reset(new AubCenter());
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setDummyBlitProperties(rootDeviceIndex);

    executionEnvironment->initializeMemoryManager();
    auto commandStreamReceiver = std::make_unique<CsrType>("", standalone, *executionEnvironment, rootDeviceIndex, deviceBitfield);

    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(commandStreamReceiver.get(),
                                                                                     EngineDescriptorHelper::getDefaultDescriptor({getChosenEngineType(*defaultHwInfo), EngineUsage::regular},
                                                                                                                                  PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo)));
    commandStreamReceiver->setupContext(*osContext);

    if (createTagAllocation) {
        commandStreamReceiver->initializeTagAllocation();
    }
    commandStreamReceiver->createGlobalFenceAllocation();

    std::unique_ptr<AubExecutionEnvironment> aubExecutionEnvironment(new AubExecutionEnvironment);
    if (allocateCommandBuffer) {
        aubExecutionEnvironment->commandBuffer = executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{rootDeviceIndex, MemoryConstants::pageSize});
    }
    aubExecutionEnvironment->executionEnvironment = std::move(executionEnvironment);
    aubExecutionEnvironment->commandStreamReceiver = std::move(commandStreamReceiver);
    return aubExecutionEnvironment;
}
} // namespace NEO
