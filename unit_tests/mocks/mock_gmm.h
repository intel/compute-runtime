/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/gmm_helper/gmm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "unit_tests/mocks/mock_device.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/surface_formats.h"
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
};
} // namespace OCLRT
