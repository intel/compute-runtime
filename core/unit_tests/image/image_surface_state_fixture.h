/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gmm_helper/gmm.h"
#include "core/gmm_helper/gmm_helper.h"
#include "core/helpers/aligned_memory.h"
#include "core/helpers/basic_math.h"
#include "core/image/image_surface_state.h"
#include "core/memory_manager/graphics_allocation.h"
#include "core/memory_manager/surface.h"
#include "opencl/source/helpers/surface_formats.h"
#include "opencl/source/kernel/kernel.h"
#include "opencl/source/mem_obj/image.h"
#include "test.h"

#include "fixtures/device_fixture.h"
#include "fixtures/image_fixture.h"
#include "mocks/mock_gmm.h"
#include "mocks/mock_gmm_resource_info.h"
#include "mocks/mock_graphics_allocation.h"

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