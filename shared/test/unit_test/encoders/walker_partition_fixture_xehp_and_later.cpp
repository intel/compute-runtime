/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/encoders/walker_partition_fixture_xehp_and_later.h"

#include "shared/test/common/helpers/default_hw_info.h"

void WalkerPartitionTests::SetUp() {
    testHardwareInfo = *defaultHwInfo;

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
