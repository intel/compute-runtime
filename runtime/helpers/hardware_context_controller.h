/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "third_party/aub_stream/headers/aub_manager.h"
#include "third_party/aub_stream/headers/hardware_context.h"

#include <memory>
#include <vector>

namespace NEO {
class OsContext;

class HardwareContextController {
  public:
    HardwareContextController() = delete;
    HardwareContextController(aub_stream::AubManager &aubManager, OsContext &osContext, uint32_t flags);

    void initialize();
    void pollForCompletion();
    void expectMemory(uint64_t gfxAddress, const void *srcAddress, size_t length, uint32_t compareOperation);
    void submit(uint64_t batchBufferGpuAddress, const void *batchBuffer, size_t batchBufferSize, uint32_t memoryBank, uint64_t entryBits);
    void writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize);

    void dumpBufferBIN(uint64_t gfxAddress, size_t size);
    void readMemory(uint64_t gfxAddress, void *memory, size_t size, uint32_t memoryBanks, size_t pageSize);

    std::vector<std::unique_ptr<aub_stream::HardwareContext>> hardwareContexts;
};
} // namespace NEO
