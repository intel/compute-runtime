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

struct SupportedDevicesHelperWindowsTest : public ::testing::Test {
    void SetUp() override {
        mockHelper.getOclocCurrentLibNameMockResult = "";
        mockHelper.getOclocFormerLibNameMockResult = "";
        mockHelper.getOclocCurrentVersionMockResult = "";
    }

    void TearDown() override {
    }

    MockSupportedDevicesHelper mockHelper = MockSupportedDevicesHelper(SupportedDevicesMode::concat, nullptr);
};

TEST_F(SupportedDevicesHelperWindowsTest, GivenVariousOclocLibraryNamesWhenExtractingOclocVersionThenEmptyStringIsReturned) {
    EXPECT_EQ(mockHelper.extractOclocVersion("libocloc-2.0.1.so"), "");
    EXPECT_EQ(mockHelper.extractOclocVersion("libocloc-2.0.so"), "");
    EXPECT_EQ(mockHelper.extractOclocVersion("libocloc2.0.so"), "");
    EXPECT_EQ(mockHelper.extractOclocVersion("libocloc-2.0"), "");
    EXPECT_EQ(mockHelper.extractOclocVersion("libocloc.so"), "");
}

TEST_F(SupportedDevicesHelperWindowsTest, GivenSupportedDevicesHelperWhenGetOclocCurrentLibNameThenReturnEmptyString) {
    EXPECT_EQ("", mockHelper.getOclocCurrentLibName());
}

TEST_F(SupportedDevicesHelperWindowsTest, GivenSupportedDevicesHelperWhenGetOclocFormerLibNameThenReturnEmptyString) {
    EXPECT_EQ("", mockHelper.getOclocFormerLibName());
}

TEST_F(SupportedDevicesHelperWindowsTest, GivenSupportedDevicesHelperWhenGetOclocCurrentVersionThenReturnCorrectValue) {
    EXPECT_EQ("ocloc", mockHelper.getOclocCurrentVersion());
}

TEST_F(SupportedDevicesHelperWindowsTest, GivenSupportedDevicesHelperWhenGetOclocFormerVersionThenReturnCorrectValue) {
    EXPECT_EQ("", mockHelper.getOclocFormerVersion());
}

TEST_F(SupportedDevicesHelperWindowsTest, GivenSupportedDevicesHelperWhenGetDataFromFormerOclocVersionThenReturnEmptyData) {
    SupportedDevicesHelper helper(SupportedDevicesMode::concat, nullptr);
    EXPECT_EQ("", helper.getDataFromFormerOclocVersion());
}

} // namespace NEO
