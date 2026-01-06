/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_io_functions.h"

#include "opencl/source/context/context.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "cl_api_tests.h"

using namespace NEO;

using ClGetPlatformIDsTests = ApiTests;

namespace ULT {
TEST_F(ClGetPlatformIDsTests, GivenNullPlatformWhenGettingPlatformIdsThenNumberofPlatformsIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_uint numPlatforms = 0;

    retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_GT(numPlatforms, 0u);
}

TEST_F(ClGetPlatformIDsTests, GivenPlatformsWhenGettingPlatformIdsThenPlatformsIdIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_platform_id platform = nullptr;

    retVal = clGetPlatformIDs(1, &platform, nullptr);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_NE(nullptr, platform);
}

TEST_F(ClGetPlatformIDsTests, GivenNumEntriesZeroAndPlatformNotNullWhenGettingPlatformIdsThenClInvalidValueErrorIsReturned) {
    cl_int retVal = CL_SUCCESS;
    cl_platform_id platform = nullptr;

    retVal = clGetPlatformIDs(0, &platform, nullptr);

    EXPECT_EQ(CL_INVALID_VALUE, retVal);
}

TEST(clGetPlatformIDsNegativeTests, GivenFailedInitializationWhenGettingPlatformIdsThenClOutOfHostMemoryErrorIsReturned) {
    platformsImpl->clear();
    VariableBackup<UltHwConfig> backup{&ultHwConfig};
    ultHwConfig.mockedPrepareDeviceEnvironmentsFuncResult = false;

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    EXPECT_EQ(0u, numPlatforms);
    EXPECT_EQ(nullptr, platformRet);

    platformsImpl->clear();
}

TEST(clGetPlatformIDsNegativeTests, whenFailToCreateDeviceThenClGetPlatfomsIdsReturnsOutOfHostMemoryError) {
    VariableBackup<decltype(DeviceFactory::createRootDeviceFunc)> createFuncBackup{&DeviceFactory::createRootDeviceFunc};
    DeviceFactory::createRootDeviceFunc = [](ExecutionEnvironment &executionEnvironment, uint32_t rootDeviceIndex) -> std::unique_ptr<Device> {
        return nullptr;
    };
    platformsImpl->clear();

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    EXPECT_EQ(0u, numPlatforms);
    EXPECT_EQ(nullptr, platformRet);

    platformsImpl->clear();
}

TEST(clGetPlatformIDsNegativeTests, whenFailToCreatePlatformThenClGetPlatfomsIdsReturnsOutOfHostMemoryError) {
    VariableBackup<decltype(Platform::createFunc)> createFuncBackup{&Platform::createFunc};
    Platform::createFunc = [](ExecutionEnvironment &executionEnvironment) -> std::unique_ptr<Platform> {
        return nullptr;
    };
    platformsImpl->clear();

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    EXPECT_EQ(0u, numPlatforms);
    EXPECT_EQ(nullptr, platformRet);

    platformsImpl->clear();
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
    platformsImpl->clear();

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_OUT_OF_HOST_MEMORY, retVal);
    EXPECT_EQ(0u, numPlatforms);
    EXPECT_EQ(nullptr, platformRet);

    platformsImpl->clear();
}

