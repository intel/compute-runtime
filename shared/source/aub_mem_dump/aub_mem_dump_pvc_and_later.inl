/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/aub_alloc_dump.inl"
#include "shared/source/aub_mem_dump/aub_mem_dump.h"

namespace AubAllocDump {
using namespace NEO;

template SurfaceInfo *getDumpSurfaceInfo<Family>(GraphicsAllocation &gfxAllocation, const GmmHelper &gmmHelper, DumpFormat dumpFormat);

template uint32_t getImageSurfaceTypeFromGmmResourceType<Family>(GMM_RESOURCE_TYPE gmmResourceType);
} // namespace AubAllocDump
