/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/aub/aub_center.h"
using namespace aub_stream;
namespace NEO {
AubManager *createAubManager(uint32_t gfxFamily, uint32_t devicesCount, uint64_t memoryBankSize, bool localMemorySupported, uint32_t streamMode, uint64_t gpuAddressSpace) {
    return AubManager::create(gfxFamily, devicesCount, memoryBankSize, localMemorySupported, streamMode, gpuAddressSpace);
}
} // namespace NEO
