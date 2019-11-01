/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "third_party/aub_stream/headers/aub_manager.h"
#include "third_party/aub_stream/headers/aubstream.h"
#include "third_party/aub_stream/headers/hardware_context.h"

struct MockHardwareContext : public aub_stream::HardwareContext {
    using SurfaceInfo = aub_stream::SurfaceInfo;

    MockHardwareContext(uint32_t deviceIndex) : deviceIndex(deviceIndex) {}
    ~MockHardwareContext() override {}

    void initialize() override { initializeCalled = true; }
    void pollForCompletion() override { pollForCompletionCalled = true; }
    void writeAndSubmitBatchBuffer(uint64_t gfxAddress, const void *batchBuffer, size_t size, uint32_t memoryBank, size_t pageSize = 65536) override { writeAndSubmitCalled = true; }
    void submitBatchBuffer(uint64_t gfxAddress, bool overrideRingHead) override { submitCalled = true; }
    void writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize = 65536) override {
        writeMemoryCalled = true;
        writeMemoryPageSizePassed = pageSize;
        memoryBanksPassed = memoryBanks;
    }
    void freeMemory(uint64_t gfxAddress, size_t size) override { freeMemoryCalled = true; }
    void expectMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t compareOperation) override { expectMemoryCalled = true; }
    void readMemory(uint64_t gfxAddress, void *memory, size_t size, uint32_t memoryBank, size_t pageSize) override { readMemoryCalled = true; }
    void dumpBufferBIN(uint64_t gfxAddress, size_t size) override { dumpBufferBINCalled = true; }
    void dumpSurface(const SurfaceInfo &surfaceInfo) override { dumpSurfaceCalled = true; }

    bool initializeCalled = false;
    bool pollForCompletionCalled = false;
    bool writeAndSubmitCalled = false;
    bool submitCalled = false;
    bool writeMemoryCalled = false;
    bool freeMemoryCalled = false;
    bool expectMemoryCalled = false;
    bool readMemoryCalled = false;
    bool dumpBufferBINCalled = false;
    bool dumpSurfaceCalled = false;

    size_t writeMemoryPageSizePassed = 0;
    uint32_t memoryBanksPassed = 0;

    const uint32_t deviceIndex;
};

class MockAubManager : public aub_stream::AubManager {
    using HardwareContext = aub_stream::HardwareContext;
    using PageInfo = aub_stream::PageInfo;

  public:
    MockAubManager(){};
    MockAubManager(uint32_t productFamily, uint32_t devicesCount, uint64_t memoryBankSize, bool localMemorySupported, uint32_t streamMode) {
        mockAubManagerParams.productFamily = productFamily;
        mockAubManagerParams.devicesCount = devicesCount;
        mockAubManagerParams.memoryBankSize = memoryBankSize;
        mockAubManagerParams.localMemorySupported = localMemorySupported;
        mockAubManagerParams.streamMode = streamMode;
    }
    ~MockAubManager() override {}

    HardwareContext *createHardwareContext(uint32_t device, uint32_t engine) { return createHardwareContext(device, engine, 0); }
    HardwareContext *createHardwareContext(uint32_t device, uint32_t engine, uint32_t flags) override {
        contextFlags = flags;
        return new MockHardwareContext(device);
    }

    void open(const std::string &aubFileName) override {
        fileName.assign(aubFileName);
        openCalledCnt++;
    }
    void close() override {
        fileName.clear();
        closeCalled = true;
    }
    bool isOpen() override {
        isOpenCalled = true;
        return !fileName.empty();
    }
    const std::string getFileName() override {
        getFileNameCalled = true;
        return fileName;
    }
    void pause(bool onoff) override {
        isPaused = onoff;
    }

    void addComment(const char *message) override {
        receivedComment.assign(message);
        addCommentCalled = true;
    }

    void writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize = 65536) override {
        writeMemoryCalled = true;
        hintToWriteMemory = hint;
        writeMemoryPageSizePassed = pageSize;
    }

    void writePageTableEntries(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint,
                               std::vector<PageInfo> &lastLevelPages, size_t pageSize) override {
        writePageTableEntriesCalled = true;
    }

    void writePhysicalMemoryPages(const void *memory, std::vector<PageInfo> &pages, size_t size, int hint) override {
        writePhysicalMemoryPagesCalled = true;
    }

    void freeMemory(uint64_t gfxAddress, size_t size) override {
        freeMemoryCalled = true;
    }

    uint32_t openCalledCnt = 0;
    std::string fileName = "";
    bool closeCalled = false;
    bool isOpenCalled = false;
    bool getFileNameCalled = false;
    bool isPaused = false;
    bool addCommentCalled = false;
    std::string receivedComment = "";
    bool writeMemoryCalled = false;
    bool writePageTableEntriesCalled = false;
    bool writePhysicalMemoryPagesCalled = false;
    bool freeMemoryCalled = false;
    uint32_t contextFlags = 0;
    int hintToWriteMemory = 0;
    size_t writeMemoryPageSizePassed = 0;

    struct MockAubManagerParams {
        uint32_t productFamily = 0;
        int32_t devicesCount = 0;
        uint64_t memoryBankSize = 0;
        bool localMemorySupported = false;
        uint32_t streamMode = 0xFFFFFFFF;
    } mockAubManagerParams;

  protected:
    HardwareContext *hardwareContext = nullptr;
};
