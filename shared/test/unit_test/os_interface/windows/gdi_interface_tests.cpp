/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#if defined(_WIN32)
#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/gdi_interface_logging.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

TEST(GdiInterface, WhenGdiIsCreatedThenItIsInitialized) {
    NEO::Gdi gdi;
    ASSERT_TRUE(gdi.isInitialized());
}

TEST(GdiInterface, GivenInvalidGdiDllNameWhenCreatingGdiThenGdiIsNotInitialized) {
    const char *oldName = Os::gdiDllName;
    Os::gdiDllName = "surely_not_exists_.dll";

    NEO::Gdi gdi;
    EXPECT_FALSE(gdi.isInitialized());

    Os::gdiDllName = oldName;
}

TEST(GdiInterface, givenGdiOverridePathWhenGdiInterfaceIsCalledThenOverridePathIsUsed) {
    const char *oldName = Os::gdiDllName;

    DebugManagerStateRestore dbgRestorer;

    Os::gdiDllName = "surely_not_exists_.dll";
    DebugManager.flags.OverrideGdiPath.set(oldName);

    NEO::Gdi gdi;
    EXPECT_TRUE(gdi.isInitialized());

    Os::gdiDllName = oldName;
}

TEST(ThkWrapperTest, givenThkWrapperWhenConstructedThenmFuncIsInitialized) {
    NEO::ThkWrapper<void *> wrapper;
    EXPECT_EQ(nullptr, wrapper.mFunc);
}

TEST(GdiInterface, GivenGdiLoggingSupportWhenLoggingEnabledAndLoggingToFileUsedThenExpectIoFunctionsUsed) {
    if (!GdiLogging::gdiLoggingSupport) {
        GTEST_SKIP();
    }

    VariableBackup<uint32_t> mockFopenCalledBackup(&NEO::IoFunctions::mockFopenCalled, 0);
    VariableBackup<uint32_t> mockFcloseCalledBackup(&NEO::IoFunctions::mockFcloseCalled, 0);
    VariableBackup<uint32_t> mockVfptrinfCalledBackup(&NEO::IoFunctions::mockVfptrinfCalled, 0);

    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.LogGdiCalls.set(true);
    DebugManager.flags.LogGdiCallsToFile.set(true);

    std::unique_ptr<Gdi> gdi = std::make_unique<Gdi>();
    EXPECT_EQ(1u, NEO::IoFunctions::mockFopenCalled);

    D3DKMT_OPENADAPTERFROMLUID param = {};
    gdi->openAdapterFromLuid(&param);
    EXPECT_EQ(2u, NEO::IoFunctions::mockVfptrinfCalled);

    gdi.reset(nullptr);
    EXPECT_EQ(1u, NEO::IoFunctions::mockFcloseCalled);
}

TEST(GdiInterface, GivenGdiLoggingSupportWhenLoggingEnabledAndLoggingToFileNotUsedThenExpectIoFunctionsUsed) {
    if (!GdiLogging::gdiLoggingSupport) {
        GTEST_SKIP();
    }

    VariableBackup<uint32_t> mockFopenCalledBackup(&NEO::IoFunctions::mockFopenCalled, 0);
    VariableBackup<uint32_t> mockFcloseCalledBackup(&NEO::IoFunctions::mockFcloseCalled, 0);
    VariableBackup<uint32_t> mockVfptrinfCalledBackup(&NEO::IoFunctions::mockVfptrinfCalled, 0);

    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.LogGdiCalls.set(true);
    DebugManager.flags.LogGdiCallsToFile.set(false);

    std::unique_ptr<Gdi> gdi = std::make_unique<Gdi>();
    EXPECT_EQ(0u, NEO::IoFunctions::mockFopenCalled);

    D3DKMT_OPENADAPTERFROMLUID param = {};
    gdi->openAdapterFromLuid(&param);
    EXPECT_EQ(2u, NEO::IoFunctions::mockVfptrinfCalled);

    gdi.reset(nullptr);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);
}
#endif
