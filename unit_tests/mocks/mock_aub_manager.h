/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "third_party/aub_stream/headers/aub_manager.h"
#include "third_party/aub_stream/headers/hardware_context.h"

struct MockHardwareContext : public aub_stream::HardwareContext {
    MockHardwareContext(uint32_t deviceIndex, uint32_t engineIndex)
        : deviceIndex(deviceIndex), engineIndex(engineIndex){};
    ~MockHardwareContext() override{};

    void initialize() override { initializeCalled = true; }
    void pollForCompletion() override { pollForCompletionCalled = true; };
    void submit(uint64_t gfxAddress, const void *batchBuffer, size_t size, uint32_t memoryBank, size_t pageSize = 65536) override { submitCalled = true; }
    void writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize = 65536) override { writeMemoryCalled = true; }
    void freeMemory(uint64_t gfxAddress, size_t size) override { freeMemoryCalled = true; }
    void expectMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t compareOperation) override { expectMemoryCalled = true; }
    void readMemory(uint64_t gfxAddress, void *memory, size_t size, uint32_t memoryBank, size_t pageSize) override { readMemoryCalled = true; };
    void dumpBufferBIN(uint64_t gfxAddress, size_t size) override { dumpBufferBINCalled = true; };

    bool initializeCalled = false;
    bool pollForCompletionCalled = false;
    bool submitCalled = false;
    bool writeMemoryCalled = false;
    bool freeMemoryCalled = false;
    bool expectMemoryCalled = false;
    bool readMemoryCalled = false;
    bool dumpBufferBINCalled = false;

    uint32_t deviceIndex = 0;
    uint32_t engineIndex = 0;
};

class MockAubManager : public aub_stream::AubManager {
    using HardwareContext = aub_stream::HardwareContext;

  public:
    MockAubManager(){};
    ~MockAubManager() override {}

    HardwareContext *createHardwareContext(uint32_t device, uint32_t engine) override { return new MockHardwareContext(device, engine); }
    void writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize = 65536) override { writeMemoryCalled = true; }

    bool writeMemoryCalled = false;

  protected:
    HardwareContext *hardwareContext = nullptr;
};
