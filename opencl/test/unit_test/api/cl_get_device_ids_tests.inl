/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/test/unit_test/fixtures/platform_fixture.h"

using namespace NEO;

using clGetDeviceIDsTests = Test<PlatformFixture>;

namespace ULT {

TEST_F(clGetDeviceIDsTests, GivenZeroNumEntriesWhenGettingDeviceIdsThenNumberOfDevicesIsGreaterThanZero) {
    cl_uint numDevices = 0;

    auto retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, 0, nullptr, &numDevices);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numDevices, (cl_uint)0);
}

TEST_F(clGetDeviceIDsTests, GivenNonNullDevicesWhenGettingDeviceIdsThenDeviceIdIsReturned) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    auto retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetDeviceIDsTests, GivenNullPlatformWhenGettingDeviceIdsThenDeviceIdIsReturned) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    auto retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetDeviceIDsTests, GivenInvalidDeviceTypeWhenGettingDeviceIdsThenInvalidDeviceTypeErrorIsReturned) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    auto retVal = clGetDeviceIDs(pPlatform, 0x0f00, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_INVALID_DEVICE_TYPE, retVal);
}

TEST_F(clGetDeviceIDsTests, GivenZeroNumEntriesAndNonNullDevicesWhenGettingDeviceIdsThenInvalidValueErrorIsReturned) {
    cl_device_id pDevices[1];

    auto retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, 0, pDevices, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetDeviceIDsTests, GivenInvalidPlatformWhenGettingDeviceIdsThenInvalidPlatformErrorIsReturned) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];
    uint32_t trash[6] = {0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef};
    cl_platform_id p = reinterpret_cast<cl_platform_id>(trash);

    auto retVal = clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(clGetDeviceIDsTests, GivenDeviceTypeAllWhenGettingDeviceIdsThenDeviceIdIsReturned) {
    cl_uint numDevices = 0;
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    auto retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_ALL, numEntries, pDevices, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numDevices, (cl_uint)0);
}

TEST_F(clGetDeviceIDsTests, GivenDeviceTypeDefaultWhenGettingDeviceIdsThenDeviceIdIsReturned) {
    cl_uint numDevices = 0;
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    auto retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_DEFAULT, numEntries, pDevices, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numDevices, (cl_uint)0);
}

TEST_F(clGetDeviceIDsTests, GivenDeviceTypeCpuWhenGettingDeviceIdsThenDeviceNotFoundErrorIsReturned) {
    cl_uint numDevices = 0;

    auto retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_CPU, 0, nullptr, &numDevices);

    EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);
    EXPECT_EQ(numDevices, (cl_uint)0);
}

TEST(clGetDeviceIDsTest, givenMultipleRootDevicesWhenGetDeviceIdsThenAllRootDevicesAreReturned) {
    platformsImpl->clear();
    constexpr auto numRootDevices = 3u;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    cl_uint numDevices = 0;
    cl_uint numEntries = numRootDevices;
    cl_device_id devices[numRootDevices];
    auto retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_ALL, numEntries, devices, &numDevices);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(numEntries, numDevices);
    for (auto i = 0u; i < numRootDevices; i++) {
        EXPECT_EQ(devices[i], platform()->getClDevice(i));
    }
}
TEST(clGetDeviceIDsTest, givenMultipleRootDevicesWhenGetDeviceIdsButNumEntriesIsLowerThanNumDevicesThenSubsetOfRootDevicesIsReturned) {
    platformsImpl->clear();
    constexpr auto numRootDevices = 3u;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    cl_uint maxNumDevices;
    auto retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_ALL, 0, nullptr, &maxNumDevices);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(numRootDevices, maxNumDevices);

    cl_uint numDevices = 0;
    cl_uint numEntries = numRootDevices - 1;
    cl_device_id devices[numRootDevices];

    const auto dummyDevice = reinterpret_cast<cl_device_id>(0x1357);
    for (auto i = 0u; i < numRootDevices; i++) {
        devices[i] = dummyDevice;
    }

    retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_ALL, numEntries, devices, &numDevices);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_LT(numDevices, maxNumDevices);
    EXPECT_EQ(numEntries, numDevices);
    for (auto i = 0u; i < numEntries; i++) {
        EXPECT_EQ(devices[i], platform()->getClDevice(i));
    }
    EXPECT_EQ(devices[numEntries], dummyDevice);
}

TEST(clGetDeviceIDsTest, givenReturnSubDevicesAsApiDevicesWhenCallClGetDeviceIDsThenSubDevicesAreReturnedAsSeparateClDevices) {
    platformsImpl->clear();
    constexpr auto numRootDevices = 3u;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    DebugManager.flags.CreateMultipleSubDevices.set(numRootDevices);
    DebugManager.flags.ReturnSubDevicesAsApiDevices.set(1);
    cl_uint maxNumDevices;
    auto retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_ALL, 0, nullptr, &maxNumDevices);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(numRootDevices * numRootDevices, maxNumDevices);

    cl_uint numDevices = 0;
    cl_uint numEntries = maxNumDevices - 1;
    cl_device_id devices[numRootDevices * numRootDevices];

    const auto dummyDevice = reinterpret_cast<cl_device_id>(0x1357);
    for (auto i = 0u; i < maxNumDevices; i++) {
        devices[i] = dummyDevice;
    }

    retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_ALL, numEntries, devices, &numDevices);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_LT(numDevices, maxNumDevices);
    EXPECT_EQ(numEntries, numDevices);
    for (auto i = 0u; i < numEntries; i++) {
        EXPECT_EQ(devices[i], platform()->getClDevice(i / numRootDevices)->getSubDevice(i % numRootDevices));
    }
    EXPECT_EQ(devices[numEntries], dummyDevice);
}

TEST(clGetDeviceIDsTest, givenMultipleRootDevicesAndLimitedNumberOfReturnedDevicesWhenGetDeviceIdsThenLimitedNumberOfRootDevicesIsReturned) {
    platformsImpl->clear();
    constexpr auto numRootDevices = 3u;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;
    DebugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    DebugManager.flags.LimitAmountOfReturnedDevices.set(numRootDevices - 1);

    cl_uint numDevices = 0;
    cl_uint numEntries = numRootDevices;
    cl_device_id devices[numRootDevices];

    const auto dummyDevice = reinterpret_cast<cl_device_id>(0x1357);
    for (auto i = 0u; i < numRootDevices; i++) {
        devices[i] = dummyDevice;
    }

    auto retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_ALL, numEntries, devices, &numDevices);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(numEntries - 1, numDevices);
    for (auto i = 0u; i < numDevices; i++) {
        EXPECT_EQ(devices[i], platform()->getClDevice(i));
    }
    EXPECT_EQ(devices[numDevices], dummyDevice);
}
TEST(clGetDeviceIDsNegativeTests, whenFailToCreateDeviceThenclGetDeviceIDsReturnsNoDeviceError) {
    VariableBackup<decltype(DeviceFactory::createRootDeviceFunc)> createFuncBackup{&DeviceFactory::createRootDeviceFunc};
    DeviceFactory::createRootDeviceFunc = [](ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) -> std::unique_ptr<Device> {
        return nullptr;
    };
    platformsImpl->clear();

    constexpr auto numRootDevices = 3u;
    cl_uint numDevices = 0;
    cl_uint numEntries = numRootDevices;
    cl_device_id devices[numRootDevices];

    auto retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_ALL, numEntries, devices, &numDevices);
    EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);
    EXPECT_EQ(numDevices, 0u);
}
} // namespace ULT
