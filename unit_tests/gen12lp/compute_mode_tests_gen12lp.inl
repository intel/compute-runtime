/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "unit_tests/command_stream/compute_mode_tests.h"

TGLLPTEST_F(ComputeModeRequirements, givenCsrRequestFlagsWithSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);
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
    overrideComputeModeRequest<FamilyType>(false, false, true, true, 127u, false);

    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags);
    EXPECT_EQ(cmdsSize, stream.getUsed());
}

TGLLPTEST_F(ComputeModeRequirements, givenCsrRequestFlagsWithoutSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
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
    overrideComputeModeRequest<FamilyType>(false, false, false, true, 127u, false);

    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags);
    EXPECT_EQ(cmdsSize, stream.getUsed());
}
