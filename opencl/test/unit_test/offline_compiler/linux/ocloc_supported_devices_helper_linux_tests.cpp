/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_supported_devices_helper.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_ocloc_supported_devices_helper.h"

#include "gtest/gtest.h"

namespace NEO {
using namespace Ocloc;

struct SupportedDevicesHelperLinuxTest : public ::testing::Test {
    void SetUp() override {
        mockHelper.getOclocCurrentVersionMockResult = "";
    }

    void TearDown() override {
    }

    MockSupportedDevicesHelper mockHelper = MockSupportedDevicesHelper(SupportedDevicesMode::concat, nullptr);
};

TEST_F(SupportedDevicesHelperLinuxTest, GivenVariousOclocLibraryNamesWhenExtractingOclocVersionThenEmptyStringIsReturned) {
    {
        // Valid Ocloc lib names
        EXPECT_EQ(mockHelper.extractOclocVersion("libocloc-2.0.1.so"), "ocloc-2.0.1");
        EXPECT_EQ(mockHelper.extractOclocVersion("libocloc-2.0.so"), "ocloc-2.0");
    }

    {
        // Invalid Ocloc lib names
        EXPECT_EQ(mockHelper.extractOclocVersion("libocloc2.0.so"), "ocloc");
        EXPECT_EQ(mockHelper.extractOclocVersion("libocloc-2.0"), "ocloc");
        EXPECT_EQ(mockHelper.extractOclocVersion(""), "ocloc");
        EXPECT_EQ(mockHelper.extractOclocVersion("libocloc.so"), "ocloc");
    }
}

TEST_F(SupportedDevicesHelperLinuxTest, GivenSupportedDevicesHelperWhenGetOclocCurrentVersionThenReturnCorrectValue) {
    EXPECT_EQ("ocloc-current", mockHelper.getOclocCurrentVersion());
}

TEST_F(SupportedDevicesHelperLinuxTest, GivenSupportedDevicesHelperWhenGetOclocFormerVersionThenReturnCorrectValue) {
    EXPECT_EQ("ocloc-former", mockHelper.getOclocFormerVersion());
}

} // namespace NEO
