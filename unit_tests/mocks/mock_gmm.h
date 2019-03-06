/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/surface_formats.h"
#include "unit_tests/mocks/mock_device.h"
#include "unit_tests/mocks/mock_gmm_resource_info.h"

namespace OCLRT {
namespace MockGmmParams {
static SurfaceFormatInfo mockSurfaceFormat;
}

class MockGmm : public Gmm {
  public:
    static std::unique_ptr<Gmm> queryImgParams(ImageInfo &imgInfo) {
        return std::unique_ptr<Gmm>(new Gmm(imgInfo));
    }

    static ImageInfo initImgInfo(cl_image_desc &imgDesc, int baseMipLevel, const SurfaceFormatInfo *surfaceFormat) {
        ImageInfo imgInfo = {0};
        imgInfo.baseMipLevel = baseMipLevel;
        imgInfo.imgDesc = &imgDesc;
        if (!surfaceFormat) {
            ArrayRef<const SurfaceFormatInfo> readWriteSurfaceFormats = SurfaceFormats::readWrite();
            MockGmmParams::mockSurfaceFormat = readWriteSurfaceFormats[0]; // any valid format
            imgInfo.surfaceFormat = &MockGmmParams::mockSurfaceFormat;
        } else {
            imgInfo.surfaceFormat = surfaceFormat;
        }
        return imgInfo;
    }

    static GraphicsAllocation *allocateImage2d(MemoryManager &memoryManager) {
        cl_image_desc imgDesc{};
        imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imgDesc.image_width = 5;
        imgDesc.image_height = 5;
        auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
        return memoryManager.allocateGraphicsMemoryWithProperties(AllocationProperties{&imgInfo, true});
    }
};
} // namespace OCLRT
