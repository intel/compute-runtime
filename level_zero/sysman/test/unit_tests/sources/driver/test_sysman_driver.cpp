/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"

#if defined(_WIN32)
#include "level_zero/sysman/test/unit_tests/sources/windows/mock_sysman_fixture.h"
#else
#include "level_zero/sysman/test/unit_tests/sources/linux/mock_sysman_fixture.h"
#endif

#include "level_zero/sysman/source/driver/sysman_driver.h"
#include "level_zero/sysman/test/unit_tests/sources/driver/mock_sysman_driver.h"
#include "level_zero/sysman/test/unit_tests/sources/driver/mock_sysman_fixture.h"
#include "level_zero/zes_intel_gpu_sysman.h"

#include "gtest/gtest.h"

#include <cstring>

namespace L0 {
namespace Sysman {
namespace ult {

using SysmanDriverHandleOsAgnosticTest = SysmanDeviceFixture;

TEST_F(SysmanDriverHandleOsAgnosticTest,
       GivenExtensionNameLongerOrEqualToMaximumCharsWhenCallingGetExtensionPropertiesThenNameIsTruncatedToMaximumCharsWithNullTermination) {
    std::string extensionNameLonger(ZES_MAX_EXTENSION_NAME, 'X');
    std::vector<std::pair<std::string, uint32_t>> extensionsSupported = {
        {extensionNameLonger, 100}};

    auto sysmanDriverHandle = static_cast<SysmanDriverHandleImp *>(driverHandle.get());

    uint32_t pCount = 0;
    EXPECT_EQ(ZE_RESULT_SUCCESS, sysmanDriverHandle->getExtensionProperties(&pCount, nullptr, extensionsSupported));
    EXPECT_EQ(1u, pCount);

    std::vector<zes_driver_extension_properties_t> extensionsReturned(pCount);
    EXPECT_EQ(ZE_RESULT_SUCCESS, sysmanDriverHandle->getExtensionProperties(&pCount, extensionsReturned.data(), extensionsSupported));

    // Verify that the extension name is truncated properly
    EXPECT_EQ(static_cast<size_t>(ZES_MAX_EXTENSION_NAME - 1), strlen(extensionsReturned[0].name));
    EXPECT_EQ('\0', extensionsReturned[0].name[ZES_MAX_EXTENSION_NAME - 1]);
    EXPECT_EQ(100u, extensionsReturned[0].version);
    std::string expectedName(ZES_MAX_EXTENSION_NAME - 1, 'X');
    EXPECT_EQ(expectedName, std::string(extensionsReturned[0].name));
}

TEST(SysmanDriverDeferredDiscoveryOsAgnostic,
     GivenNoFlagWhenNoDevicesPresentThenInitializeFails) {
    OsAgnosticMockSysmanDriver driver;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
    ze_result_t result = ZE_RESULT_SUCCESS;

    driver.initialize(&result, 0);

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
    EXPECT_EQ(nullptr, L0::Sysman::globalSysmanDriverHandle);

    if (L0::Sysman::globalSysmanDriver != nullptr) {
        delete L0::Sysman::globalSysmanDriver;
    }
    L0::Sysman::globalSysmanDriver = nullptr;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
    L0::Sysman::driverCount = 0;
}

TEST(SysmanDriverDeferredDiscoveryOsAgnostic,
     GivenEXP_NO_GPUSFlagWhenNoDevicesPresentThenInitializeSucceedsInDeferredMode) {
    OsAgnosticMockSysmanDriver driver;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;

    driver.initialize(&result, ZES_INTEL_INIT_FLAG_EXP_NO_GPUS);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, L0::Sysman::globalSysmanDriverHandle);
    EXPECT_EQ(1u, L0::Sysman::driverCount);

    auto *handleImp = static_cast<L0::Sysman::SysmanDriverHandleImp *>(L0::Sysman::globalSysmanDriver);
    ASSERT_NE(nullptr, handleImp);
    EXPECT_TRUE(handleImp->isDeferredDiscoveryMode());

