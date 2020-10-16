/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub/aub_center.h"
using namespace aub_stream;
namespace NEO {
AubManager *createAubManager(uint32_t gfxFamily, uint32_t devicesCount, uint64_t memoryBankSize, uint32_t stepping, bool localMemorySupported, uint32_t streamMode, uint64_t gpuAddressSpace) {
    return AubManager::create(gfxFamily, devicesCount, memoryBankSize, stepping, localMemorySupported, streamMode, gpuAddressSpace);
}
} // namespace NEO
