/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/os_library.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_os_library.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/api/opencl/source/gtpin/leo_gtpin_notify.h"
#include "level_zero/api/opencl/source/platform/leo_platform.h"
#include "level_zero/api/opencl/test/common/fixtures/ocl_fixture.h"
#include "level_zero/core/test/unit_tests/mocks/mock_driver.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace NEO {
namespace LEO {
namespace ult {

struct LeoGtpinInitTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();

        uint32_t (*openPinHandler)(void *) = [](void *arg) -> uint32_t {
            gtpinInitTimesCalled++;
            return 0;
        };

        MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, false);
        auto osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
        osLibrary->procMap["OpenGTPinOCL"] = reinterpret_cast<void *>(openPinHandler);
    }

    void TearDown() override {
        delete MockOsLibrary::loadLibraryNewObject;
        MockOsLibrary::loadLibraryNewObject = nullptr;
        Test<OclFixture>::TearDown();
    }

    static uint32_t gtpinInitTimesCalled;

    DebugManagerStateRestore restorer;
    VariableBackup<uint32_t> gtpinCounterBackup{&gtpinInitTimesCalled, 0};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};
    VariableBackup<NEO::OsLibrary *> osLibraryBackup{&MockOsLibrary::loadLibraryNewObject, nullptr};
};
uint32_t LeoGtpinInitTest::gtpinInitTimesCalled = 0u;

TEST_F(LeoGtpinInitTest, givenInstrumentationEnabledWhenTryNotifyGtpinInitThenOpensOclGtpinFrontend) {
    debugManager.flags.ZET_ENABLE_PROGRAM_INSTRUMENTATION.set(true);

    platform->tryNotifyGtpinInit();

    EXPECT_EQ(1u, gtpinInitTimesCalled);
}

TEST_F(LeoGtpinInitTest, givenInstrumentationDisabledWhenTryNotifyGtpinInitThenDoesNotOpenGtpin) {
    platform->tryNotifyGtpinInit();

    EXPECT_EQ(0u, gtpinInitTimesCalled);
}

TEST_F(LeoGtpinInitTest, givenInstrumentationEnabledWhenTryNotifyGtpinInitCalledMultipleTimesThenGtpinIsInitializedOnlyOnce) {
    debugManager.flags.ZET_ENABLE_PROGRAM_INSTRUMENTATION.set(true);

    platform->tryNotifyGtpinInit();
    platform->tryNotifyGtpinInit();

    EXPECT_EQ(1u, gtpinInitTimesCalled);
}

TEST_F(LeoGtpinInitTest, givenPopulatedPlatformsWhenGtPinTryNotifyInitThenDelegatesToFirstPlatform) {
    debugManager.flags.ZET_ENABLE_PROGRAM_INSTRUMENTATION.set(true);

    std::vector<std::unique_ptr<Platform>> platforms;
    platforms.push_back(std::make_unique<Platform>(driverHandle->toHandle()));
    VariableBackup<decltype(platformsImpl)> platformsBackup{&platformsImpl, &platforms};

    gtPinTryNotifyInit();

    EXPECT_EQ(1u, gtpinInitTimesCalled);
}

TEST_F(LeoGtpinInitTest, givenNullPlatformsWhenGtPinTryNotifyInitThenDoesNotInitializeGtpin) {
    debugManager.flags.ZET_ENABLE_PROGRAM_INSTRUMENTATION.set(true);
    VariableBackup<decltype(platformsImpl)> platformsBackup{&platformsImpl, nullptr};

    gtPinTryNotifyInit();

    EXPECT_EQ(0u, gtpinInitTimesCalled);
}

TEST_F(LeoGtpinInitTest, givenEmptyPlatformsWhenGtPinTryNotifyInitThenDoesNotInitializeGtpin) {
    debugManager.flags.ZET_ENABLE_PROGRAM_INSTRUMENTATION.set(true);
    std::vector<std::unique_ptr<Platform>> platforms;
    VariableBackup<decltype(platformsImpl)> platformsBackup{&platformsImpl, &platforms};

    gtPinTryNotifyInit();

    EXPECT_EQ(0u, gtpinInitTimesCalled);
}

struct LeoGtpinL0InitTest : public Test<OclFixture> {
    void SetUp() override {
        Test<OclFixture>::SetUp();
        driverHandles.push_back(driverHandle->toHandle());
        driver.gtPinInitializationNeeded = true;
    }

    void TearDown() override {
        delete MockOsLibrary::loadLibraryNewObject;
        MockOsLibrary::loadLibraryNewObject = nullptr;
        platforms.clear();
        Test<OclFixture>::TearDown();
    }

    static uint32_t l0GtpinInitTimesCalled;

    L0::ult::Mock<L0::ult::Driver> driver;
    std::vector<_ze_driver_handle_t *> driverHandles;
    std::vector<std::unique_ptr<Platform>> platforms;
    VariableBackup<uint32_t> gtpinCounterBackup{&l0GtpinInitTimesCalled, 0};
    VariableBackup<NEO::OsLibrary *> osLibraryBackup{&MockOsLibrary::loadLibraryNewObject, nullptr};
    VariableBackup<decltype(L0::globalDriverHandles)> globalDriverHandlesBackup{&L0::globalDriverHandles, &driverHandles};
    VariableBackup<bool> levelZeroDriverInitializedBackup{&L0::levelZeroDriverInitialized};
    VariableBackup<decltype(platformsImpl)> platformsBackup{&platformsImpl, &platforms};
    VariableBackup<bool> platformsInitAttemptedBackup{&platformsInitAttempted, false};
};
uint32_t LeoGtpinL0InitTest::l0GtpinInitTimesCalled = 0u;

TEST_F(LeoGtpinL0InitTest, givenL0GtpinInitPendingWhenGetPlatformIdsInitializesDriverThenL0GtpinIsNotInitializedUntilExplicitZeDriverGet) {
    cl_uint numPlatforms = 0;
    auto retVal = clGetPlatformIDs(0, nullptr, &numPlatforms);
    EXPECT_EQ(CL_SUCCESS, retVal);
    EXPECT_EQ(0u, l0GtpinInitTimesCalled);
    EXPECT_TRUE(driver.gtPinInitializationNeeded.load());
    EXPECT_FALSE(driver.gtpinInitSkipped.load());

    uint32_t (*openPinHandler)(void *) = [](void *arg) -> uint32_t {
        l0GtpinInitTimesCalled++;
        return 0;
    };
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> funcBackup{&NEO::OsLibrary::loadFunc, MockOsLibrary::load};
    auto osLibrary = new MockOsLibraryCustom(nullptr, false);
    osLibrary->procMap["OpenGTPin"] = reinterpret_cast<void *>(openPinHandler);
    MockOsLibrary::loadLibraryNewObject = osLibrary;

    uint32_t driverCount = 1;
    ze_driver_handle_t hDriver = nullptr;
    EXPECT_EQ(ZE_RESULT_SUCCESS, ::zeDriverGet(&driverCount, &hDriver));
    EXPECT_EQ(1u, l0GtpinInitTimesCalled);
    EXPECT_FALSE(driver.gtPinInitializationNeeded.load());
}

} // namespace ult
} // namespace LEO
} // namespace NEO
