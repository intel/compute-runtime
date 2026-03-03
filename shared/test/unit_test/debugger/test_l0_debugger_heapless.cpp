/*
 * Copyright (C) 2020-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/test/common/fixtures/device_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/stream_capture.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_l0_debugger.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using L0DebuggerHeaplessTest = Test<DeviceFixture>;
using PlatformsSupportingSbaTracking = IsWithinGfxCore<IGFX_GEN12_CORE, IGFX_XE3_CORE>;

HWTEST_F(L0DebuggerHeaplessTest, givenDebuggerWhenInitializedThenIsSbaTrackingMatchesHeaplessMode) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    debugger->initialize();

    auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    bool isHeapless = compilerProductHelper.isHeaplessModeEnabled(pDevice->getHardwareInfo());

    EXPECT_EQ(!isHeapless, debugger->isSbaTrackingEnabled());
    EXPECT_EQ(!isHeapless, debugger->debuggerRequiresSBATracking);
}

HWTEST_F(L0DebuggerHeaplessTest, givenSbaTrackingDisabledWhenCaptureStateBaseAddressThenNoCommandsAdded) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    debugger->debuggerRequiresSBATracking = false;

    StackVec<char, 4096> buffer(4096);
    NEO::LinearStream cmdStream(buffer.begin(), buffer.size());

    NEO::Debugger::SbaAddresses sba = {};
    sba.generalStateBaseAddress = 0x1000;
    sba.surfaceStateBaseAddress = 0x2000;
    sba.indirectObjectBaseAddress = 0x3000;
    sba.instructionBaseAddress = 0x4000;
    sba.bindlessSurfaceStateBaseAddress = 0x5000;

    debugger->captureStateBaseAddress(cmdStream, sba, false);

    EXPECT_EQ(0u, cmdStream.getUsed());
}

HWTEST_F(L0DebuggerHeaplessTest, givenSbaTrackingDisabledWhenGetSbaAddressLoadCommandsSizeThenZeroReturned) {
    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    debugger->debuggerRequiresSBATracking = false;

    EXPECT_EQ(0u, debugger->getSbaAddressLoadCommandsSize());
}

HWTEST_F(L0DebuggerHeaplessTest, givenSbaTrackingDisabledWhenDebuggerDestroyedThenNoSbaCleanupPerformed) {
    auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    if (!compilerProductHelper.isHeaplessModeEnabled(pDevice->getHardwareInfo())) {
        GTEST_SKIP();
    }

    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    debugger->initialize();

    EXPECT_FALSE(debugger->isSbaTrackingEnabled());
    EXPECT_TRUE(debugger->perContextSbaAllocations.empty());
    EXPECT_EQ(0u, debugger->sbaTrackingGpuVa.size);
}

HWTEST_F(L0DebuggerHeaplessTest, givenSbaTrackingDisabledWhenCallingprintTrackedAddressesThenNothingIsPrinted) {
    auto &compilerProductHelper = pDevice->getRootDeviceEnvironment().getHelper<CompilerProductHelper>();
    if (!compilerProductHelper.isHeaplessModeEnabled(pDevice->getHardwareInfo())) {
        GTEST_SKIP();
    }

    auto debugger = std::make_unique<MockDebuggerL0Hw<FamilyType>>(pDevice);
    debugger->initialize();
    StreamCapture capture;
    capture.captureStdout();
    debugger->printTrackedAddresses(0);
    std::string output = capture.getCapturedStdout();
    size_t pos = output.find("INFO: Debugger: SBA stored ssh");
    EXPECT_EQ(std::string::npos, pos);
}
