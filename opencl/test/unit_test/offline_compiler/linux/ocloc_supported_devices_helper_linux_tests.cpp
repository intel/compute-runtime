/*
 * Copyright (C) 2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/offline_compiler/source/ocloc_supported_devices_helper.h"
#include "shared/test/common/helpers/variable_backup.h"

#include "opencl/test/unit_test/offline_compiler/mock/mock_ocloc_supported_devices_helper.h"

#include "gtest/gtest.h"

namespace Ocloc {

extern std::string oclocCurrentLibName;
extern std::string oclocFormerLibName;
} // namespace Ocloc

using namespace Ocloc;
using namespace NEO;

struct SupportedDevicesHelperLinuxTest : public ::testing::Test {
    void SetUp() override {
        mockHelper.getOclocCurrentVersionMockResult = "";
        oclocCurrentLibName.shrink_to_fit();
        oclocFormerLibName.shrink_to_fit();
    }

    void TearDown() override {
        oclocCurrentLibName.shrink_to_fit();
        oclocFormerLibName.shrink_to_fit();
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
    VariableBackup<std::string> oclocNameBackup(&oclocCurrentLibName, "libocloc-current.so");
    EXPECT_EQ("ocloc-current", mockHelper.getOclocCurrentVersion());
}

TEST_F(SupportedDevicesHelperLinuxTest, GivenSupportedDevicesHelperWhenGetOclocFormerVersionThenReturnCorrectValue) {
    VariableBackup<std::string> oclocNameBackup(&oclocFormerLibName, "libocloc-former.so");
    EXPECT_EQ("ocloc-former", mockHelper.getOclocFormerVersion());
}