    delete L0::Sysman::globalSysmanDriver;
    L0::Sysman::globalSysmanDriver = nullptr;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
    L0::Sysman::driverCount = 0;
}

TEST(SysmanDriverDeferredDiscoveryOsAgnostic,
     GivenDeferredModeWhenGetDeviceTriggersDiscoveryAndNoDevicesFoundThenReturnsZero) {
    OsAgnosticMockSysmanDriver driver;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;

    driver.initialize(&result, ZES_INTEL_INIT_FLAG_EXP_NO_GPUS);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    // globalSysmanDriver is a MockSysmanDriverHandleImp created by createDeferredHandle()
    auto *handleImp = static_cast<MockSysmanDriverHandleImp *>(L0::Sysman::globalSysmanDriver);
    ASSERT_NE(nullptr, handleImp);

    // Count-query call (pCount=0, phDevices=nullptr): triggers deferred discovery,
    // finds 0 devices, returns ZE_RESULT_SUCCESS with pCount set to 0.
    uint32_t deviceCount = 0u;
    ze_result_t deviceResult = handleImp->getDevice(&deviceCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceResult);
    EXPECT_EQ(0u, deviceCount);
    EXPECT_EQ(1u, handleImp->deferredDiscoveryCallCount);
    EXPECT_TRUE(handleImp->areDevicesDiscovered());

    delete L0::Sysman::globalSysmanDriver;
    L0::Sysman::globalSysmanDriver = nullptr;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
    L0::Sysman::driverCount = 0;
}

TEST(SysmanDriverDeferredDiscoveryOsAgnostic,
     GivenDeferredModeWhenGetDeviceTriggersDiscoveryAndDevicesFoundThenReturnsCount) {
    MockSysmanDriverWithDevices driver;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;

    driver.initialize(&result, ZES_INTEL_INIT_FLAG_EXP_NO_GPUS);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto *handleImp = static_cast<MockSysmanDriverHandleImp *>(L0::Sysman::globalSysmanDriver);
    ASSERT_NE(nullptr, handleImp);

    uint32_t deviceCount = 0u;
    ze_result_t deviceResult = handleImp->getDevice(&deviceCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, deviceResult);
    EXPECT_EQ(2u, deviceCount);
    EXPECT_EQ(1u, handleImp->deferredDiscoveryCallCount);

    delete L0::Sysman::globalSysmanDriver;
    L0::Sysman::globalSysmanDriver = nullptr;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
    L0::Sysman::driverCount = 0;
}

TEST(SysmanDriverDeferredDiscoveryOsAgnostic,
     GivenDeferredModeWhenGetDeviceCalledMultipleTimesThenDiscoveryRunsOnlyOnce) {
    OsAgnosticMockSysmanDriver driver;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;

    driver.initialize(&result, ZES_INTEL_INIT_FLAG_EXP_NO_GPUS);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto *handleImp = static_cast<MockSysmanDriverHandleImp *>(L0::Sysman::globalSysmanDriver);
    ASSERT_NE(nullptr, handleImp);

    uint32_t deviceCount = 0u;
    handleImp->getDevice(&deviceCount, nullptr);
    handleImp->getDevice(&deviceCount, nullptr);
    handleImp->getDevice(&deviceCount, nullptr);

    EXPECT_EQ(1u, handleImp->deferredDiscoveryCallCount);

    delete L0::Sysman::globalSysmanDriver;
    L0::Sysman::globalSysmanDriver = nullptr;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
    L0::Sysman::driverCount = 0;
}

TEST(SysmanDriverDeferredDiscoveryOsAgnostic,
     GivenGPUOnlyFlagWhenInitCalledThenDriverIsInitialized) {
    OsAgnosticMockSysmanDriver driver;

    ze_result_t result = L0::Sysman::init(static_cast<zes_init_flags_t>(ZE_INIT_FLAG_GPU_ONLY));

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, driver.initCalledCount);
}

TEST(SysmanDriverDeferredDiscoveryOsAgnostic,
     GivenInitializeFailsWhenDriverInitCalledThenReturnsError) {
    OsAgnosticMockSysmanDriver driver;
    driver.useBaseDriverInit = true;
    driver.useBaseInit = false;
    driver.sysmanInitFail = true;

    ze_result_t result = driver.driverInit(0);

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
    EXPECT_EQ(1u, driver.initCalledCount);
}

TEST(SysmanDriverDeferredDiscoveryOsAgnostic,
     GivenCreateDeferredHandleWhenCalledDirectlyThenHandleIsInDeferredMode) {
    SysmanDriverImpWithRealDeferred driver;
    L0::Sysman::globalSysmanDriver = nullptr;
    auto *env = new NEO::MockExecutionEnvironment();
    env->incRefInternal(); // deferred handle takes this ref; destructor calls decRefInternal
    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;

    SysmanDriverHandle *handle = driver.createDeferredHandle(*env, &result);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, handle);
    auto *handleImp = static_cast<SysmanDriverHandleImp *>(handle);
    EXPECT_TRUE(handleImp->isDeferredDiscoveryMode());
    EXPECT_FALSE(handleImp->areDevicesDiscovered());

