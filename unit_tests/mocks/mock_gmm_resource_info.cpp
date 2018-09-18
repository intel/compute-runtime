/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
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
}

GMM_STATUS MockGmmResourceInfo::getOffset(GMM_REQ_OFFSET_INFO &reqOffsetInfo) {
    arrayIndexPassedToGetOffset = reqOffsetInfo.ArrayIndex;
    getOffsetCalled++;

    reqOffsetInfo.Lock.Offset = 16;
    reqOffsetInfo.Lock.Pitch = 2;
    reqOffsetInfo.Render.YOffset = 1;
    if (mockResourceCreateParams.Format == GMM_RESOURCE_FORMAT::GMM_FORMAT_NV12) {
        reqOffsetInfo.Render.XOffset = 32;
        reqOffsetInfo.Render.Offset = 64;
    }
    if (reqOffsetInfo.Slice == 1) {
        reqOffsetInfo.Render.YOffset = mockResourceCreateParams.BaseHeight;
    }
    if (reqOffsetInfo.MipLevel > 0) {
        reqOffsetInfo.Lock.Offset = 32;
    }

    return GMM_SUCCESS;
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
