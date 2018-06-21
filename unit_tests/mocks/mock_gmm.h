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
    static std::unique_ptr<Gmm> queryImgParams(ImageInfo &imgInfo, const HardwareInfo *hwInfo = nullptr) {
        auto queryHwInfo = hwInfo;
        if (!queryHwInfo) {
            queryHwInfo = *platformDevices;
        }
        return std::unique_ptr<Gmm>(GmmHelper::createGmmAndQueryImgParams(imgInfo, *queryHwInfo));
    }

    static ImageInfo initImgInfo(cl_image_desc &imgDesc, int baseMipLevel, const SurfaceFormatInfo *surfaceFormat) {
        ImageInfo imgInfo = {0};
        imgInfo.baseMipLevel = baseMipLevel;
        imgInfo.imgDesc = &imgDesc;
        if (!surfaceFormat) {
            MockGmmParams::mockSurfaceFormat = readWriteSurfaceFormats[0]; // any valid format
            imgInfo.surfaceFormat = &MockGmmParams::mockSurfaceFormat;
        } else {
            imgInfo.surfaceFormat = surfaceFormat;
        }
        return imgInfo;
    }
};
} // namespace OCLRT
