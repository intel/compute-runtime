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
    EXPECT_EQ(0u, BlitterDispatcher<FamilyType>::getSizePreemption());
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenDispatchingPreemptionCmdThenDispatchNothing) {
    BlitterDispatcher<FamilyType>::dispatchPreemption(cmdBuffer);

    EXPECT_EQ(0u, cmdBuffer.getUsed());
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenAskingForMonitorFenceCmdSizeThenReturnZero) {
    EXPECT_EQ(0u, BlitterDispatcher<FamilyType>::getSizeMonitorFence(pDevice->getHardwareInfo()));
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenDispatchingMonitorFenceCmdThenDispatchNothing) {
    BlitterDispatcher<FamilyType>::dispatchMonitorFence(cmdBuffer, MemoryConstants::pageSize64k, 1ull, pDevice->getHardwareInfo());

    EXPECT_EQ(0u, cmdBuffer.getUsed());
}
HWTEST_F(BlitterDispatcheTest, givenBlitterWhenAskingForCacheFlushCmdSizeThenReturnZero) {
    EXPECT_EQ(0u, BlitterDispatcher<FamilyType>::getSizeCacheFlush(pDevice->getHardwareInfo()));
}

HWTEST_F(BlitterDispatcheTest, givenBlitterWhenDispatchingCacheFlushCmdThenDispatchNothing) {
    BlitterDispatcher<FamilyType>::dispatchCacheFlush(cmdBuffer, pDevice->getHardwareInfo());

    EXPECT_EQ(0u, cmdBuffer.getUsed());
}
