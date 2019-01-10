/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <cstddef>

namespace AubDump {

struct HardwareContext {
    virtual void initialize() = 0;
    virtual void pollForCompletion() = 0;
    virtual void submit(uint64_t gfxAddress, const void *batchBuffer, size_t size, uint32_t memoryBank, size_t pageSize = 65536) = 0;
    virtual void writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize = 65536) = 0;
    virtual void freeMemory(uint64_t gfxAddress, size_t size) = 0;
    virtual void expectMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t compareOperation) = 0;
    virtual void readMemory(uint64_t gfxAddress, void *memory, size_t size) = 0;
    virtual ~HardwareContext() = default;
};

} // namespace AubDump
