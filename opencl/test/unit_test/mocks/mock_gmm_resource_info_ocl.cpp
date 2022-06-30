/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_gmm_resource_info.h"

#include "opencl/source/helpers/surface_formats.h"

#include "gtest/gtest.h"

namespace NEO {

void MockGmmResourceInfo::setSurfaceFormat() {
    auto iterate = [&](ArrayRef<const ClSurfaceFormatInfo> formats) {
        if (!surfaceFormatInfo) {
            for (auto &format : formats) {
                if (mockResourceCreateParams.Format == format.surfaceFormat.GMMSurfaceFormat) {
                    surfaceFormatInfo = &format.surfaceFormat;
                    break;
                }
            }
        }
    };

    if (mockResourceCreateParams.Format == GMM_RESOURCE_FORMAT::GMM_FORMAT_P010 || mockResourceCreateParams.Format == GMM_RESOURCE_FORMAT::GMM_FORMAT_P016) {
        tempSurface.GMMSurfaceFormat = mockResourceCreateParams.Format;
        tempSurface.NumChannels = 1;
        tempSurface.ImageElementSizeInBytes = 16;
        tempSurface.PerChannelSizeInBytes = 16;

        surfaceFormatInfo = &tempSurface;
    }

    if (mockResourceCreateParams.Format == GMM_RESOURCE_FORMAT::GMM_FORMAT_RGBP) {
        tempSurface.GMMSurfaceFormat = GMM_RESOURCE_FORMAT::GMM_FORMAT_RGBP;
        tempSurface.NumChannels = 1;
        tempSurface.ImageElementSizeInBytes = 8;
        tempSurface.PerChannelSizeInBytes = 8;

        surfaceFormatInfo = &tempSurface;
    }

    iterate(SurfaceFormats::readOnly12());
    iterate(SurfaceFormats::readOnly20());
    iterate(SurfaceFormats::writeOnly());
    iterate(SurfaceFormats::readWrite());

    iterate(SurfaceFormats::packedYuv());
    iterate(SurfaceFormats::planarYuv());
    iterate(SurfaceFormats::packed());

    iterate(SurfaceFormats::readOnlyDepth());
    iterate(SurfaceFormats::readWriteDepth());

    ASSERT_NE(nullptr, surfaceFormatInfo);
}
} // namespace NEO
