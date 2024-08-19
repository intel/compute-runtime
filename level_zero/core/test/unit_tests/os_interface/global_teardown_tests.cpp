/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/os_interface/global_teardown_tests.h"

#include "shared/source/os_interface/os_library.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/source/global_teardown.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"

namespace L0 {
namespace ult {

TEST(GlobalTearDownCallbackTests, givenL0LoaderThenGlobalTeardownCallbackIsCalled) {
    L0::levelZeroDriverInitialized = true;
    std::unique_ptr<NEO::OsLibrary> loaderLibrary = std::unique_ptr<NEO::OsLibrary>{NEO::OsLibrary::loadFunc(L0::ult::testLoaderDllName)};
    EXPECT_EQ(ZE_RESULT_SUCCESS, setDriverTeardownHandleInLoader(L0::ult::testLoaderDllName));
    L0::levelZeroDriverInitialized = false;
}
TEST(GlobalTearDownCallbackTests, givenL0LoaderButL0DriverDidNotInitThenSetTearDownReturnsUninitialized) {
    L0::levelZeroDriverInitialized = false;
    std::unique_ptr<NEO::OsLibrary> loaderLibrary = std::unique_ptr<NEO::OsLibrary>{NEO::OsLibrary::loadFunc(L0::ult::testLoaderDllName)};
    EXPECT_EQ(ZE_RESULT_ERROR_UNINITIALIZED, setDriverTeardownHandleInLoader(L0::ult::testLoaderDllName));
}
TEST(GlobalTearDownCallbackTests, givenL0LoaderIsMissingThenGlobalTeardownCallbackIsNotCalled) {
    L0::levelZeroDriverInitialized = true;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, setDriverTeardownHandleInLoader("invalid.so"));
    L0::levelZeroDriverInitialized = false;
}
TEST(GlobalTearDownCallbackTests, givenL0LoaderWithoutGlobalTeardownCallbackThenGlobalTeardownCallbackIsNotCalled) {
    L0::levelZeroDriverInitialized = true;
    EXPECT_EQ(ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE, setDriverTeardownHandleInLoader(L0::ult::testDllName));
    L0::levelZeroDriverInitialized = false;
}
TEST(GlobalTearDownTests, givenCallToGlobalTearDownFunctionThenGlobalDriversAreNull) {
    globalDriverTeardown();
    EXPECT_EQ(globalDriver, nullptr);
    EXPECT_EQ(Sysman::globalSysmanDriver, nullptr);
}
TEST(GlobalTearDownTests, givenCallToGlobalTearDownFunctionWithNullSysManDriverThenGlobalDriverIsNull) {
    delete Sysman::globalSysmanDriver;
    Sysman::globalSysmanDriver = nullptr;
    globalDriverTeardown();
    EXPECT_EQ(globalDriver, nullptr);
    EXPECT_EQ(Sysman::globalSysmanDriver, nullptr);
}

TEST(GlobalTearDownTests, givenForkedProcessWhenGlobalTearDownFunctionCalledThenGlobalDriverIsNotDeleted) {
    VariableBackup<uint32_t> driverCountBackup{&L0::driverCount};
    VariableBackup<_ze_driver_handle_t *> globalDriverHandleBackup{&L0::globalDriverHandle};

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_NE(L0::globalDriver, nullptr);

    // change pid in driver
    L0::globalDriver->pid = L0::globalDriver->pid + 5;

    auto tempDriver = L0::globalDriver;

    globalDriverTeardown();
    EXPECT_EQ(L0::globalDriver, nullptr);
    EXPECT_EQ(Sysman::globalSysmanDriver, nullptr);

    delete tempDriver;
}

} // namespace ult
} // namespace L0