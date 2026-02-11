/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/surface_format_info.h"
#include "shared/test/common/fixtures/device_fixture.h"

#include "gtest/gtest.h"

#include <memory>

namespace NEO {
class GmmHelper;
class MockGmm;

class ImageSurfaceStateTests : public DeviceFixture,
                               public testing::Test {
  public:
    ImageSurfaceStateTests() = default;
    void SetUp() override;

    void TearDown() override {
        DeviceFixture::tearDown();
    }

    std::unique_ptr<MockGmm> mockGmm;
    GmmHelper *gmmHelper = nullptr;
    NEO::ImageInfo imageInfo{};
};

} // namespace NEO
