/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub/aub_center.h"
using namespace aub_stream;
namespace OCLRT {
AubManager *createAubManager(uint32_t gfxFamily, uint32_t devicesCount, uint64_t memoryBankSize, bool localMemorySupported, const std::string &aubFileName, uint32_t streamMode) {
    return AubManager::create(gfxFamily, devicesCount, memoryBankSize, localMemorySupported, aubFileName, streamMode);
}
} // namespace OCLRT
