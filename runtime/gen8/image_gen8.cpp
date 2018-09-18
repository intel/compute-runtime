/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "runtime/mem_obj/image.h"
#include "runtime/mem_obj/image.inl"
#include <map>

namespace OCLRT {

typedef BDWFamily Family;
static auto gfxCore = IGFX_GEN8_CORE;

template <typename GfxFamily>
void ImageHw<GfxFamily>::setMediaSurfaceRotation(void *) {}

template <typename GfxFamily>
void ImageHw<GfxFamily>::setSurfaceMemoryObjectControlStateIndexToMocsTable(void *, uint32_t) {}

template <>
size_t ImageHw<BDWFamily>::getHostPtrRowPitchForMap(uint32_t mipLevel) {
    if (getImageDesc().num_mip_levels <= 1) {
        return getHostPtrRowPitch();
    }
    size_t mipWidth = (getImageDesc().image_width >> mipLevel) > 0 ? (getImageDesc().image_width >> mipLevel) : 1;
    return mipWidth * getSurfaceFormatInfo().ImageElementSizeInBytes;
}

template <>
size_t ImageHw<BDWFamily>::getHostPtrSlicePitchForMap(uint32_t mipLevel) {
    if (getImageDesc().num_mip_levels <= 1) {
        return getHostPtrSlicePitch();
    }
    size_t mipHeight = (getImageDesc().image_height >> mipLevel) > 0 ? (getImageDesc().image_height >> mipLevel) : 1;
    size_t rowPitch = getHostPtrRowPitchForMap(mipLevel);

    return rowPitch * mipHeight;
}

#include "runtime/mem_obj/image_factory_init.inl"
} // namespace OCLRT
