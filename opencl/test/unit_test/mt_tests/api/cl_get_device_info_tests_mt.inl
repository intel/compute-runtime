/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_cl_device.h"

using namespace NEO;

namespace ULT {

using clGetDeviceInfoMtTests = Test<PlatformFixture>;

TEST_F(clGetDeviceInfoMtTests, GivenMultipleThreadsQueryingDeviceExtensionsWithVersionThenReturnedValuesAreValid) {
    UltClDeviceFactory deviceFactory{1, 0};
    deviceFactory.rootDevices[0]->getDeviceInfo(CL_DEVICE_EXTENSIONS_WITH_VERSION, 0, nullptr, nullptr);
    auto extensionsCount = deviceFactory.rootDevices[0]->deviceInfo.extensionsWithVersion.size();

    std::vector<cl_name_version> extensionsWithVersionArray[4];
    for (auto &extensionsWithVersion : extensionsWithVersionArray) {
        extensionsWithVersion.resize(extensionsCount);
    }

    std::vector<std::thread> threads;
    for (auto &extensionsWithVersion : extensionsWithVersionArray) {
        threads.push_back(std::thread{[&] {
            clGetDeviceInfo(devices[0], CL_DEVICE_EXTENSIONS_WITH_VERSION,
                            sizeof(cl_name_version) * extensionsCount, extensionsWithVersion.data(), nullptr);
        }});
    }

    for (auto &thread : threads) {
        thread.join();
    }

    auto &deviceInfo = deviceFactory.rootDevices[0]->deviceInfo;
    for (auto &extensionsWithVersion : extensionsWithVersionArray) {
        for (size_t i = 0; i < extensionsCount; i++) {
            EXPECT_STREQ(deviceInfo.extensionsWithVersion[i].name, extensionsWithVersion[i].name);
            EXPECT_EQ(deviceInfo.extensionsWithVersion[i].version, extensionsWithVersion[i].version);
        }
    }
}

} // namespace ULT
