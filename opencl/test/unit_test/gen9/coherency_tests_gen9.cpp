/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "opencl/source/platform/platform.h"
#include "opencl/test/unit_test/helpers/dispatch_flags_helper.h"
#include "test.h"

using namespace NEO;

typedef ::testing::Test Gen9CoherencyRequirements;

GEN9TEST_F(Gen9CoherencyRequirements, noCoherencyProgramming) {
    ExecutionEnvironment *executionEnvironment = platform()->peekExecutionEnvironment();
    executionEnvironment->initializeMemoryManager();
    CommandStreamReceiverHw<SKLFamily> csr(*executionEnvironment, 0);
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
