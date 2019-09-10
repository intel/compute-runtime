/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/gfx_partition.h"

namespace NEO {

void GfxPartition::initAdditionalRange(uint64_t gpuAddressSpace, uint64_t &gfxBase, uint64_t &gfxTop) {
    UNRECOVERABLE_IF("Invalid GPU Address Range!");
}

} // namespace NEO
