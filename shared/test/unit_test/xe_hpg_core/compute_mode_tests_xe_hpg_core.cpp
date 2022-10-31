/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/unit_test/command_stream/compute_mode_tests.h"

using namespace NEO;

using ComputeModeRequirementsXeHpgCore = ComputeModeRequirements;

XE_HPG_CORETEST_F(ComputeModeRequirementsXeHpgCore, GivenVariousSettingsWhenComputeModeIsProgrammedThenThreadLimitsAreCorrectlySet) {
    DebugManagerStateRestore restorer;
    setUpImpl<FamilyType>();

    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    const auto &hwInfoConfig = *HwInfoConfig::get(productFamily);
    const auto &[isBasicWARequired, isExtendedWARequired] = hwInfoConfig.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, csr->isRcs());
    std::ignore = isExtendedWARequired;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    if (isBasicWARequired) {
        cmdsSize += +sizeof(PIPE_CONTROL);
    }

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 128u);

    auto defaultScmCmd = FamilyType::cmdInitStateComputeMode;
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, defaultScmCmd.getZPassAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED, defaultScmCmd.getPixelAsyncComputeThreadLimit());

    char buff[1024];
    auto pCsr = getCsrHw<FamilyType>();
    struct {
        typename STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT zPassThreadLimit;
        typename STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT pixelThreadLimit;
    } testValues[] = {
        {STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED},
        {STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_2},
        {STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_8},
        {STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_16},
        {STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_24},
        {STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_32},
        {STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_40},
        {STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_MAX_48},
        {STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_64, STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED},
        {STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_56, STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED},
        {STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_48, STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED},
    };

    for (auto testValue : testValues) {
        DebugManager.flags.ForceZPassAsyncComputeThreadLimit.set(testValue.zPassThreadLimit);
        DebugManager.flags.ForcePixelAsyncComputeThreadLimit.set(testValue.pixelThreadLimit);

        pCsr->streamProperties.stateComputeMode = {};
        pCsr->streamProperties.stateComputeMode.setProperties(false, 0u, 0u, PreemptionMode::Disabled, *defaultHwInfo);
        LinearStream stream(buff, 1024);
        pCsr->programComputeMode(stream, flags, *defaultHwInfo);
        EXPECT_EQ(cmdsSize, stream.getUsed());

        auto pScmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
        if (isBasicWARequired) {
            pScmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), sizeof(PIPE_CONTROL)));
        }
        EXPECT_EQ(testValue.zPassThreadLimit, pScmCmd->getZPassAsyncComputeThreadLimit());
        EXPECT_EQ(testValue.pixelThreadLimit, pScmCmd->getPixelAsyncComputeThreadLimit());

        auto expectedFields = FamilyType::stateComputeModeZPassAsyncComputeThreadLimitMask |
                              FamilyType::stateComputeModePixelAsyncComputeThreadLimitMask;
        EXPECT_TRUE(isValueSet(pScmCmd->getMaskBits(), expectedFields));
    }

    DebugManager.flags.ForceZPassAsyncComputeThreadLimit.set(-1);
    DebugManager.flags.ForcePixelAsyncComputeThreadLimit.set(-1);

    pCsr->streamProperties.stateComputeMode = {};
    pCsr->streamProperties.stateComputeMode.setProperties(false, 0u, 0u, PreemptionMode::Disabled, *defaultHwInfo);
    LinearStream stream(buff, 1024);
    pCsr->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto pScmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    if (isBasicWARequired) {
        pScmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), sizeof(PIPE_CONTROL)));
    }
    EXPECT_EQ(STATE_COMPUTE_MODE::Z_PASS_ASYNC_COMPUTE_THREAD_LIMIT_MAX_60, pScmCmd->getZPassAsyncComputeThreadLimit());
    EXPECT_EQ(STATE_COMPUTE_MODE::PIXEL_ASYNC_COMPUTE_THREAD_LIMIT_DISABLED, pScmCmd->getPixelAsyncComputeThreadLimit());
    EXPECT_FALSE(isValueSet(pScmCmd->getMaskBits(), FamilyType::stateComputeModeZPassAsyncComputeThreadLimitMask));
    EXPECT_FALSE(isValueSet(pScmCmd->getMaskBits(), FamilyType::stateComputeModePixelAsyncComputeThreadLimitMask));
}

XE_HPG_CORETEST_F(ComputeModeRequirementsXeHpgCore, givenComputeModeCmdSizeWhenLargeGrfModeChangeIsRequiredThenSCMCommandSizeIsCalculated) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    overrideComputeModeRequest<FamilyType>(false, false, false, false, 128u);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    auto cmdSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 256u);
    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdSize, retSize);

    overrideComputeModeRequest<FamilyType>(true, false, false, true, 256u);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdSize, retSize);
}

XE_HPG_CORETEST_F(ComputeModeRequirementsXeHpgCore, givenCoherencyWithSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    overrideComputeModeRequest<FamilyType>(false, false, true);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    overrideComputeModeRequest<FamilyType>(false, true, true);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + (2 * sizeof(PIPE_CONTROL));

    overrideComputeModeRequest<FamilyType>(true, true, true);
    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);

    overrideComputeModeRequest<FamilyType>(true, false, true);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
}

XE_HPG_CORETEST_F(ComputeModeRequirementsXeHpgCore, givenCoherencyWithoutSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    setUpImpl<FamilyType>();

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);

    overrideComputeModeRequest<FamilyType>(false, false, false, false);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    overrideComputeModeRequest<FamilyType>(false, false, false, true);
    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
}
