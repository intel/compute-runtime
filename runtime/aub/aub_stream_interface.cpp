/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_center.h"
using namespace AubDump;
namespace OCLRT {
AubManager *createAubManager(uint32_t gfxFamily, uint32_t devicesCount, uint64_t memoryBankSize, bool localMemorySupported, const std::string &aubFileName) {
    return AubDump::AubManager::create(gfxFamily, devicesCount, memoryBankSize, localMemorySupported, aubFileName);
}
} // namespace OCLRT
