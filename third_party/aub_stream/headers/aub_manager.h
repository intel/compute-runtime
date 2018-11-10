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

    static AubManager *create(uint32_t gfxFamily, uint32_t devicesCount, size_t memoryBankSizeInGB, bool localMemorySupported, std::string &aubFileName);
};

} // namespace AubDump
