/*
 * Copyright (C) 2018-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using Gen8CoherencyRequirements = ::testing::Test;

GEN8TEST_F(Gen8CoherencyRequirements, WhenMemoryManagerIsInitializedThenNoCoherencyProgramming) {
    auto executionEnvironment = std::make_unique<MockExecutionEnvironment>();
    executionEnvironment->initializeMemoryManager();
    CommandStreamReceiverHw<BDWFamily> csr(*executionEnvironment, 0, 1);
    LinearStream stream;
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();

    auto retSize = csr.getCmdSizeForComputeMode();
    EXPECT_EQ(0u, retSize);
    csr.programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(0u, stream.getUsed());

    flags.requiresCoherency = true;
    retSize = csr.getCmdSizeForComputeMode();
    EXPECT_EQ(0u, retSize);
    csr.programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(0u, stream.getUsed());
}
