/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/image/image_surface_state_fixture.h"

#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_gmm.h"

#include <memory>

namespace NEO {

void ImageSurfaceStateTests::SetUp() {
    DeviceFixture::setUp();
    gmmHelper = pDevice->getGmmHelper();
    mockGmm = std::make_unique<MockGmm>(gmmHelper);
}

} // namespace NEO