    delete L0::Sysman::globalSysmanDriver; // destructor releases env via decRefInternal
    L0::Sysman::globalSysmanDriver = nullptr;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
}

TEST(SysmanDriverDeferredDiscoveryOsAgnostic,
     GivenUnrecognisedFlagWhenInitCalledThenReturnsUninitialisedWithoutCallingDriverInit) {
    OsAgnosticMockSysmanDriver driver;

    // 0x0002 is not ZE_INIT_FLAG_GPU_ONLY (0x1) and not ZES_INTEL_INIT_FLAG_EXP_NO_GPUS (bit 16)
    ze_result_t result = L0::Sysman::init(static_cast<zes_init_flags_t>(0x0002));

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, result);
    // driverInit() must NOT have been called the gate returned before reaching it
    EXPECT_EQ(0u, driver.initCalledCount);
}

TEST(SysmanDriverDeferredDiscoveryOsAgnostic,
     GivenDeferredDiscoveryFailsWhenGetDeviceCalledThenErrorPropagatedAndPCountSetToZero) {
    MockSysmanDriverWithFailingDiscovery driver;
    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
    driver.initialize(&result, ZES_INTEL_INIT_FLAG_EXP_NO_GPUS);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto *handleImp = static_cast<MockSysmanDriverHandleImp *>(L0::Sysman::globalSysmanDriver);
    ASSERT_NE(nullptr, handleImp);

    // Pass non-zero pCount so getDevice() enters the deferred path and not the count-query path
    uint32_t deviceCount = 1u;
    ze_result_t deviceResult = handleImp->getDevice(&deviceCount, nullptr);

    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, deviceResult);
    EXPECT_EQ(0u, deviceCount); // getDevice() must zero pCount when returning an error
    EXPECT_EQ(1u, handleImp->deferredDiscoveryCallCount);

    delete L0::Sysman::globalSysmanDriver;
    L0::Sysman::globalSysmanDriver = nullptr;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
    L0::Sysman::driverCount = 0;
}

TEST(SysmanDriverDeferredDiscoveryOsAgnostic,
     GivenSurvivabilityHandleWhenInitCalledWithEXPFlagThenDeferredModeNotEntered) {
    OsAgnosticMockSysmanDriver driver;
    L0::Sysman::globalSysmanDriverHandle = nullptr;

    // Provide a survivability handle via MockOsDriver
    auto *survivabilityHandle = new MockSysmanDriverHandleImp();
    driver.pMockOsDriver->survivabilityHandle = survivabilityHandle;
    driver.pMockOsDriver->survivabilityDriverCount = 1u;
    driver.pMockOsDriver->survivabilityResult = ZE_RESULT_SUCCESS;

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    driver.initialize(&result, ZES_INTEL_INIT_FLAG_EXP_NO_GPUS);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(nullptr, L0::Sysman::globalSysmanDriverHandle);
    EXPECT_EQ(1u, L0::Sysman::driverCount);
    // Deferred branch must NOT have been taken isDeferredDiscoveryMode() must be false.
    // The survivability path sets globalSysmanDriverHandle but NOT globalSysmanDriver;
    // use survivabilityHandle (which we own) directly.
    EXPECT_FALSE(survivabilityHandle->isDeferredDiscoveryMode());

    delete survivabilityHandle;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
    L0::Sysman::driverCount = 0;
}

