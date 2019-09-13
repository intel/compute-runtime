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
#include "unit_tests/helpers/dispatch_flags_helper.h"

using namespace NEO;

typedef ::testing::Test Gen9CoherencyRequirements;

GEN9TEST_F(Gen9CoherencyRequirements, noCoherencyProgramming) {
    ExecutionEnvironment *executionEnvironment = platformImpl->peekExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    CommandStreamReceiverHw<SKLFamily> csr(*executionEnvironment);
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
