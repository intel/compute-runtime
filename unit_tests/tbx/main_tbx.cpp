/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/unit_tests/helpers/debug_manager_state_restore.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/hw_info.h"
#include "unit_tests/custom_event_listener.h"
#include "unit_tests/helpers/gtest_helpers.h"

using namespace NEO;

namespace NEO {
bool getDevices(size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment);
} // namespace NEO

TEST(CSRTests, getDevices) {
    size_t numDevicesReturned = 0;

    DebugManagerStateRestore dbgState;
    DebugManager.flags.SetCommandStreamReceiver.set(2);
    ExecutionEnvironment executionEnvironment;
    NEO::getDevices(numDevicesReturned, executionEnvironment);

    auto hwInfo = executionEnvironment.getHardwareInfo();

    EXPECT_GT_VAL(hwInfo->gtSystemInfo.EUCount, 0u);
    EXPECT_GT_VAL(hwInfo->gtSystemInfo.ThreadCount, 0u);
    EXPECT_GT_VAL(hwInfo->gtSystemInfo.SliceCount, 0u);
    EXPECT_GT_VAL(hwInfo->gtSystemInfo.SubSliceCount, 0u);
    EXPECT_GT_VAL(hwInfo->gtSystemInfo.L3CacheSizeInKb, 0u);
    EXPECT_EQ_VAL(hwInfo->gtSystemInfo.CsrSizeInMb, 8u);
    EXPECT_FALSE(hwInfo->gtSystemInfo.IsDynamicallyPopulated);
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
