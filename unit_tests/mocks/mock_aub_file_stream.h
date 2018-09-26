/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/command_stream/aub_command_stream_receiver.h"

namespace OCLRT {

struct MockAubFileStream : public AUBCommandStreamReceiver::AubFileStream {
    bool init(uint32_t stepping, uint32_t device) override {
        initCalledCnt++;
        return true;
    }
    void flush() override {
        flushCalled = true;
    }
    std::unique_lock<std::mutex> lockStream() override {
        lockStreamCalled = true;
        return AUBCommandStreamReceiver::AubFileStream::lockStream();
    }
    void expectMemory(uint64_t physAddress, const void *memory, size_t size, uint32_t addressSpace) override {
        physAddressCapturedFromExpectMemory = physAddress;
        memoryCapturedFromExpectMemory = reinterpret_cast<uintptr_t>(memory);
        sizeCapturedFromExpectMemory = size;
        addressSpaceCapturedFromExpectMemory = addressSpace;
    }
    uint32_t initCalledCnt = 0;
    bool flushCalled = false;
    bool lockStreamCalled = false;
    uint64_t physAddressCapturedFromExpectMemory = 0;
    uintptr_t memoryCapturedFromExpectMemory = 0;
    size_t sizeCapturedFromExpectMemory = 0;
    uint32_t addressSpaceCapturedFromExpectMemory = 0;
};
} // namespace OCLRT
