/*
 * Copyright (C) 2019-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/memory_manager/gfx_partition.h"

namespace NEO {

bool GfxPartition::initAdditionalRange(uint32_t cpuVirtualAddressSize, uint64_t gpuAddressSpace, uint64_t &gfxBase, uint64_t &gfxTop, uint32_t rootDeviceIndex, size_t numRootDevices) {
    return false;
}

} // namespace NEO
