/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/hw_info.h"
#include "unit_tests/custom_event_listener.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/gtest_helpers.h"

using namespace OCLRT;

namespace OCLRT {
bool getDevices(HardwareInfo **hwInfo, size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment);
} // namespace OCLRT

TEST(CSRTests, getDevices) {
    HardwareInfo *hwInfo = nullptr;
    size_t numDevicesReturned = 0;

    DebugManagerStateRestore dbgState;
    DebugManager.flags.SetCommandStreamReceiver.set(2);
    ExecutionEnvironment executionEnvironment;
    OCLRT::getDevices(&hwInfo, numDevicesReturned, executionEnvironment);

    ASSERT_NE(nullptr, hwInfo);
    ASSERT_NE(nullptr, hwInfo->pSysInfo);

    EXPECT_GT_VAL(hwInfo->pSysInfo->EUCount, 0u);
    EXPECT_GT_VAL(hwInfo->pSysInfo->ThreadCount, 0u);
    EXPECT_GT_VAL(hwInfo->pSysInfo->SliceCount, 0u);
    EXPECT_GT_VAL(hwInfo->pSysInfo->SubSliceCount, 0u);
    EXPECT_GT_VAL(hwInfo->pSysInfo->L3CacheSizeInKb, 0u);
    EXPECT_EQ_VAL(hwInfo->pSysInfo->CsrSizeInMb, 8u);
    EXPECT_FALSE(hwInfo->pSysInfo->IsDynamicallyPopulated);
}

int main(int argc, char **argv) {
    bool useDefaultListener = false;

    ::testing::InitGoogleTest(&argc, argv);

    // parse remaining args assuming they're mine
    for (int i = 1; i < argc; ++i) {
        if (!strcmp("--enable_default_listener", argv[i])) {
            useDefaultListener = true;
        }
    }

    if (useDefaultListener == false) {
        auto &listeners = ::testing::UnitTest::GetInstance()->listeners();
        auto defaultListener = listeners.default_result_printer();
        auto customEventListener = new CCustomEventListener(defaultListener);

        listeners.Release(defaultListener);
        listeners.Append(customEventListener);
    }

    auto retVal = RUN_ALL_TESTS();

    return retVal;
}
