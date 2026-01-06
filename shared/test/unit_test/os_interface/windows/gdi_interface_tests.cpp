/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#if defined(_WIN32)
#include "shared/source/os_interface/os_library.h"
#include "shared/source/os_interface/windows/gdi_interface.h"
#include "shared/source/os_interface/windows/gdi_interface_logging.h"
#include "shared/source/os_interface/windows/os_inc.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_io_functions.h"
#include "shared/test/common/mocks/windows/mock_gdi_interface.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

TEST(GdiInterface, WhenGdiIsCreatedThenItIsInitialized) {
    NEO::Gdi gdi;
    ASSERT_TRUE(gdi.isInitialized());
}

TEST(GdiInterface, DISABLED_GivenInvalidGdiDllNameWhenCreatingGdiThenGdiIsNotInitialized) {
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
    debugManager.flags.OverrideGdiPath.set(oldName);

    NEO::Gdi gdi;
    EXPECT_TRUE(gdi.isInitialized());

    Os::gdiDllName = oldName;
}

TEST(GdiInterface, givenPrintKmdTimesWhenCallThkWrapperThenRecordTime) {
    if (!GdiLogging::gdiLoggingSupport) {
        GTEST_SKIP();
    }

    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.PrintKmdTimes.set(1);

    auto gdi = std::make_unique<Gdi>();
    EXPECT_TRUE(gdi->isInitialized());

    StreamCapture capture;
    capture.captureStdout();

    D3DKMT_OPENADAPTERFROMLUID param = {};
    gdi->openAdapterFromLuid(&param);
    gdi->openAdapterFromLuid(&param);
    D3DKMT_CLOSEADAPTER closeAdapter = {};
    closeAdapter.hAdapter = param.hAdapter;
    gdi->closeAdapter(&closeAdapter);

    gdi.reset();
    auto output = capture.getCapturedStdout();
    EXPECT_TRUE(output.find("\n--- Gdi statistics ---\n") != std::string::npos);
}

TEST(ThkWrapperTest, givenThkWrapperWhenConstructedThenmFuncIsInitialized) {
    GdiProfiler profiler{};
    NEO::ThkWrapper<void *> wrapper(profiler, "nullptr", 0u);
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
    debugManager.flags.LogGdiCalls.set(true);
    debugManager.flags.LogGdiCallsToFile.set(true);

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
    debugManager.flags.LogGdiCalls.set(true);
    debugManager.flags.LogGdiCallsToFile.set(false);

    StreamCapture capture;
    capture.captureStdout();

    std::unique_ptr<Gdi> gdi = std::make_unique<Gdi>();
    EXPECT_EQ(0u, NEO::IoFunctions::mockFopenCalled);

    D3DKMT_OPENADAPTERFROMLUID param = {};
    gdi->openAdapterFromLuid(&param);
    EXPECT_EQ(2u, NEO::IoFunctions::mockVfptrinfCalled);

    gdi.reset(nullptr);
    EXPECT_EQ(0u, NEO::IoFunctions::mockFcloseCalled);

    capture.getCapturedStdout();
}
#endif
