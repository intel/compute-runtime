/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <cstdint>
#include <string>

namespace AubDump {

struct HardwareContext;

class AubManager {
  public:
    virtual ~AubManager() = default;
    virtual HardwareContext *createHardwareContext(uint32_t device, uint32_t engine) = 0;
    virtual void writeMemory(uint64_t gfxAddress, const void *memory, size_t size, uint32_t memoryBanks, int hint, size_t pageSize = 65536) = 0;

    static AubManager *create(uint32_t gfxFamily, uint32_t devicesCount, uint64_t memoryBankSize, bool localMemorySupported, const std::string &aubFileName);
};

} // namespace AubDump
