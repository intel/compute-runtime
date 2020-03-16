/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/root_device.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/hw_info_config.h"

#include "opencl/test/unit_test/api/cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetPlatformIDsTests;

TEST(clGetPlatformIDsMultiPlatformTest, whenCreateDevicesWithDifferentProductFamilyThenClGetPlatformIdsCreatesMultiplePlatformsProperlySorted) {
    if ((HwInfoConfig::get(IGFX_SKYLAKE) == nullptr) || (HwInfoConfig::get(IGFX_KABYLAKE) == nullptr)) {
        GTEST_SKIP();
    }
    DebugManagerStateRestore restorer;
    const size_t numRootDevices = 2u;
    DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    VariableBackup<decltype(DeviceFactory::createRootDeviceFunc)> createFuncBackup{&DeviceFactory::createRootDeviceFunc};
    DeviceFactory::createRootDeviceFunc = [](ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) -> std::unique_ptr<Device> {
        auto device = std::unique_ptr<Device>(Device::create<RootDevice>(&executionEnvironment, rootDeviceIndex));
        auto hwInfo = device->getRootDeviceEnvironment().getMutableHardwareInfo();
        if (rootDeviceIndex == 0) {
            hwInfo->platform.eProductFamily = IGFX_SKYLAKE;
        } else {
            hwInfo->platform.eProductFamily = IGFX_KABYLAKE;
        }
        return device;
    };
    platformsImpl.clear();

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformsRet[2];
    cl_uint numPlatforms = 0;

    retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);
    EXPECT_EQ(2u, numPlatforms);
    EXPECT_EQ(CL_SUCCESS, retVal);

    numPlatforms = 0u;
    retVal = clGetPlatformIDs(2u, platformsRet, &numPlatforms);
    EXPECT_EQ(2u, numPlatforms);
    EXPECT_EQ(CL_SUCCESS, retVal);

    EXPECT_NE(nullptr, platformsRet[0]);
    auto platform0 = castToObject<Platform>(platformsRet[0]);
    EXPECT_EQ(1u, platform0->getNumDevices());
    EXPECT_EQ(IGFX_KABYLAKE, platform0->getClDevice(0)->getHardwareInfo().platform.eProductFamily);
    EXPECT_EQ(1u, platform0->getClDevice(0)->getRootDeviceIndex());

    EXPECT_NE(nullptr, platformsRet[1]);
    auto platform1 = castToObject<Platform>(platformsRet[1]);
    EXPECT_EQ(1u, platform1->getNumDevices());
    EXPECT_EQ(IGFX_SKYLAKE, platform1->getClDevice(0)->getHardwareInfo().platform.eProductFamily);
    EXPECT_EQ(0u, platform1->getClDevice(0)->getRootDeviceIndex());
    platformsImpl.clear();
}
