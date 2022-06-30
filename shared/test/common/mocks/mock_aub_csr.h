/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

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

struct MockAubFileStreamMockMmioWrite : public AubMemDump::AubFileStream {
    void writeMMIOImpl(uint32_t offset, uint32_t value) override {
        mmioList.push_back(std::make_pair(offset, value));
    }
    bool isOnMmioList(const MMIOPair &mmio) {
        bool mmioFound = false;
        for (auto &mmioPair : mmioList) {
            if (mmioPair.first == mmio.first && mmioPair.second == mmio.second) {
                mmioFound = true;
                break;
            }
        }
        return mmioFound;
    }

    std::vector<std::pair<uint32_t, uint32_t>> mmioList;
};

template <typename GfxFamily>
struct MockAubCsrToTestDumpContext : public AUBCommandStreamReceiverHw<GfxFamily> {
    using AUBCommandStreamReceiverHw<GfxFamily>::AUBCommandStreamReceiverHw;

    void addContextToken(uint32_t dumpHandle) override {
        handle = dumpHandle;
    }
    uint32_t handle = 0;
};

template <typename GfxFamily>
struct MockAubCsr : public AUBCommandStreamReceiverHw<GfxFamily> {
    using CommandStreamReceiverHw<GfxFamily>::defaultSshSize;
    using AUBCommandStreamReceiverHw<GfxFamily>::taskCount;
    using AUBCommandStreamReceiverHw<GfxFamily>::latestSentTaskCount;
    using AUBCommandStreamReceiverHw<GfxFamily>::pollForCompletionTaskCount;
    using AUBCommandStreamReceiverHw<GfxFamily>::getParametersForWriteMemory;
    using AUBCommandStreamReceiverHw<GfxFamily>::writeMemory;
    using AUBCommandStreamReceiverHw<GfxFamily>::AUBCommandStreamReceiverHw;

    CompletionStamp flushTask(LinearStream &commandStream, size_t commandStreamStart,
                              const IndirectHeap *dsh, const IndirectHeap *ioh, const IndirectHeap *ssh,
                              uint32_t taskLevel, DispatchFlags &dispatchFlags, Device &device) override {
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
    void writeMMIO(uint32_t offset, uint32_t value) override {
        AUBCommandStreamReceiverHw<GfxFamily>::writeMMIO(offset, value);
        writeMMIOCalled = true;
    }
    void submitBatchBufferAub(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits) override {
        AUBCommandStreamReceiverHw<GfxFamily>::submitBatchBufferAub(batchBufferGpuAddress, batchBuffer, batchBufferSize, memoryBank, entryBits);
        submitBatchBufferCalled = true;
    }

    void writeMemoryWithAubManager(GraphicsAllocation &graphicsAllocation) override {
        CommandStreamReceiverSimulatedHw<GfxFamily>::writeMemoryWithAubManager(graphicsAllocation);
        writeMemoryWithAubManagerCalled = true;
    }

    void pollForCompletion() override {
        AUBCommandStreamReceiverHw<GfxFamily>::pollForCompletion();
        pollForCompletionCalled = true;
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
    WaitStatus waitForCompletionWithTimeout(const WaitParams &params, uint32_t taskCountToWait) override {
        return NEO::WaitStatus::Ready;
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
    bool multiOsContextCapable = false;
    bool flushBatchedSubmissionsCalled = false;
    bool initProgrammingFlagsCalled = false;
    bool initializeEngineCalled = false;
    bool writeMemoryCalled = false;
    bool writeMemoryWithAubManagerCalled = false;
    bool writeMMIOCalled = false;
    bool submitBatchBufferCalled = false;
    bool pollForCompletionCalled = false;
    bool expectMemoryEqualCalled = false;
    bool expectMemoryNotEqualCalled = false;
    bool expectMemoryCompressedCalled = false;
    bool addAubCommentCalled = false;
    bool dumpAllocationCalled = false;

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

    ADDMETHOD_NOBASE(addPatchInfoComments, bool, true, ());

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
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->initGmm();
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex]->aubCenter.reset(new AubCenter());

    executionEnvironment->initializeMemoryManager();
    auto commandStreamReceiver = std::make_unique<CsrType>("", standalone, *executionEnvironment, rootDeviceIndex, deviceBitfield);

    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(commandStreamReceiver.get(),
                                                                                     EngineDescriptorHelper::getDefaultDescriptor({getChosenEngineType(*defaultHwInfo), EngineUsage::Regular},
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
