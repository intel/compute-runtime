/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/surface_format_info.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm_resource_info.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

namespace NEO {
namespace MockGmmParams {
static SurfaceFormatInfo mockSurfaceFormat = {GMM_FORMAT_R8G8B8A8_UNORM_TYPE, GFX3DSTATE_SURFACEFORMAT::GFX3DSTATE_SURFACEFORMAT_R8G8B8A8_UNORM, 0, 4, 1, 4};
}

class MockGmm : public Gmm {
  public:
    using Gmm::Gmm;
    using Gmm::setupImageResourceParams;

    MockGmm(GmmHelper *gmmHelper) : Gmm(gmmHelper, nullptr, 1, 0, GMM_RESOURCE_USAGE_OCL_BUFFER, false, {}, true){};

    static std::unique_ptr<Gmm> queryImgParams(GmmHelper *gmmHelper, ImageInfo &imgInfo, bool preferCompression) {
        return std::unique_ptr<Gmm>(new Gmm(gmmHelper, imgInfo, {}, preferCompression));
    }

    static ImageInfo initImgInfo(ImageDescriptor &imgDesc, int baseMipLevel, const SurfaceFormatInfo *surfaceFormat) {
        ImageInfo imgInfo = {};
        imgInfo.baseMipLevel = baseMipLevel;
        imgInfo.imgDesc = imgDesc;
        if (!surfaceFormat) {
            imgInfo.surfaceFormat = &MockGmmParams::mockSurfaceFormat;
        } else {
            imgInfo.surfaceFormat = surfaceFormat;
        }
        return imgInfo;
    }

    static GraphicsAllocation *allocateImage2d(MemoryManager &memoryManager) {
        ImageDescriptor imgDesc{};
        imgDesc.imageType = ImageType::Image2D;
        imgDesc.imageWidth = 5;
        imgDesc.imageHeight = 5;
        auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
        return memoryManager.allocateGraphicsMemoryWithProperties({mockRootDeviceIndex, true, imgInfo, AllocationType::IMAGE, mockDeviceBitfield});
    }
};
} // namespace NEO