TEST(clGetPlatformIDsTest, givenOneApiPvcSendWarWaEnvWhenCreatingExecutionEnvironmentThenCorrectEnvValueIsStored) {
    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);

    {
        std::unordered_map<std::string, std::string> mockableEnvs = {{"ONEAPI_PVC_SEND_WAR_WA", "1"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        cl_int retVal = CL_SUCCESS;
        cl_platform_id platformRet = nullptr;
        cl_uint numPlatforms = 0;

        platformsImpl->clear();

        retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

        EXPECT_EQ(CL_SUCCESS, retVal);

        auto executionEnvironment = platform()->peekExecutionEnvironment();
        EXPECT_TRUE(executionEnvironment->isOneApiPvcWaEnv());

        platformsImpl->clear();
    }
    {
        std::unordered_map<std::string, std::string> mockableEnvs = {{"ONEAPI_PVC_SEND_WAR_WA", "0"}};
        VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

        cl_int retVal = CL_SUCCESS;
        cl_platform_id platformRet = nullptr;
        cl_uint numPlatforms = 0;

        platformsImpl->clear();

        retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

        EXPECT_EQ(CL_SUCCESS, retVal);

        auto executionEnvironment = platform()->peekExecutionEnvironment();
        EXPECT_FALSE(executionEnvironment->isOneApiPvcWaEnv());

        platformsImpl->clear();
    }
}

TEST(clGetPlatformIDsTest, givenEnabledExperimentalSupportAndEnabledProgramDebuggingWhenGettingPlatformIdsThenDebuggingEnabledIsSetInExecutionEnvironment) {
    DebugManagerStateRestore stateRestore;
    NEO::debugManager.flags.ExperimentalEnableL0DebuggerForOpenCL.set(1);

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    platformsImpl->clear();

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto executionEnvironment = platform()->peekExecutionEnvironment();
    EXPECT_TRUE(executionEnvironment->isDebuggingEnabled());

    platformsImpl->clear();
}

TEST(clGetPlatformIDsTest, givenEnabledExperimentalSupportAndEnableProgramDebuggingWithValue2WhenGettingPlatformIdsThenDebuggingEnabledIsSetInExecutionEnvironment) {
    DebugManagerStateRestore stateRestore;
    NEO::debugManager.flags.ExperimentalEnableL0DebuggerForOpenCL.set(1);

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "2"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    platformsImpl->clear();

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto executionEnvironment = platform()->peekExecutionEnvironment();
    EXPECT_TRUE(executionEnvironment->isDebuggingEnabled());

    platformsImpl->clear();
}

TEST(clGetPlatformIDsTest, givenNoExperimentalSupportAndEnabledProgramDebuggingWhenGettingPlatformIdsThenDebuggingEnabledIsNotSetInExecutionEnvironment) {
    DebugManagerStateRestore stateRestore;
    NEO::debugManager.flags.ExperimentalEnableL0DebuggerForOpenCL.set(0);

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    platformsImpl->clear();

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto executionEnvironment = platform()->peekExecutionEnvironment();
    EXPECT_FALSE(executionEnvironment->isDebuggingEnabled());

    platformsImpl->clear();
}

TEST(clGetPlatformIDsTest, givenNoExperimentalSupportAndEnableProgramDebuggingWithValue2WhenGettingPlatformIdsThenDebuggingEnabledIsNotSetInExecutionEnvironment) {
    DebugManagerStateRestore stateRestore;
    NEO::debugManager.flags.ExperimentalEnableL0DebuggerForOpenCL.set(0);

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "2"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    platformsImpl->clear();

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto executionEnvironment = platform()->peekExecutionEnvironment();
    EXPECT_FALSE(executionEnvironment->isDebuggingEnabled());

    platformsImpl->clear();
}

TEST(clGetPlatformIDsTest, givenEnabledExperimentalSupportAndZeroProgramDebuggingWhenGettingPlatformIdsThenDebuggingEnabledIsNotSetInExecutionEnvironment) {
    DebugManagerStateRestore stateRestore;
    NEO::debugManager.flags.ExperimentalEnableL0DebuggerForOpenCL.set(1);

    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"ZET_ENABLE_PROGRAM_DEBUGGING", "0"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    platformsImpl->clear();

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);

    auto executionEnvironment = platform()->peekExecutionEnvironment();
    EXPECT_FALSE(executionEnvironment->isDebuggingEnabled());

    platformsImpl->clear();
}

TEST(clGetPlatformIDsTest, givenEnabledFP64EmulationWhenGettingPlatformIdsThenFP64EmulationIsEnabled) {
    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"NEO_FP64_EMULATION", "1"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    platformsImpl->clear();

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);

    ASSERT_NE(nullptr, platformsImpl);
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    EXPECT_TRUE(executionEnvironment->isFP64EmulationEnabled());

    platformsImpl->clear();
}

