/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_supported_devices_helper.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_ocloc_supported_devices_helper.h"

#include "gtest/gtest.h"

using namespace NEO;

struct SupportedDevicesHelperWindowsTest : public ::testing::Test {
    void SetUp() override {
        mockHelper.getCurrentOclocNameMockResult = "";
    }

    void TearDown() override {
    }

    MockSupportedDevicesHelper mockHelper = MockSupportedDevicesHelper(SupportedDevicesMode::concat, nullptr);
};

TEST_F(SupportedDevicesHelperWindowsTest, GivenVariousOclocLibraryNamesWhenExtractingOclocNameThenEmptyStringIsReturned) {
    EXPECT_EQ(mockHelper.extractOclocName("libocloc-2.0.1.so"), "");
    EXPECT_EQ(mockHelper.extractOclocName("libocloc-2.0.so"), "");
    EXPECT_EQ(mockHelper.extractOclocName("libocloc2.0.so"), "");
    EXPECT_EQ(mockHelper.extractOclocName("libocloc-2.0"), "");
    EXPECT_EQ(mockHelper.extractOclocName("libocloc.so"), "");
}

TEST_F(SupportedDevicesHelperWindowsTest, GivenSupportedDevicesHelperWhenGetCurrentOclocNameThenReturnCorrectValue) {
    EXPECT_EQ("ocloc", mockHelper.getCurrentOclocName());
}

TEST_F(SupportedDevicesHelperWindowsTest, GivenSupportedDevicesHelperWhenGetFormerOclocNameThenReturnCorrectValue) {
    EXPECT_EQ("", mockHelper.getFormerOclocName());
}

TEST_F(SupportedDevicesHelperWindowsTest, GivenSupportedDevicesHelperWhenGetDataFromFormerOclocThenReturnEmptyData) {
    SupportedDevicesHelper helper(SupportedDevicesMode::concat, nullptr);
    EXPECT_EQ("", helper.getDataFromFormerOcloc());
}