TEST_F(SysmanDriverFixture,
       GivenNonDeferredHandleWhenGetDeviceCalledThenDeferredBlockNotEnteredAndCountReturned) {
    // Fixture creates MockSysmanDriverHandleImp with deferredDiscoveryMode=false (default)
    ASSERT_FALSE(driverHandle->isDeferredDiscoveryMode());

    uint32_t deviceCount = 0u;
    ze_result_t result = driverHandle->getDevice(&deviceCount, nullptr);

    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(0u, deviceCount);
    // Discovery must never have been triggered
    EXPECT_EQ(0u, driverHandle->deferredDiscoveryCallCount);
}

TEST(SysmanDriverDeferredDiscoveryOsAgnostic,
     GivenDeferredHandleDeletedBeforeGetDeviceCalledThenSavedEnvReleasedInDestructor) {
    OsAgnosticMockSysmanDriver driver;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    driver.initialize(&result, ZES_INTEL_INIT_FLAG_EXP_NO_GPUS);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);

    auto *handleImp = static_cast<MockSysmanDriverHandleImp *>(L0::Sysman::globalSysmanDriver);
    ASSERT_NE(nullptr, handleImp);
    ASSERT_NE(nullptr, handleImp->savedExecutionEnvironment);

    // Record the execution environment and its refcount before deletion.
    // The destructor must call decRefInternal() because getDevice() was never called.
    NEO::ExecutionEnvironment *env = handleImp->savedExecutionEnvironment;
    int32_t refBefore = env->getRefInternalCount();
    env->incRefInternal(); // hold an extra ref so we can query it after the handle is deleted

    // Delete the handle without ever calling getDevice() this is the destructor cleanup path
    delete L0::Sysman::globalSysmanDriver;

    // The destructor must have called decRefInternal() on savedExecutionEnvironment.
    // We added one extra ref before deleting, so the net count after deletion is:
    //   (refBefore + 1) - 1 == refBefore
    EXPECT_EQ(refBefore, env->getRefInternalCount());

    env->decRefInternal(); // release our extra hold
    L0::Sysman::globalSysmanDriver = nullptr;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
    L0::Sysman::driverCount = 0;
}

TEST(SysmanDriverDeferredDiscoveryOsAgnostic,
     GivenEmptyHwDeviceIdsWhenDiscoverAndInitializeCalledThenReturnsZero) {
    NEO::ExecutionEnvironment executionEnvironment;
    SysmanDriverImp::HwDeviceIds emptyIds;

    uint32_t count = SysmanDriverImp::discoverAndInitializeDevices(executionEnvironment, emptyIds, "error");

    EXPECT_EQ(0u, count);
}

TEST(SysmanDriverDeferredDiscoveryOsAgnostic,
     GivenRealDeferredHandleWhenGetDeviceCalledThenRealDiscoverHwDevicesEntered) {
    SysmanDriverImpWithRealDeferred driver;
    L0::Sysman::globalSysmanDriver = nullptr;
    auto *env = new NEO::MockExecutionEnvironment();
    env->incRefInternal();
    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;

    SysmanDriverHandle *handle = driver.createDeferredHandle(*env, &result);
    ASSERT_EQ(ZE_RESULT_SUCCESS, result);
    ASSERT_NE(nullptr, handle);

    uint32_t deviceCount = 0u;
    static_cast<SysmanDriverHandleImp *>(handle)->getDevice(&deviceCount, nullptr);

    delete L0::Sysman::globalSysmanDriver;
    L0::Sysman::globalSysmanDriver = nullptr;
    L0::Sysman::globalSysmanDriverHandle = nullptr;
}

} // namespace ult
} // namespace Sysman
} // namespace L0