TEST(clGetPlatformIDsTest, givenDefaultFP64EmulationStateWhenGettingPlatformIdsThenFP64EmulationIsDisabled) {
    VariableBackup<uint32_t> mockGetenvCalledBackup(&IoFunctions::mockGetenvCalled, 0);
    std::unordered_map<std::string, std::string> mockableEnvs = {{"NEO_FP64_EMULATION", "0"}};
    VariableBackup<std::unordered_map<std::string, std::string> *> mockableEnvValuesBackup(&IoFunctions::mockableEnvValues, &mockableEnvs);

    cl_int retVal = CL_SUCCESS;
    cl_platform_id platformRet = nullptr;
    cl_uint numPlatforms = 0;

    platformsImpl->clear();

    retVal = clGetPlatformIDs(1, &platformRet, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);

    ASSERT_NE(nullptr, platformsImpl);
    auto executionEnvironment = platform()->peekExecutionEnvironment();
    EXPECT_FALSE(executionEnvironment->isDebuggingEnabled());

    platformsImpl->clear();
}

TEST(clGetPlatformIDsTest, givenMultipleDifferentDevicesWhenGetPlatformIdsThenSeparatePlatformIsReturnedPerEachProductFamily) {
    if (!hardwareInfoSetup[IGFX_LUNARLAKE] ||
        !hardwareInfoSetup[IGFX_BMG] ||
        !hardwareInfoSetup[IGFX_PTL]) {
        GTEST_SKIP();
    }
    platformsImpl->clear();
    VariableBackup<UltHwConfig> backup(&ultHwConfig);
    const size_t numRootDevices = 5u;
    MockExecutionEnvironment executionEnvironment(defaultHwInfo.get(), true, numRootDevices);

    executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_LUNARLAKE;
    executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    executionEnvironment.rootDeviceEnvironments[1]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_BMG;
    executionEnvironment.rootDeviceEnvironments[1]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = false;
    executionEnvironment.rootDeviceEnvironments[2]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_LUNARLAKE;
    executionEnvironment.rootDeviceEnvironments[2]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    executionEnvironment.rootDeviceEnvironments[3]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_LUNARLAKE;
    executionEnvironment.rootDeviceEnvironments[3]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;
    executionEnvironment.rootDeviceEnvironments[4]->getMutableHardwareInfo()->platform.eProductFamily = IGFX_PTL;
    executionEnvironment.rootDeviceEnvironments[4]->getMutableHardwareInfo()->capabilityTable.isIntegratedDevice = true;

    ultHwConfig.sourceExecutionEnvironment = &executionEnvironment;

    uint32_t numPlatforms = 0;
    clGetPlatformIDs(0, nullptr, &numPlatforms);

    ASSERT_EQ(3u, numPlatforms);

    cl_platform_id platforms[3];
    clGetPlatformIDs(3, platforms, &numPlatforms);

    auto platform0 = static_cast<Platform *>(platforms[0]);
    auto platform1 = static_cast<Platform *>(platforms[1]);
    auto platform2 = static_cast<Platform *>(platforms[2]);

    EXPECT_EQ(1u, platform0->getNumDevices());
    EXPECT_EQ(IGFX_BMG, platform0->getClDevices()[0]->getHardwareInfo().platform.eProductFamily);

    EXPECT_EQ(1u, platform1->getNumDevices());
    EXPECT_EQ(IGFX_PTL, platform1->getClDevices()[0]->getHardwareInfo().platform.eProductFamily);

    EXPECT_EQ(3u, platform2->getNumDevices());
    EXPECT_EQ(IGFX_LUNARLAKE, platform2->getClDevices()[0]->getHardwareInfo().platform.eProductFamily);
    EXPECT_EQ(IGFX_LUNARLAKE, platform2->getClDevices()[1]->getHardwareInfo().platform.eProductFamily);
    EXPECT_EQ(IGFX_LUNARLAKE, platform2->getClDevices()[2]->getHardwareInfo().platform.eProductFamily);
}
} // namespace ULT
