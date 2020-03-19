/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/direct_submission/dispatchers/blitter_dispatcher.h"
#include "shared/test/unit_test/direct_submission/dispatchers/dispatcher_fixture.h"

#include "test.h"

using namespace NEO;

using BlitterDispatcheTest = Test<DispatcherFixture>;

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenAskingForPreemptionCmdSizeThenReturnZero) {
    BlitterDispatcher<FamilyType> blitterDispatcher;
    EXPECT_EQ(0u, blitterDispatcher.getSizePreemption());
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenDispatchingPreemptionCmdThenDispatchNothing) {
    BlitterDispatcher<FamilyType> blitterDispatcher;
    blitterDispatcher.dispatchPreemption(cmdBuffer);

    EXPECT_EQ(0u, cmdBuffer.getUsed());
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenAskingForMonitorFenceCmdSizeThenReturnZero) {
    BlitterDispatcher<FamilyType> blitterDispatcher;
    EXPECT_EQ(0u, blitterDispatcher.getSizeMonitorFence(pDevice->getHardwareInfo()));
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenDispatchingMonitorFenceCmdThenDispatchNothing) {
    BlitterDispatcher<FamilyType> blitterDispatcher;
    blitterDispatcher.dispatchMonitorFence(cmdBuffer, MemoryConstants::pageSize64k, 1ull, pDevice->getHardwareInfo());

    EXPECT_EQ(0u, cmdBuffer.getUsed());
}
HWTEST_F(BlitterDispatcheTest, givenBlitterWhenAskingForCacheFlushCmdSizeThenReturnZero) {
    BlitterDispatcher<FamilyType> blitterDispatcher;
    EXPECT_EQ(0u, blitterDispatcher.getSizeCacheFlush(pDevice->getHardwareInfo()));
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenDispatchingCacheFlushCmdThenDispatchNothing) {
    BlitterDispatcher<FamilyType> blitterDispatcher;
    blitterDispatcher.dispatchCacheFlush(cmdBuffer, pDevice->getHardwareInfo());

    EXPECT_EQ(0u, cmdBuffer.getUsed());
}
