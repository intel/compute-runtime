/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace AubDump {

struct HardwareContext {
    virtual void initialize() = 0;
    virtual void pollForCompletion() = 0;
    virtual void submit(uint64_t gfxAddress, const void *batchBuffer, size_t size, uint32_t memoryBank) = 0;
    virtual void writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize = 4096) = 0;
    virtual void freeMemory(uint64_t gfxAddress, size_t size) = 0;
};

} // namespace AubDump
