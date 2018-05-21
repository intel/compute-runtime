/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
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
    size_t mipWidth = (getImageDesc().image_width >> mipLevel) > 0 ? (getImageDesc().image_width >> mipLevel) : 1;
    return mipWidth * getSurfaceFormatInfo().ImageElementSizeInBytes;
}

template <>
size_t ImageHw<BDWFamily>::getHostPtrSlicePitchForMap(uint32_t mipLevel) {
    size_t mipHeight = (getImageDesc().image_height >> mipLevel) > 0 ? (getImageDesc().image_height >> mipLevel) : 1;
    size_t rowPitch = getHostPtrRowPitchForMap(mipLevel);

    return rowPitch * mipHeight;
}

#include "runtime/mem_obj/image_factory_init.inl"
}
