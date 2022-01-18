/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/aligned_memory.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/image/image_surface_state.h"
#include "shared/source/memory_manager/graphics_allocation.h"
#include "shared/source/memory_manager/surface.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/mocks/mock_gmm.h"
#include "shared/test/common/test_macros/test.h"

#include <memory>

namespace NEO {

class ImageSurfaceStateTests : public DeviceFixture,
                               public testing::Test {
  public:
    ImageSurfaceStateTests() = default;
    void SetUp() override {
        DeviceFixture::SetUp();
        mockGmm = std::make_unique<MockGmm>(pDevice->getGmmClientContext());
        gmmHelper = pDevice->getGmmHelper();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    std::unique_ptr<MockGmm> mockGmm;
    GmmHelper *gmmHelper = nullptr;
    NEO::ImageInfo imageInfo{};
};

} // namespace NEO
