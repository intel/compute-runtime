/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/execution_environment/execution_environment.h"
#include "runtime/platform/platform.h"
#include "test.h"

using namespace NEO;

typedef ::testing::Test Gen8CoherencyRequirements;

GEN8TEST_F(Gen8CoherencyRequirements, noCoherencyProgramming) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    CommandStreamReceiverHw<BDWFamily> csr(*executionEnvironment);
    LinearStream stream;
    DispatchFlags flags = {};

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
