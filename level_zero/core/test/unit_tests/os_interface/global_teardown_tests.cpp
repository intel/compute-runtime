/*
 * Copyright (C) 2023-2025 Intel Corporation
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
#include "level_zero/ddi/ze_ddi_tables.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"

namespace L0 {
namespace ult {

struct GlobalTearDownTests : public ::testing::Test {
    VariableBackup<decltype(globalDriverHandles)> globalDriverHandlesBackup{&globalDriverHandles, nullptr};
};
TEST_F(GlobalTearDownTests, whenCallingGlobalDriverSetupThenLoaderFunctionForTranslateHandleIsLoadedIfAvailable) {
    void *mockSetDriverTeardownPtr = reinterpret_cast<void *>(static_cast<uintptr_t>(0x1234ABC8));
    void *mockLoaderTranslateHandlePtr = reinterpret_cast<void *>(static_cast<uintptr_t>(0x5678EF08));

    VariableBackup<decltype(setDriverTeardownFunc)> teardownFuncBackup{&setDriverTeardownFunc, nullptr};
    VariableBackup<decltype(loaderTranslateHandleFunc)> translateFuncBackup{&loaderTranslateHandleFunc, nullptr};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> loadFuncBackup{&NEO::OsLibrary::loadFunc, MockOsLibraryCustom::load};
    VariableBackup<decltype(MockOsLibrary::loadLibraryNewObject)> mockLibraryBackup{&MockOsLibrary::loadLibraryNewObject, nullptr};

    MockOsLibrary::loadLibraryNewObject = nullptr;
    globalDriverSetup();

    EXPECT_EQ(nullptr, setDriverTeardownFunc);
    EXPECT_EQ(nullptr, loaderTranslateHandleFunc);

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    globalDriverSetup();

    EXPECT_EQ(nullptr, setDriverTeardownFunc);
    EXPECT_EQ(nullptr, loaderTranslateHandleFunc);

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    auto osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap["zelSetDriverTeardown"] = mockSetDriverTeardownPtr;
    globalDriverSetup();

    EXPECT_EQ(nullptr, setDriverTeardownFunc);
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

    EXPECT_EQ(nullptr, setDriverTeardownFunc);
    EXPECT_EQ(mockLoaderTranslateHandlePtr, reinterpret_cast<void *>(loaderTranslateHandleFunc));
    globalDriverTeardown();
}

uint32_t loaderTearDownCalled = 0;

ze_result_t loaderTearDown() {
    loaderTearDownCalled++;
    return ZE_RESULT_ERROR_UNKNOWN;
};

TEST_F(GlobalTearDownTests, givenInitializedDriverWhenCallingGlobalDriverTeardownThenLoaderFunctionForTeardownIsLoadedAndCalledIfAvailable) {

    void *mockLoaderTranslateHandlePtr = reinterpret_cast<void *>(static_cast<uintptr_t>(0x5678EF08));

    VariableBackup<decltype(loaderTearDownCalled)> loaderTeardownCalledBackup{&loaderTearDownCalled, 0};
    VariableBackup<decltype(levelZeroDriverInitialized)> driverInitializeBackup{&levelZeroDriverInitialized, true};
    VariableBackup<decltype(setDriverTeardownFunc)> teardownFuncBackup{&setDriverTeardownFunc, nullptr};
    VariableBackup<decltype(loaderTranslateHandleFunc)> translateFuncBackup{&loaderTranslateHandleFunc, nullptr};
    VariableBackup<decltype(NEO::OsLibrary::loadFunc)> loadFuncBackup{&NEO::OsLibrary::loadFunc, MockOsLibraryCustom::load};
    VariableBackup<decltype(MockOsLibrary::loadLibraryNewObject)> mockLibraryBackup{&MockOsLibrary::loadLibraryNewObject, nullptr};

    loaderTranslateHandleFunc = reinterpret_cast<decltype(loaderTranslateHandleFunc)>(mockLoaderTranslateHandlePtr);
    MockOsLibrary::loadLibraryNewObject = nullptr;
    globalDriverTeardown();

    EXPECT_EQ(nullptr, setDriverTeardownFunc);
    EXPECT_EQ(nullptr, loaderTranslateHandleFunc);

    loaderTranslateHandleFunc = reinterpret_cast<decltype(loaderTranslateHandleFunc)>(mockLoaderTranslateHandlePtr);
    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    globalDriverTeardown();

    EXPECT_EQ(nullptr, setDriverTeardownFunc);
    EXPECT_EQ(mockLoaderTranslateHandlePtr, reinterpret_cast<void *>(loaderTranslateHandleFunc));

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    auto osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap["zelSetDriverTeardown"] = reinterpret_cast<void *>(&loaderTearDown);
    globalDriverTeardown();

    EXPECT_EQ(&loaderTearDown, reinterpret_cast<void *>(setDriverTeardownFunc));
    EXPECT_EQ(mockLoaderTranslateHandlePtr, reinterpret_cast<void *>(loaderTranslateHandleFunc));
    EXPECT_EQ(1u, loaderTearDownCalled);

    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap["zelLoaderTranslateHandle"] = mockLoaderTranslateHandlePtr;
    globalDriverTeardown();

    EXPECT_EQ(nullptr, setDriverTeardownFunc);
    EXPECT_EQ(mockLoaderTranslateHandlePtr, reinterpret_cast<void *>(loaderTranslateHandleFunc));
    EXPECT_EQ(1u, loaderTearDownCalled);

    loaderTranslateHandleFunc = nullptr;
    MockOsLibrary::loadLibraryNewObject = new MockOsLibraryCustom(nullptr, true);
    osLibrary = static_cast<MockOsLibraryCustom *>(MockOsLibrary::loadLibraryNewObject);
    osLibrary->procMap["zelSetDriverTeardown"] = reinterpret_cast<void *>(&loaderTearDown);
    osLibrary->procMap["zelLoaderTranslateHandle"] = mockLoaderTranslateHandlePtr;
    globalDriverTeardown();

    EXPECT_EQ(&loaderTearDown, reinterpret_cast<void *>(setDriverTeardownFunc));
    EXPECT_EQ(nullptr, loaderTranslateHandleFunc);
}

TEST_F(GlobalTearDownTests, givenInitializedDriverAndNoTeardownFunctionIsAvailableWhenCallGlobalTeardownThenDontCrash) {
    VariableBackup<bool> initializedBackup{&levelZeroDriverInitialized};
    VariableBackup<decltype(setDriverTeardownFunc)> teardownFuncBackup{&setDriverTeardownFunc};

    levelZeroDriverInitialized = true;
    setDriverTeardownFunc = nullptr;
    EXPECT_NO_THROW(globalDriverTeardown());
}

TEST_F(GlobalTearDownTests, givenNotInitializedDriverAndTeardownFunctionIsAvailableWhenCallGlobalTeardownThenDontCallTeardownFunc) {
    VariableBackup<bool> initializedBackup{&levelZeroDriverInitialized};
    VariableBackup<decltype(setDriverTeardownFunc)> teardownFuncBackup{&setDriverTeardownFunc};

    levelZeroDriverInitialized = false;
    setDriverTeardownFunc = []() -> ze_result_t {
        EXPECT_TRUE(false);
        return ZE_RESULT_SUCCESS;
    };
    EXPECT_NO_THROW(globalDriverTeardown());
}

TEST_F(GlobalTearDownTests, givenCallToGlobalTearDownFunctionThenGlobalDriversAreNull) {
    globalDriverTeardown();
    EXPECT_EQ(globalDriverHandles, nullptr);
    EXPECT_EQ(Sysman::globalSysmanDriver, nullptr);
}
TEST_F(GlobalTearDownTests, givenCallToGlobalTearDownFunctionWithNullSysManDriverThenGlobalDriverIsNull) {
    delete Sysman::globalSysmanDriver;
    Sysman::globalSysmanDriver = nullptr;
    globalDriverTeardown();
    EXPECT_EQ(globalDriverHandles, nullptr);
    EXPECT_EQ(Sysman::globalSysmanDriver, nullptr);
}

TEST_F(GlobalTearDownTests, givenForkedProcessWhenGlobalTearDownFunctionCalledThenGlobalDriverIsNotDeleted) {
    VariableBackup<decltype(globalDriverHandles)> globalDriverHandlesBackup{&globalDriverHandles, nullptr};

    globalDriverHandles = new std::vector<_ze_driver_handle_t *>;

    ze_result_t result = ZE_RESULT_ERROR_UNINITIALIZED;
    DriverImp driverImp;
    driverImp.initialize(&result);
    EXPECT_EQ(ZE_RESULT_SUCCESS, result);
    EXPECT_EQ(1u, globalDriverHandles->size());

    auto tempDriver = static_cast<L0::DriverHandleImp *>(DriverHandle::fromHandle((*globalDriverHandles)[0]));
    EXPECT_NE(tempDriver, nullptr);

    // change pid in driver
    tempDriver->pid = tempDriver->pid + 5;

    globalDriverTeardown();
    EXPECT_EQ(globalDriverHandles, nullptr);
    EXPECT_EQ(Sysman::globalSysmanDriver, nullptr);

    delete tempDriver;
}

TEST_F(GlobalTearDownTests, givenGlobalDriverDispatchWhenGlobalSetupAndTeardownAreCalledThenPerApiValidFlagsAreChanged) {
    VariableBackup<DriverDispatch> globalDispatchBackup{&globalDriverDispatch};

    globalDriverSetup();

    EXPECT_TRUE(globalDriverDispatch.core.isValidFlag);
    EXPECT_TRUE(globalDriverDispatch.tools.isValidFlag);
    EXPECT_TRUE(globalDriverDispatch.sysman.isValidFlag);

    globalDriverTeardown();

    EXPECT_FALSE(globalDriverDispatch.core.isValidFlag);
    EXPECT_FALSE(globalDriverDispatch.tools.isValidFlag);
    EXPECT_FALSE(globalDriverDispatch.sysman.isValidFlag);
}
} // namespace ult
} // namespace L0
