/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstddef>
#include <cstdint>

namespace aub_stream {

struct AllocationParams;
struct SurfaceInfo;

struct HardwareContext {
    virtual void initialize() = 0;
    virtual void pollForCompletion() = 0;
    virtual void writeAndSubmitBatchBuffer(uint64_t gfxAddress, const void *batchBuffer, size_t size, uint32_t memoryBanks, size_t pageSize) = 0;
    virtual void submitBatchBuffer(uint64_t gfxAddress, bool overrideRingHead) = 0;
    virtual void writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize) = 0;
    virtual void freeMemory(uint64_t gfxAddress, size_t size) = 0;
    virtual void expectMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t compareOperation) = 0;
    virtual void readMemory(uint64_t gfxAddress, void *memory, size_t size, uint32_t memoryBanks, size_t pageSize) = 0;
    virtual void dumpBufferBIN(uint64_t gfxAddress, size_t size) = 0;
    virtual void dumpSurface(const SurfaceInfo &surfaceInfo) = 0;
    virtual ~HardwareContext() = default;
    virtual void writeMemory2(AllocationParams allocationParams) = 0;
    virtual void writeMMIO(uint32_t offset, uint32_t value) = 0;
    virtual void pollForFenceCompletion() = 0;
    virtual void release(){};
    virtual uint32_t getCurrentFence() { return 0; };
    virtual uint32_t getExpectedFence() { return 0; };
};

} // namespace aub_stream
