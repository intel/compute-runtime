/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/test/unit_tests/os_interface/global_teardown_tests.h"

#include "shared/source/os_interface/os_library.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_os_library.h"
#include "shared/test/common/test_macros/test.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/core/source/driver/driver_imp.h"
#include "level_zero/core/source/global_teardown.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"

namespace L0 {
namespace ult {

TEST(GlobalTearDownTests, whenCallingGlobalDriverSetupThenLoaderFunctionsAreLoadedIfAvailable) {

    void *mockSetDriverTeardownPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(0x1234ABC8));
    void *mockLoaderTranslateHandlePtr = reinterpret_cast<void *>(static_cast<uintptr_t>(0x5678EF08));

    VariableBackup<decltype(setDriverTeardownFunc)> teardownFuncBackup{&setDriverTeardownFunc, nullptr};
    VariableBackup<decltype(loaderTranslateHandleFunc)> translateFuncBackup{&loaderTranslateHandleFunc, nullptr};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> loadFuncBackup{&NEO::OsLibrary::loadFunc, MockOsLibraryCustom::load};
    VariableBackup<decltype(MockOsLibrary::loadLibraryNewObject)> mockLibraryBackup{&MockOsLibrary::loadLibraryNewObject, nullptr};

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    globalDriverSetup();

    EXPECT_EQ(nullptr, setDriverTeardownFunc);
    EXPECT_EQ(nullptr, loaderTranslateHandleFunc);

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    auto osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap["zelSetDriverTeardown"] = mockSetDriverTeardownPtr;
    globalDriverSetup();

    EXPECT_EQ(mockSetDriverTeardownPtr, reinterpret_cast<void *>(setDriverTeardownFunc));
    EXPECT_EQ(nullptr, loaderTranslateHandleFunc);

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap["zelLoaderTranslateHandle"] = mockLoaderTranslateHandlePtr;
    globalDriverSetup();

    EXPECT_EQ(nullptr, setDriverTeardownFunc);
    EXPECT_EQ(mockLoaderTranslateHandlePtr, reinterpret_cast<void *>(loaderTranslateHandleFunc));

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap["zelSetDriverTeardown"] = mockSetDriverTeardownPtr;
    osLibrary->procMap["zelLoaderTranslateHandle"] = mockLoaderTranslateHandlePtr;
    globalDriverSetup();

    EXPECT_EQ(mockSetDriverTeardownPtr, reinterpret_cast<void *>(setDriverTeardownFunc));
    EXPECT_EQ(mockLoaderTranslateHandlePtr, reinterpret_cast<void *>(loaderTranslateHandleFunc));
}

TEST(GlobalTearDownTests, givenInitializedDriverAndNoTeardownFunctionIsAvailableWhenCallGlobalTeardownThenDontCrash) {
    VariableBackup<bool> initializedBackup{&levelZeroDriverInitialized};
    VariableBackup<decltype(setDriverTeardownFunc)> teardownFuncBackup{&setDriverTeardownFunc};

    levelZeroDriverInitialized = true;
    setDriverTeardownFunc = nullptr;
    EXPECT_NO_THROW(globalDriverTeardown());
}

TEST(GlobalTearDownTests, givenInitializedDriverAndTeardownFunctionIsAvailableWhenCallGlobalTeardownThenCallTeardownFunc) {
    VariableBackup<bool> initializedBackup{&levelZeroDriverInitialized};
    VariableBackup<decltype(setDriverTeardownFunc)> teardownFuncBackup{&setDriverTeardownFunc};
    static uint32_t teardownCalled = 0u;

    levelZeroDriverInitialized = true;
    setDriverTeardownFunc = []() -> ze_result_t {
        EXPECT_EQ(0u, teardownCalled);
        teardownCalled++;
        return ZE_RESULT_SUCCESS;
    };
    teardownCalled = 0u;
    globalDriverTeardown();
    EXPECT_EQ(1u, teardownCalled);
}

TEST(GlobalTearDownTests, givenNotInitializedDriverAndTeardownFunctionIsAvailableWhenCallGlobalTeardownThenDontCallTeardownFunc) {
    VariableBackup<bool> initializedBackup{&levelZeroDriverInitialized};
    VariableBackup<decltype(setDriverTeardownFunc)> teardownFuncBackup{&setDriverTeardownFunc};

    levelZeroDriverInitialized = false;
    setDriverTeardownFunc = []() -> ze_result_t {
        EXPECT_TRUE(false);
        return ZE_RESULT_SUCCESS;
    };
    EXPECT_NO_THROW(globalDriverTeardown());
}

TEST(GlobalTearDownTests, givenInitializedDriverAndTeardownFunctionFailsWhenCallGlobalTeardownThenDontCrash) {
    VariableBackup<bool> initializedBackup{&levelZeroDriverInitialized};
    VariableBackup<decltype(setDriverTeardownFunc)> teardownFuncBackup{&setDriverTeardownFunc};
    static uint32_t teardownCalled = 0u;

    levelZeroDriverInitialized = true;
    setDriverTeardownFunc = []() -> ze_result_t {
        EXPECT_EQ(0u, teardownCalled);
        teardownCalled++;
        return ZE_RESULT_ERROR_UNKNOWN;
    };
    teardownCalled = 0u;
    EXPECT_NO_THROW(globalDriverTeardown());
    EXPECT_EQ(1u, teardownCalled);
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
