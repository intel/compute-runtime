/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/debug_helpers.h"

#include "aubstream/allocation_params.h"
#include "aubstream/aub_manager.h"
#include "aubstream/aubstream.h"
#include "aubstream/hardware_context.h"

#include <optional>
#include <unordered_map>

struct MockHardwareContext : public aub_stream::HardwareContext {
    using SurfaceInfo = aub_stream::SurfaceInfo;

    MockHardwareContext(uint32_t deviceIndex) : deviceIndex(deviceIndex) {}
    ~MockHardwareContext() override {}

    void initialize() override { initializeCalled = true; }
    void pollForCompletion() override { pollForCompletionCalled = true; }
    void writeAndSubmitBatchBuffer(uint64_t gfxAddress, const void *batchBuffer, size_t size, uint32_t memoryBank, size_t pageSize) override { writeAndSubmitCalled = true; }
    void submitBatchBuffer(uint64_t gfxAddress, bool overrideRingHead) override { submitCalled = true; }
    void writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize) override {
        UNRECOVERABLE_IF(true); // should not be used
    }
    void writeMemory2(aub_stream::AllocationParams allocationParams) override {
        writeMemory2Called = true;
        writeMemoryPageSizePassed = allocationParams.pageSize;
        memoryBanksPassed = allocationParams.memoryBanks;

        if (storeAllocationParams) {
            storedAllocationParams.push_back(allocationParams);
        }
    }
    void writeMMIO(uint32_t offset, uint32_t value) override { writeMMIOCalled = true; }
    void freeMemory(uint64_t gfxAddress, size_t size) override { freeMemoryCalled = true; }
    void expectMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t compareOperation) override { expectMemoryCalled = true; }
    void readMemory(uint64_t gfxAddress, void *memory, size_t size, uint32_t memoryBank, size_t pageSize) override {
        readMemoryCalled = true;
        latestGpuVaForMemoryRead = gfxAddress;
    }
    void dumpBufferBIN(uint64_t gfxAddress, size_t size) override { dumpBufferBINCalled = true; }
    void dumpSurface(const SurfaceInfo &surfaceInfo) override { dumpSurfaceCalled = true; }
    void pollForFenceCompletion() override {}

    std::vector<aub_stream::AllocationParams> storedAllocationParams;
    uint64_t latestGpuVaForMemoryRead = 0;
    bool storeAllocationParams = false;
    bool initializeCalled = false;
    bool pollForCompletionCalled = false;
    bool writeAndSubmitCalled = false;
    bool submitCalled = false;
    bool writeMemory2Called = false;
    bool freeMemoryCalled = false;
    bool expectMemoryCalled = false;
    bool readMemoryCalled = false;
    bool writeMMIOCalled = false;
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
    MockAubManager(const aub_stream::AubManagerOptions &inputOptions) : options(inputOptions) {
    }
    ~MockAubManager() override {}

    HardwareContext *createHardwareContext(uint32_t device, uint32_t engine) { return createHardwareContext(device, engine, 0); }
    HardwareContext *createHardwareContext(uint32_t device, uint32_t engine, uint32_t flags) override {
        contextFlags = flags;
        return new MockHardwareContext(device);
    }

    HardwareContext *createHardwareContext2(const aub_stream::CreateHardwareContext2Params &params, uint32_t device, uint32_t engine, uint32_t flags) override {
        contextFlags = flags;
        return new MockHardwareContext(device);
    }

    bool releaseHardwareContext(HardwareContext *context) override {
        delete context;
        return true;
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
        receivedComments.append(message);
        addCommentCalled = true;
    }

    void writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize) override {
        UNRECOVERABLE_IF(true); // should not be used
    }

    void writeMemory2(aub_stream::AllocationParams allocationParams) override {
        writeMemory2Called = true;
        hintToWriteMemory = allocationParams.hint;
        writeMemoryPageSizePassed = allocationParams.pageSize;

        if (storeAllocationParams) {
            storedAllocationParams.push_back(allocationParams);
        }
    }

    void writePageTableEntries(uint64_t gfxAddress, size_t size, uint32_t memoryBanks, int hint,
                               std::vector<PageInfo> &lastLevelPages, size_t pageSize) override {
        writePageTableEntriesCalled = true;
    }

    void writePhysicalMemoryPages(const void *memory, std::vector<PageInfo> &pages, size_t size, int hint) override {
        writePhysicalMemoryPagesCalled = true;
    }

    void freeMemory(uint64_t gfxAddress, size_t size) override {
        freeMemoryCalled = true;
        freedGfxAddress = gfxAddress;
    }

    bool reservePhysicalMemory(aub_stream::AllocationParams allocationParams, aub_stream::PhysicalAllocationInfo &physicalAllocInfo) override { return false; };
    bool mapGpuVa(uint64_t gfxAddress, size_t size, aub_stream::PhysicalAllocationInfo physicalAllocInfo) override { return false; };

    uint32_t readMMIO(uint32_t offset) override {
        if (mmioData)
            return (*mmioData)[offset];
        else
            return ::aub_stream::AubManager::readMMIO(offset);
    }

    std::vector<aub_stream::AllocationParams> storedAllocationParams;
    uint32_t openCalledCnt = 0;
    std::string fileName = "";
    bool closeCalled = false;
    bool isOpenCalled = false;
    bool getFileNameCalled = false;
    bool isPaused = false;
    bool addCommentCalled = false;
    std::string receivedComments = "";
    bool writeMemory2Called = false;
    bool writePageTableEntriesCalled = false;
    bool writePhysicalMemoryPagesCalled = false;
    bool freeMemoryCalled = false;
    uint64_t freedGfxAddress = 0;
    bool storeAllocationParams = false;
    uint32_t contextFlags = 0;
    int hintToWriteMemory = 0;
    size_t writeMemoryPageSizePassed = 0;
    std::optional<std::unordered_map<uint32_t, uint32_t>> mmioData;

    aub_stream::AubManagerOptions options{};

  protected:
    HardwareContext *hardwareContext = nullptr;
};
