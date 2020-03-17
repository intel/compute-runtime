/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/memory_manager/memory_manager.h"

#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "opencl/test/unit_test/mocks/mock_gmm_resource_info.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

namespace NEO {
namespace MockGmmParams {
static ClSurfaceFormatInfo mockSurfaceFormat;
}

class MockGmm : public Gmm {
  public:
    using Gmm::Gmm;
    using Gmm::setupImageResourceParams;

    MockGmm() : Gmm(platform()->peekGmmClientContext(), nullptr, 1, false){};

    static std::unique_ptr<Gmm> queryImgParams(GmmClientContext *clientContext, ImageInfo &imgInfo) {
        return std::unique_ptr<Gmm>(new Gmm(clientContext, imgInfo, {}));
    }

    static ImageInfo initImgInfo(cl_image_desc &imgDesc, int baseMipLevel, const ClSurfaceFormatInfo *surfaceFormat) {
        ImageInfo imgInfo = {};
        imgInfo.baseMipLevel = baseMipLevel;
        imgInfo.imgDesc = Image::convertDescriptor(imgDesc);
        if (!surfaceFormat) {
            ArrayRef<const ClSurfaceFormatInfo> readWriteSurfaceFormats = SurfaceFormats::readWrite();
            MockGmmParams::mockSurfaceFormat = readWriteSurfaceFormats[0]; // any valid format
            imgInfo.surfaceFormat = &MockGmmParams::mockSurfaceFormat.surfaceFormat;
        } else {
            imgInfo.surfaceFormat = &surfaceFormat->surfaceFormat;
        }
        return imgInfo;
    }

    static GraphicsAllocation *allocateImage2d(MemoryManager &memoryManager) {
        cl_image_desc imgDesc{};
        imgDesc.image_type = CL_MEM_OBJECT_IMAGE2D;
        imgDesc.image_width = 5;
        imgDesc.image_height = 5;
        auto imgInfo = MockGmm::initImgInfo(imgDesc, 0, nullptr);
        return memoryManager.allocateGraphicsMemoryWithProperties({0, true, imgInfo, GraphicsAllocation::AllocationType::IMAGE});
    }
};
} // namespace NEO
