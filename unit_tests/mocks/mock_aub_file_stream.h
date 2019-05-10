/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_stream/aub_command_stream_receiver.h"

#include "gmock/gmock.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

namespace NEO {

struct MockAubFileStream : public AUBCommandStreamReceiver::AubFileStream {
    bool init(uint32_t stepping, uint32_t device) override {
        initCalledCnt++;
        return true;
    }
    void open(const char *filePath) override {
        fileName.assign(filePath);
        openCalledCnt++;
    }
    void close() override {
        fileName.clear();
        closeCalled = true;
    }
    bool isOpen() const override {
        isOpenCalled = true;
        return !fileName.empty();
    }
    const std::string &getFileName() const override {
        getFileNameCalled = true;
        return fileName;
    }
    void flush() override {
        flushCalled = true;
    }
    std::unique_lock<std::mutex> lockStream() override {
        lockStreamCalled = true;
        return AUBCommandStreamReceiver::AubFileStream::lockStream();
    }
    void expectMMIO(uint32_t mmioRegister, uint32_t expectedValue) {
        mmioRegisterFromExpectMMIO = mmioRegister;
        expectedValueFromExpectMMIO = expectedValue;
    }
    void expectMemory(uint64_t physAddress, const void *memory, size_t size, uint32_t addressSpace, uint32_t compareOperation) override {
        physAddressCapturedFromExpectMemory = physAddress;
        memoryCapturedFromExpectMemory = reinterpret_cast<uintptr_t>(memory);
        sizeCapturedFromExpectMemory = size;
        addressSpaceCapturedFromExpectMemory = addressSpace;
        compareOperationFromExpectMemory = compareOperation;
    }
    bool addComment(const char *message) override {
        receivedComment.assign(message);
        addCommentCalled = true;
        return true;
    }
    void registerPoll(uint32_t registerOffset, uint32_t mask, uint32_t value, bool pollNotEqual, uint32_t timeoutAction) override {
        registerPollCalled = true;
        AUBCommandStreamReceiver::AubFileStream::registerPoll(registerOffset, mask, value, pollNotEqual, timeoutAction);
    }
    uint32_t openCalledCnt = 0;
    std::string fileName = "";
    bool closeCalled = false;
    uint32_t initCalledCnt = 0;
    mutable bool isOpenCalled = false;
    mutable bool getFileNameCalled = false;
    bool registerPollCalled = false;
    bool addCommentCalled = false;
    std::string receivedComment = "";
    bool flushCalled = false;
    bool lockStreamCalled = false;
    uint32_t mmioRegisterFromExpectMMIO = 0;
    uint32_t expectedValueFromExpectMMIO = 0;
    uint64_t physAddressCapturedFromExpectMemory = 0;
    uintptr_t memoryCapturedFromExpectMemory = 0;
    size_t sizeCapturedFromExpectMemory = 0;
    uint32_t addressSpaceCapturedFromExpectMemory = 0;
    uint32_t compareOperationFromExpectMemory = 0;
};

struct GmockAubFileStream : public AUBCommandStreamReceiver::AubFileStream {
    MOCK_METHOD1(addComment, bool(const char *message));
};
} // namespace NEO

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
