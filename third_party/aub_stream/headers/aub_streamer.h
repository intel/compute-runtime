/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>

namespace AubDump {

class AubStreamer {
  public:
    void writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, size_t pageSize);
    void expectMemory(uint64_t gfxAddress, const void *memory, size_t size);
    void dumpBuffer(uint64_t gfxAddress, size_t size);
    void dumpImage(uint64_t gfxAddress, size_t size);

    void submit(uint64_t batchBufferGfxAddress, const void *batchBuffer, size_t size, uint32_t memoryBank);
    void pollForCompletion();

    class AubStreamerImpl;
    AubStreamerImpl *aubStreamerImpl;

    ~AubStreamer();
};

} // namespace AubDump
