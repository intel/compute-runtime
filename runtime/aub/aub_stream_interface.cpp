/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_center.h"
using namespace AubDump;
namespace OCLRT {
AubManager *createAubManager(uint32_t gfxFamily, uint32_t devicesCount, size_t memoryBankSizeInGB, bool localMemorySupported, uint32_t deviceId, const std::string &aubFileName) {
    return AubDump::AubManager::create(gfxFamily, devicesCount, memoryBankSizeInGB, localMemorySupported, deviceId, aubFileName);
}
} // namespace OCLRT
