/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/common_types.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using GfxCoreHelperXe3pAndLaterTests = ::testing::Test;
HWTEST2_F(GfxCoreHelperXe3pAndLaterTests, givenInvalidationOrL1FlushWhenSetExtraPropertiesThenDrainAllQueues, IsAtLeastXe3pCore) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    MockExecutionEnvironment mockExecutionEnvironment{};

    PIPE_CONTROL pipeControl = FamilyType::cmdInitPipeControl;
    EXPECT_TRUE(pipeControl.getQueueDrainMode());

    PipeControlArgs args = {};
    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(&pipeControl, args);
    EXPECT_TRUE(pipeControl.getQueueDrainMode());

    args = {};
    args.instructionCacheInvalidateEnable = true;
    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(&pipeControl, args);
    EXPECT_FALSE(pipeControl.getQueueDrainMode());
    pipeControl.setQueueDrainMode(true);

    args = {};
    args.stateCacheInvalidationEnable = true;
    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(&pipeControl, args);
    EXPECT_FALSE(pipeControl.getQueueDrainMode());
    pipeControl.setQueueDrainMode(true);

    args = {};
    args.textureCacheInvalidationEnable = true;
    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(&pipeControl, args);
    EXPECT_FALSE(pipeControl.getQueueDrainMode());
    pipeControl.setQueueDrainMode(true);

    args = {};
    args.constantCacheInvalidationEnable = true;
    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(&pipeControl, args);
    EXPECT_FALSE(pipeControl.getQueueDrainMode());
    pipeControl.setQueueDrainMode(true);

    args = {};
    args.hdcPipelineFlush = true;
    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(&pipeControl, args);
    EXPECT_FALSE(pipeControl.getQueueDrainMode());
    pipeControl.setQueueDrainMode(true);

    args = {};
    args.unTypedDataPortCacheFlush = true;
    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(&pipeControl, args);
    EXPECT_FALSE(pipeControl.getQueueDrainMode());
}
