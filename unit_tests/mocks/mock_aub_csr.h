/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_stream/aub_command_stream_receiver_hw.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/hw_info.h"
#include "gmock/gmock.h"
#include <string>

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace OCLRT {

template <typename GfxFamily>
struct MockAubCsr : public AUBCommandStreamReceiverHw<GfxFamily> {
    MockAubCsr(const HardwareInfo &hwInfoIn, const std::string &fileName, bool standalone, ExecutionEnvironment &executionEnvironment)
        : AUBCommandStreamReceiverHw<GfxFamily>(hwInfoIn, fileName, standalone, executionEnvironment){};

    DispatchMode peekDispatchMode() const {
        return this->dispatchMode;
    }

    GraphicsAllocation *getTagAllocation() const {
        return this->tagAllocation;
    }

    void setLatestSentTaskCount(uint32_t latestSentTaskCount) {
        this->latestSentTaskCount = latestSentTaskCount;
    }

    void flushBatchedSubmissions() override {
        flushBatchedSubmissionsCalled = true;
    }
    void initProgrammingFlags() override {
        initProgrammingFlagsCalled = true;
    }
    void pollForCompletion(EngineType engineType) override {
        pollForCompletionCalled = true;
    }
    bool flushBatchedSubmissionsCalled = false;
    bool initProgrammingFlagsCalled = false;
    bool pollForCompletionCalled = false;

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
    const std::string &getFileName() override {
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
    template <typename CsrType>
    CsrType *getCsr() {
        return static_cast<CsrType *>(executionEnvironment->commandStreamReceivers[0u].get());
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
    executionEnvironment->commandStreamReceivers.push_back(std::make_unique<CsrType>(*platformDevices[0], "", standalone, *executionEnvironment));
    executionEnvironment->memoryManager.reset(executionEnvironment->commandStreamReceivers[0u]->createMemoryManager(false, false));
    if (createTagAllocation) {
        executionEnvironment->commandStreamReceivers[0u]->initializeTagAllocation();
    }

    std::unique_ptr<AubExecutionEnvironment> aubExecutionEnvironment(new AubExecutionEnvironment);
    if (allocateCommandBuffer) {
        aubExecutionEnvironment->commandBuffer = executionEnvironment->memoryManager->allocateGraphicsMemory(4096);
    }
    aubExecutionEnvironment->executionEnvironment = std::move(executionEnvironment);
    return aubExecutionEnvironment;
}
} // namespace OCLRT

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
