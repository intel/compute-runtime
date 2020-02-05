/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/execution_environment/execution_environment.h"
#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/platform/platform.h"
#include "test.h"
#include "unit_tests/helpers/dispatch_flags_helper.h"

using namespace NEO;

typedef ::testing::Test Gen8CoherencyRequirements;

GEN8TEST_F(Gen8CoherencyRequirements, noCoherencyProgramming) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    CommandStreamReceiverHw<BDWFamily> csr(*executionEnvironment, 0);
    LinearStream stream;
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();

    auto retSize = csr.getCmdSizeForComputeMode();
    EXPECT_EQ(0u, retSize);
    csr.programComputeMode(stream, flags);
    EXPECT_EQ(0u, stream.getUsed());

    flags.requiresCoherency = true;
    retSize = csr.getCmdSizeForComputeMode();
    EXPECT_EQ(0u, retSize);
    csr.programComputeMode(stream, flags);
    EXPECT_EQ(0u, stream.getUsed());
}
