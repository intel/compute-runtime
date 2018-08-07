/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/execution_environment/execution_environment.h"
#include "runtime/helpers/hw_info.h"
#include "unit_tests/custom_event_listener.h"
#include "unit_tests/helpers/debug_manager_state_restore.h"
#include "unit_tests/helpers/gtest_helpers.h"

using namespace OCLRT;

namespace OCLRT {
bool getDevices(HardwareInfo **hwInfo, size_t &numDevicesReturned, ExecutionEnvironment &executionEnvironment);
}

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
