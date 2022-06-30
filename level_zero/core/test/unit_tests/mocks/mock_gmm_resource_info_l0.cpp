/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_gmm_resource_info.h"

#include "level_zero/core/source/image/image_formats.h"

#include "gtest/gtest.h"

using namespace ::testing;

namespace NEO {

void MockGmmResourceInfo::setSurfaceFormat() {
    auto iterateL0Formats = [&](const std::array<L0::ImageFormats::FormatTypes, L0::ImageFormats::ZE_IMAGE_FORMAT_RENDER_LAYOUT_MAX> &formats) {
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
} // namespace NEO
