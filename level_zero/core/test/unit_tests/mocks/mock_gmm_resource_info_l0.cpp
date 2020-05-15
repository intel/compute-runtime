/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/aligned_memory.h"

#include "opencl/test/unit_test/mocks/mock_gmm_resource_info.h"

#include "level_zero/core/source/image/image_formats.h"

using namespace ::testing;

namespace NEO {
GmmResourceInfo *GmmResourceInfo::create(GmmClientContext *clientContext, GMM_RESCREATE_PARAMS *resourceCreateParams) {
    if (resourceCreateParams->Type == GMM_RESOURCE_TYPE::RESOURCE_INVALID) {
        return nullptr;
    }
    return new ::testing::NiceMock<MockGmmResourceInfo>(resourceCreateParams);
}

GmmResourceInfo *GmmResourceInfo::create(GmmClientContext *clientContext, GMM_RESOURCE_INFO *inputGmmResourceInfo) {
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
    if (surfaceFormatInfo == nullptr) {
        ASSERT_TRUE(false);
        return;
    }
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
        rowPitch = static_cast<size_t>(mockResourceCreateParams.BaseWidth64 * (surfaceFormatInfo->ImageElementSizeInBytes));
        rowPitch = alignUp(rowPitch, 64);
    }
}

void MockGmmResourceInfo::setSurfaceFormat() {
    auto iterateL0Formats = [&](const std::array<L0::ImageFormats::FormatTypes, 30> &formats) {
        if (!surfaceFormatInfo) {
            for (auto &formatArray : formats) {
                for (auto &format : formatArray) {
                    if (mockResourceCreateParams.Format == format.GMMSurfaceFormat) {
                        surfaceFormatInfo = &format;
                        ASSERT_NE(nullptr, surfaceFormatInfo);
                        return;
                    }
                }
            }
        }
    };

    iterateL0Formats(L0::ImageFormats::formats);

    if (mockResourceCreateParams.Format == GMM_FORMAT_GENERIC_8BIT) {
        static const NEO::SurfaceFormatInfo surfaceFormatGMM8BIT = {GMM_FORMAT_GENERIC_8BIT, GFX3DSTATE_SURFACEFORMAT_R8_UNORM, 0, 1, 1, 1};
        surfaceFormatInfo = &surfaceFormatGMM8BIT;
    }

    ASSERT_NE(nullptr, surfaceFormatInfo);
}

uint32_t MockGmmResourceInfo::getBitsPerPixel() {
    return static_cast<uint32_t>(surfaceFormatInfo->ImageElementSizeInBytes * 8);
}

void MockGmmResourceInfo::setUnifiedAuxTranslationCapable() {
    mockResourceCreateParams.Flags.Gpu.CCS = 1;
    mockResourceCreateParams.Flags.Gpu.UnifiedAuxSurface = 1;
    mockResourceCreateParams.Flags.Info.RenderCompressed = 1;
}

void MockGmmResourceInfo::setMultisampleControlSurface() {
    mockResourceCreateParams.Flags.Gpu.MCS = 1;
}

void MockGmmResourceInfo::setUnifiedAuxPitchTiles(uint32_t value) {
    this->unifiedAuxPitch = value;
}
void MockGmmResourceInfo::setAuxQPitch(uint32_t value) {
    this->auxQPitch = value;
}

uint32_t MockGmmResourceInfo::getTileModeSurfaceState() {
    if (mockResourceCreateParams.Flags.Info.Linear == 1) {
        return 0;
    }

    if (mockResourceCreateParams.Type == GMM_RESOURCE_TYPE::RESOURCE_2D ||
        mockResourceCreateParams.Type == GMM_RESOURCE_TYPE::RESOURCE_3D) {
        return 3;
    } else {
        return 0;
    }
}

MockGmmResourceInfo::MockGmmResourceInfo() {}
MockGmmResourceInfo::~MockGmmResourceInfo() {}
} // namespace NEO
