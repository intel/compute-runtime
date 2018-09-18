/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/command_stream_receiver_hw.h"
#include "runtime/execution_environment/execution_environment.h"
#include "test.h"

using namespace OCLRT;

typedef ::testing::Test Gen9CoherencyRequirements;

GEN9TEST_F(Gen9CoherencyRequirements, noCoherencyProgramming) {
    ExecutionEnvironment executionEnvironment;
    CommandStreamReceiverHw<SKLFamily> csr(*platformDevices[0], executionEnvironment);
    LinearStream stream;
    DispatchFlags flags = {};

    auto retSize = csr.getCmdSizeForCoherency();
    EXPECT_EQ(0u, retSize);
    csr.programCoherency(stream, flags);
    EXPECT_EQ(0u, stream.getUsed());

    flags.requiresCoherency = true;
    retSize = csr.getCmdSizeForCoherency();
    EXPECT_EQ(0u, retSize);
    csr.programCoherency(stream, flags);
    EXPECT_EQ(0u, stream.getUsed());
}
