/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/command_stream/compute_mode_tests.h"

HWTEST2_F(ComputeModeRequirements, givenCsrRequestFlagsWithSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned, IsTGLLP) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + 3 * sizeof(PIPE_CONTROL) + 2 * sizeof(PIPELINE_SELECT);
    char buff[1024];
    LinearStream stream(buff, 1024);

    overrideComputeModeRequest<FamilyType>(false, false, true);

    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(0u, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(false, true, true);

    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(0u, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(true, true, true);

    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(true, false, true);

    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(false, false, true, true, 127u);

    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());
}

HWTEST2_F(ComputeModeRequirements, givenCsrRequestFlagsWithoutSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned, IsTGLLP) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + 2 * sizeof(PIPE_CONTROL) + 2 * sizeof(PIPELINE_SELECT);
    char buff[1024];
    LinearStream stream(buff, 1024);

    overrideComputeModeRequest<FamilyType>(false, false, false);

    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(0u, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(true, true, false);

    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(true, false, false);

    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(false, false, false, true, 127u);

    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());
}

HWTEST2_F(ComputeModeRequirements, givenCsrRequestOnEngineCCSWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned, IsTGLLP) {
    auto hwInfo = *defaultHwInfo;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_CCS;

    SetUpImpl<FamilyType>(&hwInfo);
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024];
    LinearStream stream(buff, 1024);

    overrideComputeModeRequest<FamilyType>(false, false, false);

    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(0u, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(true, true, false);

    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(true, false, false);

    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    stream.replaceBuffer(buff, 1024);
    overrideComputeModeRequest<FamilyType>(false, false, false, true, 127u);

    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());
}
