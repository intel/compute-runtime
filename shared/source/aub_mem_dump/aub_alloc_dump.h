/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/aub_mem_dump/aub_mem_dump.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/memory_manager/graphics_allocation.h"

namespace NEO {
class GmmHelper;
}

using namespace NEO;

namespace aub_stream {
struct SurfaceInfo;
}

namespace AubAllocDump {

enum DumpFormat {
    NONE,
    BUFFER_BIN,
    BUFFER_TRE,
    IMAGE_BMP,
    IMAGE_TRE,
};

inline bool isWritableBuffer(GraphicsAllocation &gfxAllocation) {
    return (gfxAllocation.getAllocationType() == AllocationType::buffer ||
            gfxAllocation.getAllocationType() == AllocationType::bufferHostMemory ||
            gfxAllocation.getAllocationType() == AllocationType::externalHostPtr ||
            gfxAllocation.getAllocationType() == AllocationType::mapAllocation ||
            gfxAllocation.getAllocationType() == AllocationType::svmGpu) &&
           gfxAllocation.isMemObjectsAllocationWithWritableFlags();
}

inline bool isWritableImage(GraphicsAllocation &gfxAllocation) {
    return (gfxAllocation.getAllocationType() == AllocationType::image) &&
           gfxAllocation.isMemObjectsAllocationWithWritableFlags();
}

inline DumpFormat getDumpFormat(GraphicsAllocation &gfxAllocation) {
    auto dumpBufferFormat = debugManager.flags.AUBDumpBufferFormat.get();
    auto dumpImageFormat = debugManager.flags.AUBDumpImageFormat.get();
    auto isDumpableBuffer = isWritableBuffer(gfxAllocation);
    auto isDumpableImage = isWritableImage(gfxAllocation);
    auto dumpFormat = DumpFormat::NONE;

    if (isDumpableBuffer) {
        if (0 == dumpBufferFormat.compare("BIN")) {
            dumpFormat = DumpFormat::BUFFER_BIN;
        } else if (0 == dumpBufferFormat.compare("TRE")) {
            dumpFormat = DumpFormat::BUFFER_TRE;
        }
    } else if (isDumpableImage) {
        if (0 == dumpImageFormat.compare("BMP")) {
            dumpFormat = DumpFormat::IMAGE_BMP;
        } else if (0 == dumpImageFormat.compare("TRE")) {
            dumpFormat = DumpFormat::IMAGE_TRE;
        }
    }

    return dumpFormat;
}

inline bool isBufferDumpFormat(DumpFormat dumpFormat) {
    return (AubAllocDump::DumpFormat::BUFFER_BIN == dumpFormat) || (dumpFormat == AubAllocDump::DumpFormat::BUFFER_TRE);
}

inline bool isImageDumpFormat(DumpFormat dumpFormat) {
    return (AubAllocDump::DumpFormat::IMAGE_BMP == dumpFormat) || (dumpFormat == AubAllocDump::DumpFormat::IMAGE_TRE);
}

template <typename GfxFamily>
aub_stream::SurfaceInfo *getDumpSurfaceInfo(GraphicsAllocation &gfxAllocation, const GmmHelper &gmmHelper, DumpFormat dumpFormat);

template <typename GfxFamily>
uint32_t getImageSurfaceTypeFromGmmResourceType(GMM_RESOURCE_TYPE gmmResourceType);
} // namespace AubAllocDump
