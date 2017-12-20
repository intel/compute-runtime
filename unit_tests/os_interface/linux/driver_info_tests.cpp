/*
* Copyright (c) 2017, Intel Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
* OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*/

#include "runtime/device/driver_info.h"
#include "gtest/gtest.h"
#include <memory>
#include <string>

namespace OCLRT {

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

} // namespace OCLRT