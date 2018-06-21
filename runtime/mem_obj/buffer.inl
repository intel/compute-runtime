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
#include "runtime/helpers/surface_formats.h"
#include "runtime/helpers/aligned_memory.h"
#include "runtime/mem_obj/buffer.h"
#include "runtime/gmm_helper/gmm_helper.h"

namespace OCLRT {

union SURFACE_STATE_BUFFER_LENGTH {
    uint32_t Length;
    struct SurfaceState {
        uint32_t Width : BITFIELD_RANGE(0, 6);
        uint32_t Height : BITFIELD_RANGE(7, 20);
        uint32_t Depth : BITFIELD_RANGE(21, 31);
    } SurfaceState;
};

template <typename GfxFamily>
void BufferHw<GfxFamily>::setArgStateful(void *memory) {

    using RENDER_SURFACE_STATE = typename GfxFamily::RENDER_SURFACE_STATE;
    using SURFACE_FORMAT = typename RENDER_SURFACE_STATE::SURFACE_FORMAT;
    auto surfaceState = reinterpret_cast<RENDER_SURFACE_STATE *>(memory);
    auto surfaceSize = alignUp(getSize(), 4);

    SURFACE_STATE_BUFFER_LENGTH Length = {0};
    Length.Length = static_cast<uint32_t>(surfaceSize - 1);

    surfaceState->setWidth(Length.SurfaceState.Width + 1);
    surfaceState->setHeight(Length.SurfaceState.Height + 1);
    surfaceState->setDepth(Length.SurfaceState.Depth + 1);

    // The graphics allocation for Host Ptr surface will be created in makeResident call and GPU address is expected to be the same as CPU address
    auto bufferAddress = (getGraphicsAllocation() != nullptr) ? getGraphicsAllocation()->getGpuAddress() : reinterpret_cast<uint64_t>(getHostPtr());
    bufferAddress += this->offset;

    auto bufferSize = (getGraphicsAllocation() != nullptr) ? getGraphicsAllocation()->getUnderlyingBufferSize() : getSize();

    if (bufferAddress != 0) {
        surfaceState->setSurfaceType(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_BUFFER);
    } else {
        surfaceState->setSurfaceType(RENDER_SURFACE_STATE::SURFACE_TYPE_SURFTYPE_NULL);
    }
    surfaceState->setSurfaceFormat(SURFACE_FORMAT::SURFACE_FORMAT_RAW);
    surfaceState->setSurfaceVerticalAlignment(RENDER_SURFACE_STATE::SURFACE_VERTICAL_ALIGNMENT_VALIGN_4);
    surfaceState->setSurfaceHorizontalAlignment(RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_4);

    surfaceState->setTileMode(RENDER_SURFACE_STATE::TILE_MODE_LINEAR);
    surfaceState->setVerticalLineStride(0);
    surfaceState->setVerticalLineStrideOffset(0);

    if ((isAligned<MemoryConstants::cacheLineSize>(bufferAddress) && isAligned<MemoryConstants::cacheLineSize>(bufferSize)) ||
        ((getFlags() & CL_MEM_READ_ONLY)) != 0) {
        surfaceState->setMemoryObjectControlState(GmmHelper::getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER));
    } else {
        surfaceState->setMemoryObjectControlState(GmmHelper::getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED));
    }

    surfaceState->setCoherencyType(RENDER_SURFACE_STATE::COHERENCY_TYPE_IA_COHERENT);
    surfaceState->setSurfaceBaseAddress(bufferAddress);
}
} // namespace OCLRT
