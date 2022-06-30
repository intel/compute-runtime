/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/mocks/mock_gmm_resource_info.h"

namespace NEO {

void MockGmmResourceInfo::setSurfaceFormat() {
    if (mockResourceCreateParams.Format == GMM_FORMAT_GENERIC_8BIT) {
        static const NEO::SurfaceFormatInfo surfaceFormatGMM8BITCommon = {GMM_FORMAT_GENERIC_8BIT, GFX3DSTATE_SURFACEFORMAT_R8_UNORM, 0, 1, 1, 1};
        surfaceFormatInfo = &surfaceFormatGMM8BITCommon;
        return;
    }

    static const NEO::SurfaceFormatInfo surfaceFormatGeneric = {GMM_FORMAT_R8_UINT_TYPE, GFX3DSTATE_SURFACEFORMAT_R8_UINT, 0, 1, 1, 1};
    surfaceFormatInfo = &surfaceFormatGeneric;
}
} // namespace NEO
