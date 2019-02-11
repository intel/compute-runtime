/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <string>

namespace aub_stream {

struct HardwareContext;

class AubManager {
  public:
    virtual ~AubManager() = default;
    virtual HardwareContext *createHardwareContext(uint32_t device, uint32_t engine, uint32_t flags) = 0;
    virtual void writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize) = 0;

    static AubManager *create(uint32_t productFamily, uint32_t devicesCount, uint64_t memoryBankSize, bool localMemorySupported, const std::string &aubFileName, uint32_t streamMode);
};

} // namespace aub_stream
