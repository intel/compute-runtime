/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "opencl/test/unit_test/command_stream/compute_mode_tests.h"

TGLLPTEST_F(ComputeModeRequirements, givenCsrRequestFlagsWithSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + 3 * sizeof(PIPE_CONTROL) + 2 * sizeof(PIPELINE_SELECT);
    char buff[1024];
    LinearStream stream(buff, 1024);

    overrideComputeModeRequest<FamilyType>(false, false, true);

    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(false, true, true);

    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(true, true, true);

    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(true, false, true);

    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(false, false, true, true, 127u);

    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags);
    EXPECT_EQ(cmdsSize, stream.getUsed());
}

TGLLPTEST_F(ComputeModeRequirements, givenCsrRequestFlagsWithoutSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + 2 * sizeof(PIPE_CONTROL) + 2 * sizeof(PIPELINE_SELECT);
    char buff[1024];
    LinearStream stream(buff, 1024);

    overrideComputeModeRequest<FamilyType>(false, false, false);

    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(0u, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags);
    EXPECT_EQ(0u, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(true, true, false);

    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(true, false, false);

    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(false, false, false, true, 127u);

    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags);
    EXPECT_EQ(cmdsSize, stream.getUsed());
}
