/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/command_queue/walker_partition_fixture_xehp_and_later.h"

void WalkerPartitionTests::SetUp() {
    cmdBufferAddress = cmdBuffer;

    testArgs.synchronizeBeforeExecution = false;
    testArgs.emitSelfCleanup = false;
    testArgs.initializeWparidRegister = true;
    testArgs.emitPipeControlStall = true;
    testArgs.crossTileAtomicSynchronization = true;
}

void WalkerPartitionTests::TearDown() {
    auto initialCommandBufferPointer = cmdBuffer;
    if (checkForProperCmdBufferAddressOffset) {
        EXPECT_EQ(ptrDiff(cmdBufferAddress, initialCommandBufferPointer), totalBytesProgrammed);
    }
}
