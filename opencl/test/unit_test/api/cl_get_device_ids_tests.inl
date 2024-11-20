/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/device/device.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/test_macros/test.h"

#include "opencl/source/cl_device/cl_device.h"
#include "opencl/test/unit_test/fixtures/platform_fixture.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

using namespace NEO;

using clGetDeviceIDsTests = Test<PlatformFixture>;

namespace ULT {

TEST_F(clGetDeviceIDsTests, givenZeroNumEntriesWhenGettingDeviceIdsThenNumberOfDevicesIsGreaterThanZero) {
    cl_uint numDevices = 0;

    auto retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, 0, nullptr, &numDevices);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numDevices, (cl_uint)0);
}

TEST_F(clGetDeviceIDsTests, givenNonNullDevicesWhenGettingDeviceIdsThenDeviceIdIsReturned) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    auto retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetDeviceIDsTests, givenNullPlatformWhenGettingDeviceIdsThenDeviceIdIsReturned) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    auto retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_GPU, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_SUCCESS, retVal);
}

TEST_F(clGetDeviceIDsTests, givenInvalidDeviceTypeWhenGettingDeviceIdsThenInvalidDeviceTypeErrorIsReturned) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    auto retVal = clGetDeviceIDs(pPlatform, 0x0f00, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_INVALID_DEVICE_TYPE, retVal);
}

TEST_F(clGetDeviceIDsTests, givenZeroNumEntriesAndNonNullDevicesWhenGettingDeviceIdsThenInvalidValueErrorIsReturned) {
    cl_device_id pDevices[1];

    auto retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_GPU, 0, pDevices, nullptr);
    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST_F(clGetDeviceIDsTests, givenInvalidPlatformWhenGettingDeviceIdsThenInvalidPlatformErrorIsReturned) {
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];
    uint32_t trash[6] = {0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef, 0xdeadbeef};
    cl_platform_id p = reinterpret_cast<cl_platform_id>(trash);

    auto retVal = clGetDeviceIDs(p, CL_DEVICE_TYPE_GPU, numEntries, pDevices, nullptr);
    EXPECT_EQ(CL_INVALID_PLATFORM, retVal);
}

TEST_F(clGetDeviceIDsTests, givenDeviceTypeAllWhenGettingDeviceIdsThenDeviceIdIsReturned) {
    cl_uint numDevices = 0;
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    auto retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_ALL, numEntries, pDevices, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numDevices, (cl_uint)0);
}

TEST_F(clGetDeviceIDsTests, givenDeviceTypeDefaultWhenGettingDeviceIdsThenDeviceIdIsReturned) {
    cl_uint numDevices = 0;
    cl_uint numEntries = 1;
    cl_device_id pDevices[1];

    auto retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_DEFAULT, numEntries, pDevices, &numDevices);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numDevices, (cl_uint)0);
}

TEST_F(clGetDeviceIDsTests, givenDeviceTypeCpuWhenGettingDeviceIdsThenDeviceNotFoundErrorIsReturned) {
    cl_uint numDevices = 0;

    auto retVal = clGetDeviceIDs(pPlatform, CL_DEVICE_TYPE_CPU, 0, nullptr, &numDevices);

    EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);
    EXPECT_EQ(numDevices, (cl_uint)0);
}

TEST(clGetDeviceIDsTest, givenMultipleRootDevicesWhenGetDeviceIdsThenAllRootDevicesAreReturned) {
    constexpr auto numRootDevices = 3u;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    cl_uint numDevices = 0;
    cl_uint numEntries = numRootDevices;
    cl_device_id devices[numRootDevices];

    std::string hierarchies[] = {"COMPOSITE", "FLAT", "COMBINED"};
    for (std::string hierarchy : hierarchies) {
        platformsImpl->clear();
        std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", hierarchy}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        auto retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_ALL, numEntries, devices, &numDevices);
        EXPECT_EQ(retVal, CL_SUCCESS);
        EXPECT_EQ(numEntries, numDevices);
        for (auto i = 0u; i < numRootDevices; i++) {
            EXPECT_EQ(devices[i], platform()->getClDevice(i));
        }
    }
}

