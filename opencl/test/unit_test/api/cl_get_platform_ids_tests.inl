/*
 * Copyright (C) 2018-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/os_library.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/helpers/ult_hw_config.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/mock_os_library.h"
#include "shared/test/common/mocks/mock_product_helper.h"

#include "opencl/source/api/leo_forwarding.h"
#include "opencl/source/context/context.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/mocks/mock_platform.h"

#include "cl_api_tests.h"

#include <cstring>

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

TEST(clGetPlatformIDsTest, givenEnabledExperimentalSupportAndEnabledProgramDebuggingWhenGettingPlatformIdsThenDebuggingEnabledIsSetInExecutionEnvironment) {
    DebugManagerStateRestore stateRestore;
    NEO::debugManager.flags.ExperimentalEnableL0DebuggerForOpenCL.set(1);
    NEO::debugManager.flags.ZET_ENABLE_PROGRAM_DEBUGGING.set(1);

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
    NEO::debugManager.flags.ZET_ENABLE_PROGRAM_DEBUGGING.set(2);

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
    NEO::debugManager.flags.ZET_ENABLE_PROGRAM_DEBUGGING.set(1);

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
    NEO::debugManager.flags.ZET_ENABLE_PROGRAM_DEBUGGING.set(2);

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
    NEO::debugManager.flags.ZET_ENABLE_PROGRAM_DEBUGGING.set(0);

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

TEST(clGetPlatformIDsTest, givenDefaultFP64EmulationStateWhenGettingPlatformIdsThenFP64EmulationIsDisabled) {
    DebugManagerStateRestore stateRestore;
    NEO::debugManager.flags.NEO_FP64_EMULATION.set(false);

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

static uint64_t fakeLeoPlatformStorage[2][64] = {};
static cl_platform_id fakeLeoPlatform0 = reinterpret_cast<cl_platform_id>(fakeLeoPlatformStorage[0]);
static cl_platform_id fakeLeoPlatform1 = reinterpret_cast<cl_platform_id>(fakeLeoPlatformStorage[1]);

static cl_uint mockLeoPlatformCount = 1u;

static cl_int CL_API_CALL mockLeoClGetPlatformIDs(cl_uint numEntries, cl_platform_id *platforms, cl_uint *numPlatforms) {
    if (numPlatforms) {
        *numPlatforms = mockLeoPlatformCount;
    }
    if (platforms) {
        if (numEntries > 0) {
            platforms[0] = fakeLeoPlatform0;
        }
        if (numEntries > 1) {
            platforms[1] = fakeLeoPlatform1;
        }
    }
    return CL_SUCCESS;
}

static void *CL_API_CALL mockLeoClGetExtensionFunctionAddress(const char *funcName) {
    if (0 == strcmp(funcName, "clIcdGetPlatformIDsKHR")) {
        return reinterpret_cast<void *>(mockLeoClGetPlatformIDs);
    }
    return nullptr;
}

struct MockProductHelperLeoSupported : MockProductHelper {
    MockProductHelperLeoSupported() {
        isLEOSupportedResult = true;
    }
};

struct ClGetPlatformIDsLeoTest : public ::testing::Test {
    void SetUp() override {
        mockLeoPlatformCount = 1u;
        ultHwConfig.leoForwardingSelfLoad = false;
        resetPlatformLists();
        leoTeardown();
        leoSetup();

        mockLibrary = new MockOsLibraryCustom(nullptr, true);
        mockLibrary->procMap["clGetExtensionFunctionAddress"] = reinterpret_cast<void *>(mockLeoClGetExtensionFunctionAddress);
        savedLoadFunc = OsLibrary::loadFunc;
        MockOsLibrary::loadLibraryNewObject = mockLibrary;
        OsLibrary::loadFunc = MockOsLibrary::load;
    }

    void TearDown() override {
        OsLibrary::loadFunc = savedLoadFunc;
        delete MockOsLibrary::loadLibraryNewObject;
        MockOsLibrary::loadLibraryNewObject = nullptr;
        resetPlatformLists();
        leoTeardown();
    }

    static void resetPlatformLists() {
        platformsImpl->clear();
        if (leoPlatformEntries != nullptr) {
            std::vector<LeoPlatformEntry>{}.swap(*leoPlatformEntries);
        }
    }

    MockOsLibraryCustom *mockLibrary = nullptr;
    decltype(OsLibrary::loadFunc) savedLoadFunc = nullptr;
    DebugManagerStateRestore restorer;
    VariableBackup<UltHwConfig> ultHwConfigBackup{&ultHwConfig};
};

TEST_F(ClGetPlatformIDsLeoTest, givenAutoEnableLeoWhenProductSupportsLeoThenPlatformComesFromLevelZeroAndNoNativePlatformIsBuilt) {
    debugManager.flags.EnableLEO.set(-1);
    ultHwConfig.leoDetectionEnabled = true;

    MockExecutionEnvironment mockExecutionEnvironment(defaultHwInfo.get());
    RAIIProductHelperFactory<MockProductHelperLeoSupported> raiiProductHelper{*mockExecutionEnvironment.rootDeviceEnvironments[0]};

    cl_platform_id platform = nullptr;
    cl_uint numPlatforms = 0u;
    auto retVal = clGetPlatformIDs(1, &platform, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numPlatforms);
    EXPECT_EQ(fakeLeoPlatform0, platform);
    EXPECT_TRUE(platformsImpl->empty());
    EXPECT_TRUE(areAllPlatformsLeo());
    EXPECT_TRUE(isLeoPlatformHandle(platform));
}

TEST_F(ClGetPlatformIDsLeoTest, givenLeoForcedOnThenAllPlatformsComeFromLevelZeroWithoutNativeInit) {
    debugManager.flags.EnableLEO.set(1);

    cl_platform_id platform = nullptr;
    cl_uint numPlatforms = 0u;
    auto retVal = clGetPlatformIDs(1, &platform, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(1u, numPlatforms);
    EXPECT_EQ(fakeLeoPlatform0, platform);
    EXPECT_TRUE(platformsImpl->empty());
}

TEST_F(ClGetPlatformIDsLeoTest, givenLeoForcedOffThenNoLevelZeroPlatformIsReported) {
    debugManager.flags.EnableLEO.set(0);
    ultHwConfig.leoDetectionEnabled = true;

    MockExecutionEnvironment mockExecutionEnvironment(defaultHwInfo.get());
    RAIIProductHelperFactory<MockProductHelperLeoSupported> raiiProductHelper{*mockExecutionEnvironment.rootDeviceEnvironments[0]};

    cl_uint numPlatforms = 0u;
    auto retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_FALSE(platformsImpl->empty());
    EXPECT_FALSE(areAllPlatformsLeo());
}

TEST_F(ClGetPlatformIDsLeoTest, givenTwoLevelZeroPlatformsAndANativeOneThenAllThreeAreMergedInDeviceGroupOrder) {
    debugManager.flags.EnableLEO.set(0);
    cl_uint numNativePlatforms = 0u;
    ASSERT_EQ(CL_SUCCESS, clGetPlatformIDs(0, nullptr, &numNativePlatforms));
    ASSERT_EQ(1u, numNativePlatforms);
    auto nativePlatform = static_cast<cl_platform_id>((*platformsImpl)[0].get());

    leoPlatformEntries->push_back({fakeLeoPlatform0, {IGFX_MAX_PRODUCT, false}, true});
    leoPlatformEntries->push_back({fakeLeoPlatform1, {IGFX_UNKNOWN, true}, true});

    cl_platform_id platforms[3] = {};
    cl_uint numPlatforms = 0u;
    auto retVal = clGetPlatformIDs(3, platforms, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);
    ASSERT_EQ(3u, numPlatforms);
    EXPECT_EQ(fakeLeoPlatform0, platforms[0]);
    EXPECT_EQ(nativePlatform, platforms[1]);
    EXPECT_EQ(fakeLeoPlatform1, platforms[2]);
}

TEST_F(ClGetPlatformIDsLeoTest, givenMoreLevelZeroPlatformsThanDetectedLeoProductsThenOrderingDegradesButNoPlatformIsLost) {
    debugManager.flags.EnableLEO.set(-1);
    ultHwConfig.leoDetectionEnabled = true;
    mockLeoPlatformCount = 2u;

    MockExecutionEnvironment mockExecutionEnvironment(defaultHwInfo.get());
    RAIIProductHelperFactory<MockProductHelperLeoSupported> raiiProductHelper{*mockExecutionEnvironment.rootDeviceEnvironments[0]};

    cl_platform_id platforms[2] = {};
    cl_uint numPlatforms = 0u;
    auto retVal = clGetPlatformIDs(2, platforms, &numPlatforms);

    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(2u, numPlatforms);
    EXPECT_EQ(fakeLeoPlatform0, platforms[0]);
    EXPECT_EQ(fakeLeoPlatform1, platforms[1]);
}
} // namespace ULT
