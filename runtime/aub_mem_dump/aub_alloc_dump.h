/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "runtime/aub_mem_dump/aub_mem_dump.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/gmm_helper/resource_info.h"
#include "runtime/helpers/options.h"
#include "runtime/memory_manager/graphics_allocation.h"
#include "runtime/os_interface/debug_settings_manager.h"

using namespace OCLRT;

namespace AubAllocDump {

inline bool isWritableBuffer(GraphicsAllocation &gfxAllocation) {
    return (gfxAllocation.getAllocationType() == GraphicsAllocation::AllocationType::BUFFER || gfxAllocation.getAllocationType() == GraphicsAllocation::AllocationType::BUFFER_HOST_MEMORY) &&
           gfxAllocation.isMemObjectsAllocationWithWritableFlags();
}

inline bool isWritableImage(GraphicsAllocation &gfxAllocation) {
    return (gfxAllocation.getAllocationType() == GraphicsAllocation::AllocationType::IMAGE) &&
           gfxAllocation.isMemObjectsAllocationWithWritableFlags();
}

template <typename GfxFamily>
uint32_t getImageSurfaceTypeFromGmmResourceType(GMM_RESOURCE_TYPE gmmResourceType);

template <typename GfxFamily>
void dumpBufferInBinFormat(GraphicsAllocation &gfxAllocation, AubMemDump::AubFileStream *stream, uint32_t context);

template <typename GfxFamily>
void dumpImageInBmpFormat(GraphicsAllocation &gfxAllocation, AubMemDump::AubFileStream *stream, uint32_t context);

template <typename GfxFamily>
void dumpBufferInTreFormat(GraphicsAllocation &gfxAllocation, AubMemDump::AubFileStream *stream, uint32_t context);

template <typename GfxFamily>
void dumpImageInTreFormat(GraphicsAllocation &gfxAllocation, AubMemDump::AubFileStream *stream, uint32_t context);

template <typename GfxFamily>
void dumpAllocation(GraphicsAllocation &gfxAllocation, AubMemDump::AubFileStream *stream, uint32_t context);
} // namespace AubAllocDump
