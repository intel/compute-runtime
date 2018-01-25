/*
* Copyright (c) 2017, Intel Corporation
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

#include "runtime/helpers/aligned_memory.h"
#include "runtime/helpers/surface_formats.h"
#include "unit_tests/mocks/mock_gmm_resource_info.h"

using namespace ::testing;

namespace OCLRT {
GmmResourceInfo *GmmResourceInfo::create(GMM_RESCREATE_PARAMS *resourceCreateParams) {
    return new ::testing::NiceMock<MockGmmResourceInfo>(resourceCreateParams);
}

GmmResourceInfo *GmmResourceInfo::create(GMM_RESOURCE_INFO *inputGmmResourceInfo) {
    return new ::testing::NiceMock<MockGmmResourceInfo>(inputGmmResourceInfo);
}

MockGmmResourceInfo::MockGmmResourceInfo(GMM_RESCREATE_PARAMS *resourceCreateParams) {
    mockResourceCreateParams = *resourceCreateParams;
    setupDefaultActions();
}

MockGmmResourceInfo::MockGmmResourceInfo(GMM_RESOURCE_INFO *inputGmmResourceInfo) {
    mockResourceCreateParams = reinterpret_cast<MockGmmResourceInfo *>(inputGmmResourceInfo)->mockResourceCreateParams;
    setupDefaultActions();
};

// Simulate GMM behaviour. We dont want to test 3rd party lib
void MockGmmResourceInfo::setupDefaultActions() {
    setSurfaceFormat();
    computeRowPitch();

    size = rowPitch;
    size *= static_cast<size_t>(mockResourceCreateParams.BaseHeight);

    qPitch = alignUp((uint32_t)size, 64);

    size *= mockResourceCreateParams.Depth ? static_cast<size_t>(mockResourceCreateParams.Depth) : 1;
    size *= mockResourceCreateParams.ArraySize ? static_cast<size_t>(mockResourceCreateParams.ArraySize) : 1;
    size = alignUp(size, MemoryConstants::pageSize);

    ON_CALL(*this, getOffset(_)).WillByDefault(Invoke([&](GMM_REQ_OFFSET_INFO &reqOffsetInfo) {
        reqOffsetInfo.Lock.Offset = 16;
        reqOffsetInfo.Lock.Pitch = 2;
        reqOffsetInfo.Render.YOffset = 1;
        if (mockResourceCreateParams.Format == GMM_RESOURCE_FORMAT::GMM_FORMAT_NV12) {
            reqOffsetInfo.Render.XOffset = 32;
            reqOffsetInfo.Render.Offset = 64;
        }

        return GMM_SUCCESS;
    }));
}

void MockGmmResourceInfo::computeRowPitch() {
    if (mockResourceCreateParams.OverridePitch) {
        rowPitch = mockResourceCreateParams.OverridePitch;
    } else {
        rowPitch = static_cast<size_t>(mockResourceCreateParams.BaseWidth * (surfaceFormatInfo->PerChannelSizeInBytes * surfaceFormatInfo->NumChannels));
        rowPitch = alignUp(rowPitch, 64);
    }
}

void MockGmmResourceInfo::setSurfaceFormat() {
    auto iterate = [&](const SurfaceFormatInfo *formats, const size_t numFormats) {
        if (!surfaceFormatInfo) {
            for (size_t i = 0; i < numFormats; i++) {
                if (mockResourceCreateParams.Format == formats[i].GMMSurfaceFormat) {
                    surfaceFormatInfo = &formats[i];
                    break;
                }
            }
        }
    };

    iterate(readOnlySurfaceFormats, numReadOnlySurfaceFormats);
    iterate(writeOnlySurfaceFormats, numWriteOnlySurfaceFormats);
    iterate(readWriteSurfaceFormats, numReadWriteSurfaceFormats);

    iterate(packedYuvSurfaceFormats, numPackedYuvSurfaceFormats);
    iterate(planarYuvSurfaceFormats, numPlanarYuvSurfaceFormats);

    iterate(readOnlyDepthSurfaceFormats, numReadOnlyDepthSurfaceFormats);
    iterate(readWriteDepthSurfaceFormats, numReadWriteDepthSurfaceFormats);

    iterate(snormSurfaceFormats, numSnormSurfaceFormats);

    ASSERT_NE(nullptr, surfaceFormatInfo);
}

uint32_t MockGmmResourceInfo::getBitsPerPixel() {
    return (surfaceFormatInfo->PerChannelSizeInBytes << 3) * surfaceFormatInfo->NumChannels;
}

void MockGmmResourceInfo::setUnifiedAuxTranslationCapable() {
    mockResourceCreateParams.Flags.Gpu.CCS = 1;
    mockResourceCreateParams.Flags.Gpu.UnifiedAuxSurface = 1;
    mockResourceCreateParams.Flags.Info.RenderCompressed = 1;
}

MockGmmResourceInfo::MockGmmResourceInfo() {}
MockGmmResourceInfo::~MockGmmResourceInfo() {}
} // namespace OCLRT