TEST(clGetDeviceIDsTest, givenMultipleRootDevicesWhenGetDeviceIdsButNumEntriesIsLowerThanNumDevicesThenSubsetOfRootDevicesIsReturned) {
    constexpr auto numRootDevices = 3u;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    cl_uint maxNumDevices;

    std::string hierarchies[] = {"COMPOSITE", "FLAT", "COMBINED"};
    for (std::string hierarchy : hierarchies) {
        platformsImpl->clear();
        std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", hierarchy}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

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
}

TEST(clGetDeviceIDsTest, givenMultipleRootAndSubDevicesWhenCallClGetDeviceIDsThenSubDevicesAreReturnedAsSeparateClDevices) {
    constexpr auto numRootDevices = 3u;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.CreateMultipleSubDevices.set(numRootDevices);
    cl_uint maxNumDevices;

    std::string hierarchies[] = {"FLAT", "COMBINED"};
    for (std::string hierarchy : hierarchies) {
        platformsImpl->clear();
        std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", hierarchy}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

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
}

TEST(clGetDeviceIDsTest, givenCompositeHierarchyWithMultipleRootAndSubDevicesWhenCallClGetDeviceIDsThenSubDevicesAreNotReturnedAsSeparateClDevices) {
    constexpr auto numRootDevices = 3u;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.CreateMultipleSubDevices.set(numRootDevices);
    cl_uint maxNumDevices;

    platformsImpl->clear();
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", "COMPOSITE"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    auto retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_ALL, 0, nullptr, &maxNumDevices);
    EXPECT_EQ(retVal, CL_SUCCESS);
    EXPECT_EQ(numRootDevices, maxNumDevices);

    cl_uint numDevices = 0;
    cl_uint numEntries = maxNumDevices - 1;
    cl_device_id devices[numRootDevices];

    const auto dummyDevice = reinterpret_cast<cl_device_id>(0x1357);
    for (auto i = 0u; i < maxNumDevices; i++) {
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

TEST(clGetDeviceIDsTest, givenMultipleRootDevicesAndLimitedNumberOfReturnedDevicesWhenGetDeviceIdsThenLimitedNumberOfRootDevicesIsReturned) {
    constexpr auto numRootDevices = 3u;
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    ultHwConfig.useMockedPrepareDeviceEnvironmentsFunc = false;
    DebugManagerStateRestore restorer;
    debugManager.flags.CreateMultipleRootDevices.set(numRootDevices);
    debugManager.flags.LimitAmountOfReturnedDevices.set(numRootDevices - 1);

    std::string hierarchies[] = {"COMPOSITE", "FLAT", "COMBINED"};
    for (std::string hierarchy : hierarchies) {
        platformsImpl->clear();
        std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", hierarchy}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

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
}

TEST(clGetDeviceIDsNegativeTests, whenFailToCreateDeviceThenclGetDeviceIDsReturnsNoDeviceError) {
    VariableBackup<decltype(DeviceFactory::createRootDeviceFunc)> createFuncBackup{&DeviceFactory::createRootDeviceFunc};
    DeviceFactory::createRootDeviceFunc = [](ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) -> std::unique_ptr<Device> {
        return nullptr;
    };

    std::string hierarchies[] = {"COMPOSITE", "FLAT", "COMBINED"};
    for (std::string hierarchy : hierarchies) {
        platformsImpl->clear();
        std::unordered_map<std::string, std::string> mockableEnvs = {{"ZE_FLAT_DEVICE_HIERARCHY", hierarchy}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        constexpr auto numRootDevices = 3u;
        cl_uint numDevices = 0;
        cl_uint numEntries = numRootDevices;
        cl_device_id devices[numRootDevices];

        auto retVal = clGetDeviceIDs(nullptr, CL_DEVICE_TYPE_ALL, numEntries, devices, &numDevices);
        EXPECT_EQ(CL_DEVICE_NOT_FOUND, retVal);
        EXPECT_EQ(numDevices, 0u);
    }
}

} // namespace ULT
