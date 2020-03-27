/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/test/unit_test/helpers/default_hw_info.h"

#include "gtest/gtest.h"

#include <memory>
#include <string>

namespace NEO {

TEST(DriverInfo, GivenUninitializedHardwareInfoWhenCreateDriverInfoLinuxThenReturnNull) {
    std::unique_ptr<DriverInfo> driverInfo(DriverInfo::create(nullptr, nullptr));

    EXPECT_EQ(nullptr, driverInfo.get());
}

TEST(DriverInfo, GivenCreateDriverInfoWhenLinuxThenReturnNewInstance) {
    auto hwInfo = *defaultHwInfo;
    std::unique_ptr<DriverInfo> driverInfo(DriverInfo::create(&hwInfo, nullptr));

    EXPECT_NE(nullptr, driverInfo.get());
}

TEST(DriverInfo, GivenDriverInfoWhenLinuxThenReturnDefault) {
    auto hwInfo = *defaultHwInfo;
    std::unique_ptr<DriverInfo> driverInfo(DriverInfo::create(&hwInfo, nullptr));

    std::string defaultName = "testName";
    std::string defaultVersion = "testVersion";

    auto resultName = driverInfo.get()->getDeviceName(defaultName);
    auto resultVersion = driverInfo.get()->getVersion(defaultVersion);

    EXPECT_STREQ(defaultName.c_str(), resultName.c_str());
    EXPECT_STREQ(defaultVersion.c_str(), resultVersion.c_str());
}

TEST(DriverInfo, givenGetMediaSharingSupportWhenLinuxThenReturnTrue) {
    auto hwInfo = *defaultHwInfo;
    std::unique_ptr<DriverInfo> driverInfo(DriverInfo::create(&hwInfo, nullptr));

    EXPECT_TRUE(driverInfo->getMediaSharingSupport());
}

TEST(DriverInfo, givenGetImageSupportWhenHwInfoSupportsImagesThenReturnTrueOtherwiseFalse) {
    auto hwInfo = *defaultHwInfo;

    for (bool supportsImages : {false, true}) {
        hwInfo.capabilityTable.supportsImages = supportsImages;
        std::unique_ptr<DriverInfo> driverInfo(DriverInfo::create(&hwInfo, nullptr));

        EXPECT_EQ(supportsImages, driverInfo->getImageSupport());
    }
}
} // namespace NEO
