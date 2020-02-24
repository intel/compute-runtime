/*
 * Copyright (C) 2020 Intel Corporation
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

#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/image.h"
#include "opencl/test/unit_test/fixtures/device_fixture.h"
#include "opencl/test/unit_test/fixtures/image_fixture.h"
#include "opencl/test/unit_test/mocks/mock_gmm.h"
#include "opencl/test/unit_test/mocks/mock_gmm_resource_info.h"
#include "opencl/test/unit_test/mocks/mock_graphics_allocation.h"
#include "test.h"

#include <memory>

class ImageSurfaceStateTests : public DeviceFixture,
                               public testing::Test {
  public:
    ImageSurfaceStateTests() = default;
    void SetUp() override {
        DeviceFixture::SetUp();
        gmmHelper = pDevice->getGmmHelper();
    }

    void TearDown() override {
        DeviceFixture::TearDown();
    }

    MockGmm mockGmm;
    GmmHelper *gmmHelper = nullptr;
    NEO::ImageInfo imageInfo;
};