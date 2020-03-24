/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/command_stream/preemption.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"

#include "opencl/source/command_stream/aub_command_stream_receiver_hw.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_allocation_properties.h"

#include "gmock/gmock.h"

#include <string>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

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
    using AUBCommandStreamReceiverHw<GfxFamily>::writeMemory;
    using AUBCommandStreamReceiverHw<GfxFamily>::AUBCommandStreamReceiverHw;

    DispatchMode peekDispatchMode() const {
        return this->dispatchMode;
    }

    GraphicsAllocation *getTagAllocation() const {
        return this->tagAllocation;
    }

    void setLatestSentTaskCount(uint32_t latestSentTaskCount) {
        this->latestSentTaskCount = latestSentTaskCount;
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
    void submitBatchBuffer(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits) override {
        AUBCommandStreamReceiverHw<GfxFamily>::submitBatchBuffer(batchBufferGpuAddress, batchBuffer, batchBufferSize, memoryBank, entryBits);
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
    void expectMemoryEqual(void *gfxAddress, const void *srcAddress, size_t length) override {
        AUBCommandStreamReceiverHw<GfxFamily>::expectMemoryEqual(gfxAddress, srcAddress, length);
        expectMemoryEqualCalled = true;
    }
    void expectMemoryNotEqual(void *gfxAddress, const void *srcAddress, size_t length) override {
        AUBCommandStreamReceiverHw<GfxFamily>::expectMemoryNotEqual(gfxAddress, srcAddress, length);
        expectMemoryNotEqualCalled = true;
    }
    bool waitForCompletionWithTimeout(bool enableTimeout, int64_t timeoutMicroseconds, uint32_t taskCountToWait) override {
        return true;
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
    bool multiOsContextCapable = false;
    bool flushBatchedSubmissionsCalled = false;
    bool initProgrammingFlagsCalled = false;
    bool initializeEngineCalled = false;
    bool writeMemoryCalled = false;
    bool writeMemoryWithAubManagerCalled = false;
    bool submitBatchBufferCalled = false;
    bool pollForCompletionCalled = false;
    bool expectMemoryEqualCalled = false;
    bool expectMemoryNotEqualCalled = false;
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

    MOCK_METHOD0(addPatchInfoComments, bool(void));

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
    executionEnvironment->prepareRootDeviceEnvironments(1);
    executionEnvironment->rootDeviceEnvironments[0]->setHwInfo(defaultHwInfo.get());
    executionEnvironment->rootDeviceEnvironments[0]->aubCenter.reset(new AubCenter());

    executionEnvironment->initializeMemoryManager();
    auto commandStreamReceiver = std::make_unique<CsrType>("", standalone, *executionEnvironment, 0);
    if (createTagAllocation) {
        commandStreamReceiver->initializeTagAllocation();
    }

    auto osContext = executionEnvironment->memoryManager->createAndRegisterOsContext(commandStreamReceiver.get(),
                                                                                     getChosenEngineType(*defaultHwInfo), 1,
                                                                                     PreemptionHelper::getDefaultPreemptionMode(*defaultHwInfo),
                                                                                     false, false, false);
    commandStreamReceiver->setupContext(*osContext);

    std::unique_ptr<AubExecutionEnvironment> aubExecutionEnvironment(new AubExecutionEnvironment);
    if (allocateCommandBuffer) {
        aubExecutionEnvironment->commandBuffer = executionEnvironment->memoryManager->allocateGraphicsMemoryWithProperties(MockAllocationProperties{MemoryConstants::pageSize});
    }
    aubExecutionEnvironment->executionEnvironment = std::move(executionEnvironment);
    aubExecutionEnvironment->commandStreamReceiver = std::move(commandStreamReceiver);
    return aubExecutionEnvironment;
}
} // namespace NEO

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
