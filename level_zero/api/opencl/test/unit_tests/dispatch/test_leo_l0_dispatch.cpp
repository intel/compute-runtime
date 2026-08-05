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

#include "level_zero/api/opencl/source/l0_dispatch/leo_l0_dispatch.h"

#include <string>

namespace NEO {
namespace LEO {
namespace ult {

namespace {

ze_driver_handle_t nativeDriverHandle = reinterpret_cast<ze_driver_handle_t>(0x1000);
ze_driver_handle_t foreignDriverHandle = reinterpret_cast<ze_driver_handle_t>(0x2000);

uint32_t loaderInitDriversCalled = 0u;
uint32_t loaderCreateImmediateCalled = 0u;
uint32_t libraryLoadCalled = 0u;

ze_result_t ZE_APICALL mockLoaderInitDrivers(uint32_t *pCount, ze_driver_handle_t *phDrivers, ze_init_driver_type_desc_t *desc) {
    loaderInitDriversCalled++;
    *pCount = 1u;
    if (phDrivers != nullptr) {
        *phDrivers = nativeDriverHandle;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL mockLoaderCommandListCreateImmediate(ze_context_handle_t, ze_device_handle_t, const ze_command_queue_desc_t *, ze_command_list_handle_t *) {
    loaderCreateImmediateCalled++;
    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL directInitDriversReturningNative(uint32_t *pCount, ze_driver_handle_t *phDrivers, ze_init_driver_type_desc_t *) {
    *pCount = 1u;
    if (phDrivers != nullptr) {
        *phDrivers = nativeDriverHandle;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t ZE_APICALL directInitDriversReturningForeign(uint32_t *pCount, ze_driver_handle_t *phDrivers, ze_init_driver_type_desc_t *) {
    *pCount = 1u;
    if (phDrivers != nullptr) {
        *phDrivers = foreignDriverHandle;
    }
    return ZE_RESULT_SUCCESS;
}

MockOsLibraryCustom *pendingLibrary = nullptr;

OsLibrary *mockLoad(const OsLibraryCreateProperties &properties) {
    libraryLoadCalled++;
    return pendingLibrary;
}

OsLibrary *mockLoadFailing(const OsLibraryCreateProperties &properties) {
    libraryLoadCalled++;
    return nullptr;
}

MockOsLibraryCustom *createLoaderLibrary(bool withInitDrivers, bool withCommandListEntry) {
    auto library = new MockOsLibraryCustom(nullptr, true);
    if (withInitDrivers) {
        library->procMap["zeInitDrivers"] = reinterpret_cast<void *>(&mockLoaderInitDrivers);
    }
    if (withCommandListEntry) {
        library->procMap["zeCommandListCreateImmediate"] = reinterpret_cast<void *>(&mockLoaderCommandListCreateImmediate);
    }
    return library;
}

bool createImmediateRoutedToLoader() {
    return LEO::zeCommandListCreateImmediate == &mockLoaderCommandListCreateImmediate;
}

} // namespace

struct LeoL0DispatchTest : public ::testing::Test {
    void SetUp() override {
        loaderInitDriversCalled = 0u;
        loaderCreateImmediateCalled = 0u;
        libraryLoadCalled = 0u;
        pendingLibrary = nullptr;
    }

    void TearDown() override {
        disarmL0Dispatch();
        EXPECT_FALSE(createImmediateRoutedToLoader());
        // Production never frees the loader - unloading it is what this is
        // deliberately avoiding - so the mock is the test's to dispose of. Only
        // ever the mock: the backup below starts every case from nullptr, so
        // anything here was installed by mockLoad, and a real loader another
        // test left behind is restored rather than unloaded.
        delete loaderLibrary;
        loaderLibrary = nullptr;
    }

    DebugManagerStateRestore restorer;
    VariableBackup<OsLibrary *> loaderLibraryBackup{&loaderLibrary, nullptr};
    VariableBackup<decltype(OsLibrary::loadFunc)> loadFuncBackup{&OsLibrary::loadFunc, mockLoad};
    VariableBackup<decltype(&::zeInitDrivers)> directInitDriversBackup{&directInitDrivers, &directInitDriversReturningNative};
};

TEST_F(LeoL0DispatchTest, givenDispatchNotArmedThenEntriesHoldTheDriverEntryPoints) {
    EXPECT_EQ(&::zeCommandListCreateImmediate, LEO::zeCommandListCreateImmediate);
    EXPECT_EQ(&::zeInitDrivers, LEO::zeInitDrivers);
}

TEST_F(LeoL0DispatchTest, givenLoaderDispatchDisabledWhenArmingThenLoaderIsNotLoadedAndEntriesStayOnDriver) {
    debugManager.flags.EnableLEOLoaderDispatch.set(0);

    EXPECT_FALSE(armL0Dispatch());
    EXPECT_FALSE(createImmediateRoutedToLoader());
    EXPECT_EQ(0u, libraryLoadCalled);
}

TEST_F(LeoL0DispatchTest, givenLoaderDispatchEnabledWhenArmingThenEntriesAreRoutedToLoader) {
    debugManager.flags.EnableLEOLoaderDispatch.set(1);
    pendingLibrary = createLoaderLibrary(true, true);

    EXPECT_TRUE(armL0Dispatch());
    EXPECT_EQ(1u, libraryLoadCalled);
    EXPECT_TRUE(createImmediateRoutedToLoader());
}

TEST_F(LeoL0DispatchTest, givenDefaultFlagWhenArmingThenLoaderIsNotLoadedAndEntriesStayOnDriver) {
    debugManager.flags.EnableLEOLoaderDispatch.set(-1);

    // The key is the only control: nothing else, the loader tracing layer
    // included, turns this on.
    EXPECT_FALSE(armL0Dispatch());
    EXPECT_FALSE(createImmediateRoutedToLoader());
    EXPECT_EQ(0u, libraryLoadCalled);
}

TEST_F(LeoL0DispatchTest, givenLoaderLibraryMissingWhenArmingThenEntriesStayOnDriver) {
    debugManager.flags.EnableLEOLoaderDispatch.set(1);
    VariableBackup<decltype(OsLibrary::loadFunc)> failingLoad{&OsLibrary::loadFunc, mockLoadFailing};

    EXPECT_FALSE(armL0Dispatch());
    EXPECT_FALSE(createImmediateRoutedToLoader());
    EXPECT_EQ(1u, libraryLoadCalled);
}

TEST_F(LeoL0DispatchTest, givenLoaderReturningForeignDriverHandleWhenArmingThenEntriesAreRestoredToDriver) {
    debugManager.flags.EnableLEOLoaderDispatch.set(1);
    pendingLibrary = createLoaderLibrary(true, true);
    VariableBackup<decltype(&::zeInitDrivers)> foreignDirect{&directInitDrivers, &directInitDriversReturningForeign};

    EXPECT_FALSE(armL0Dispatch());
    EXPECT_EQ(1u, loaderInitDriversCalled);
    // The entries were filled before the check ran, so this also proves the restore.
    EXPECT_FALSE(createImmediateRoutedToLoader());
    // Backing out must not unload the loader: its context destructor would free
    // every driver library it loaded, this one included.
    EXPECT_NE(nullptr, loaderLibrary);
}

TEST_F(LeoL0DispatchTest, givenLoaderDispatchDisabledWhenInitIsCalledThenLoaderIsNotLoaded) {
    debugManager.flags.EnableLEOLoaderDispatch.set(0);

    // This is the production entry point. It is guarded by a function local
    // once_flag, and the ULTs run several iterations in one process, so by the
    // time this runs the guard may already have been spent by whichever test
    // went through clGetPlatformIDs first. Deliberately assert only what holds
    // either way, and install no loader: a mock that arming never adopts would
    // simply leak, which is what the leak listener caught.
    initL0Dispatch();
    initL0Dispatch();

    EXPECT_EQ(0u, libraryLoadCalled);
    EXPECT_FALSE(createImmediateRoutedToLoader());
}

TEST_F(LeoL0DispatchTest, givenDisarmedDispatchWhenArmingAgainThenLoaderIsLoadedOnlyOnce) {
    debugManager.flags.EnableLEOLoaderDispatch.set(1);
    pendingLibrary = createLoaderLibrary(true, true);

    ASSERT_TRUE(armL0Dispatch());
    EXPECT_EQ(1u, libraryLoadCalled);

    disarmL0Dispatch();
    EXPECT_FALSE(createImmediateRoutedToLoader());
    EXPECT_NE(nullptr, loaderLibrary);

    EXPECT_TRUE(armL0Dispatch());
    EXPECT_EQ(1u, libraryLoadCalled);
    EXPECT_TRUE(createImmediateRoutedToLoader());
}

TEST_F(LeoL0DispatchTest, givenLoaderNotExportingInitDriversWhenArmingThenEntriesStayOnDriver) {
    debugManager.flags.EnableLEOLoaderDispatch.set(1);
    pendingLibrary = createLoaderLibrary(false, true);

    // Without it the handle check has nothing to compare against, and it must
    // not fall back to comparing the driver against itself.
    EXPECT_FALSE(armL0Dispatch());
    EXPECT_FALSE(createImmediateRoutedToLoader());
}

TEST_F(LeoL0DispatchTest, givenArmedDispatchWhenLeoCallsTheApiThenLoaderEntryPointIsUsed) {
    debugManager.flags.EnableLEOLoaderDispatch.set(1);
    pendingLibrary = createLoaderLibrary(true, true);

    ASSERT_TRUE(armL0Dispatch());
    // Asserted before calling through: only the loader stub tolerates these
    // arguments, so if the entry were not routed the call would crash instead
    // of failing.
    ASSERT_TRUE(createImmediateRoutedToLoader());

    // This is the shadowing itself: the call is spelled exactly as LEO spells it
    // and it is inside namespace NEO::LEO, so it has to reach the dispatch
    // rather than the global API. Safe to call - the loader stub ignores its
    // arguments.
    EXPECT_EQ(ZE_RESULT_SUCCESS, zeCommandListCreateImmediate(nullptr, nullptr, nullptr, nullptr));
    EXPECT_EQ(1u, loaderCreateImmediateCalled);
}

TEST_F(LeoL0DispatchTest, givenArmedDispatchAndEntryNotExportedByLoaderWhenLeoCallsTheApiThenDriverIsUsed) {
    debugManager.flags.EnableLEOLoaderDispatch.set(1);
    pendingLibrary = createLoaderLibrary(true, false);

    ASSERT_TRUE(armL0Dispatch());

    // Asserted on the entry rather than by calling it: the driver dereferences
    // these handles, so calling it with nulls crashes instead of returning.
    EXPECT_EQ(&::zeCommandListCreateImmediate, LEO::zeCommandListCreateImmediate);
    EXPECT_FALSE(createImmediateRoutedToLoader());
    EXPECT_EQ(0u, loaderCreateImmediateCalled);
}

} // namespace ult
} // namespace LEO
} // namespace NEO
