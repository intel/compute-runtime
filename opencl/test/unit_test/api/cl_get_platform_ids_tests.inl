/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/helpers/ult_hw_config.h"

#include "opencl/source/context/context.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/helpers/variable_backup.h"

#include "cl_api_tests.h"

using namespace NEO;

typedef api_tests clGetPlatformIDsTests;

namespace ULT {
TEST_F(clGetPlatformIDsTests, GivenNullPlatformWhenGettingPlatformIdsThenNumberofPlatformsIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_uint numPlatforms = 0;

    retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numPlatforms, 0u);
}

TEST_F(clGetPlatformIDsTests, GivenPlatformsWhenGettingPlatformIdsThenPlatformsIdIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_platform_id platform = nullptr;

    retVal = clGetPlatformIDs(1, &platform, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, platform);
}

TEST_F(clGetPlatformIDsTests, GivenNumEntriesZeroAndPlatformNotNullWhenGettingPlatformIdsThenClInvalidValueErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_platform_id platform = nullptr;

    retVal = clGetPlatformIDs(0, &platform, nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(clGetPlatformIDsNegativeTests, GivenFailedInitializationWhenGettingPlatformIdsThenClOutOfHostMemoryErrorIsReturned) {
    platformsImpl.clear();
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.mockedGetDevicesFuncResult = false;

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    EXPECT_EQ(0u, numPlatforms);
    EXPECT_EQ(nullptr, platformRet);

    platformsImpl.clear();
}
TEST(clGetPlatformIDsNegativeTests, whenFailToCreateDeviceThenClGetPlatfomsIdsReturnsOutOfHostMemoryError) {
    VariableBackup<decltype(DeviceFactory::createRootDeviceFunc)> createFuncBackup{&DeviceFactory::createRootDeviceFunc};
    DeviceFactory::createRootDeviceFunc = [](ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) -> std::unique_ptr<Device> {
        return nullptr;
    };
    platformsImpl.clear();

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    EXPECT_EQ(0u, numPlatforms);
    EXPECT_EQ(nullptr, platformRet);

    platformsImpl.clear();
}
TEST(clGetPlatformIDsNegativeTests, whenFailToCreatePlatformThenClGetPlatfomsIdsReturnsOutOfHostMemoryError) {
    VariableBackup<decltype(Platform::createFunc)> createFuncBackup{&Platform::createFunc};
    Platform::createFunc = [](ExecutionEnvironment &executionEnvironment) -> std::unique_ptr<Platform> {
        return nullptr;
    };
    platformsImpl.clear();

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    EXPECT_EQ(0u, numPlatforms);
    EXPECT_EQ(nullptr, platformRet);

    platformsImpl.clear();
}

TEST(clGetPlatformIDsNegativeTests, whenFailToInitializePlatformThenClGetPlatfomsIdsReturnsOutOfHostMemoryError) {
    VariableBackup<decltype(Platform::createFunc)> createFuncBackup{&Platform::createFunc};
    struct FailingPlatform : public Platform {
        using Platform::Platform;
        bool initialize(std::vector<std::unique_ptr<Device>> devices) override {
            return false;
        }
    };
    Platform::createFunc = [](ExecutionEnvironment &executionEnvironment) -> std::unique_ptr<Platform> {
        return std::make_unique<FailingPlatform>(executionEnvironment);
    };
    platformsImpl.clear();

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    EXPECT_EQ(0u, numPlatforms);
    EXPECT_EQ(nullptr, platformRet);

    platformsImpl.clear();
}
} // namespace ULT
