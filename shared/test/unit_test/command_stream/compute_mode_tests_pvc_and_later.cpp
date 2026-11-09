/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/command_stream/compute_mode_tests.h"

using namespace NEO;

using ComputeModeRequirementsPvcAndLater = ComputeModeRequirements;

HWTEST2_F(ComputeModeRequirementsPvcAndLater, givenComputeModeCmdSizeWhenLargeGrfModeChangeIsRequiredThenSCMAndPcCommandsSizeIsCalculated, IsXeHpcCore) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdSize = sizeof(STATE_COMPUTE_MODE);

    overrideComputeModeRequest<FamilyType>(false, false, false, false, 128u);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 256u);
    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdSize, retSize);

    overrideComputeModeRequest<FamilyType>(true, false, false, true, 256u);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdSize, retSize);
}

HWTEST2_F(ComputeModeRequirementsPvcAndLater, givenCoherencyWithoutSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned, IsWithinXeHpcCoreAndXe2HpgCore) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    setUpImpl<FamilyType>();

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);

    overrideComputeModeRequest<FamilyType>(false, false, false, false);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    overrideComputeModeRequest<FamilyType>(false, false, false, true);
    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
}

HWTEST2_F(ComputeModeRequirementsPvcAndLater, givenNumGrfRequiredChangedWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned, IsWithinXeHpcCoreAndXe2HpgCore) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    setUpImpl<FamilyType>();

    auto numGrfRequired = 128u;
    auto numGrfRequiredChanged = false;
    overrideComputeModeRequest<FamilyType>(false, false, false, numGrfRequiredChanged, numGrfRequired);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    numGrfRequiredChanged = true;
    overrideComputeModeRequest<FamilyType>(false, false, false, numGrfRequiredChanged, numGrfRequired);
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(sizeof(STATE_COMPUTE_MODE), getCsrHw<FamilyType>()->getCmdSizeForComputeMode());
}

HWTEST2_F(ComputeModeRequirementsPvcAndLater, givenComputeModeProgrammingWhenLargeGrfModeDoesntChangeButRequiredThreadArbitrationPolicyIsNewThenSCMIsReloaded, IsWithinXeHpcCoreAndXe2HpgCore) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    char buff[1024];
    LinearStream stream(buff, 1024);
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);

    overrideComputeModeRequest<FamilyType>(false, false, false, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());
}

HWTEST2_F(ComputeModeRequirementsPvcAndLater, givenComputeModeProgrammingWhenLargeGrfModeChangedThenSCMIsReloadedAndLargeGrfModeProgrammed, IsWithinXeHpcCoreAndXe2HpgCore) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    char buff[1024];
    LinearStream stream(buff, 1024);
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    uint32_t numGrfRequired = GrfConfig::largeGrfNumber;

    overrideComputeModeRequest<FamilyType>(false, false, false, true, numGrfRequired);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());
    EXPECT_TRUE(static_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase())->getLargeGrfMode());
}

HWTEST2_F(ComputeModeRequirementsPvcAndLater, givenComputeModeProgrammingWhenLargeGrfRequiredChangedButValueIsDefaultThenSCMIsReloadedButLargeGrfModeNotProgrammed, IsWithinXeHpcCoreAndXe2HpgCore) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    char buff[1024];
    LinearStream stream(buff, 1024);
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    uint32_t numGrfRequired = GrfConfig::defaultGrfNumber;

    overrideComputeModeRequest<FamilyType>(false, false, false, true, numGrfRequired);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());
    EXPECT_FALSE(static_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase())->getLargeGrfMode());
}
