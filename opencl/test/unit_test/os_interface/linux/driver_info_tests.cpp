/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/source/device/driver_info.h"

#include "gtest/gtest.h"

#include <memory>
#include <string>

namespace NEO {

TEST(DriverInfo, GivenCreateDriverInfoWhenLinuxThenReturnNewInstance) {
    std::unique_ptr<DriverInfo> driverInfo(DriverInfo::create(nullptr));

    EXPECT_NE(nullptr, driverInfo.get());
}

TEST(DriverInfo, GivenDriverInfoWhenLinuxThenReturnDefault) {
    std::unique_ptr<DriverInfo> driverInfo(DriverInfo::create(nullptr));

    std::string defaultName = "testName";
    std::string defaultVersion = "testVersion";

    auto resultName = driverInfo.get()->getDeviceName(defaultName);
    auto resultVersion = driverInfo.get()->getVersion(defaultVersion);

    EXPECT_STREQ(defaultName.c_str(), resultName.c_str());
    EXPECT_STREQ(defaultVersion.c_str(), resultVersion.c_str());
}

} // namespace NEO