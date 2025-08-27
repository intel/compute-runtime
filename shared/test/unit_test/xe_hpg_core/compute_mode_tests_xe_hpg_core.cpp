/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/source/xe_hpg_core/hw_info_xe_hpg_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/command_stream/compute_mode_tests.h"

using namespace NEO;

using ComputeModeRequirementsXeHpgCore = ComputeModeRequirements;

XE_HPG_CORETEST_F(ComputeModeRequirementsXeHpgCore, GivenVariousSettingsWhenComputeModeIsProgrammedThenThreadLimitsAreCorrectlySet) {
    DebugManagerStateRestore restorer;
    setUpImpl<FamilyType>();

    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    const auto &productHelper = this->device->getProductHelper();
    auto *releaseHelper = device->getReleaseHelper();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, csr->isRcs(), releaseHelper);
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
    auto &rootDeviceEnvironment = device->getRootDeviceEnvironment();
    for (auto testValue : testValues) {
        debugManager.flags.ForceZPassAsyncComputeThreadLimit.set(testValue.zPassThreadLimit);
        debugManager.flags.ForcePixelAsyncComputeThreadLimit.set(testValue.pixelThreadLimit);

        pCsr->streamProperties.stateComputeMode = {};
        pCsr->streamProperties.stateComputeMode.initSupport(rootDeviceEnvironment);
        pCsr->streamProperties.stateComputeMode.setPropertiesAll(false, 0u, 0u, PreemptionMode::Disabled, false);
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

    debugManager.flags.ForceZPassAsyncComputeThreadLimit.set(-1);
    debugManager.flags.ForcePixelAsyncComputeThreadLimit.set(-1);

    pCsr->streamProperties.stateComputeMode = {};
    pCsr->streamProperties.stateComputeMode.initSupport(rootDeviceEnvironment);
    pCsr->streamProperties.stateComputeMode.setPropertiesAll(false, 0u, 0u, PreemptionMode::Disabled, false);
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

    auto expSize = sizeof(STATE_COMPUTE_MODE);
    auto &rootDeviceEnvironment = csr->peekRootDeviceEnvironment();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto *releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, getCsrHw<FamilyType>()->isRcs(), releaseHelper);
    std::ignore = isExtendedWARequired;

    if (isBasicWARequired) {
        expSize += sizeof(PIPE_CONTROL);
    }

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 256u);
    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(expSize, retSize);

    overrideComputeModeRequest<FamilyType>(true, false, false, true, 256u);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(expSize, retSize);
}

XE_HPG_CORETEST_F(ComputeModeRequirementsXeHpgCore, givenCoherencyWithSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    overrideComputeModeRequest<FamilyType>(false, false, true);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    overrideComputeModeRequest<FamilyType>(false, true, true);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    auto expSize = sizeof(STATE_COMPUTE_MODE) + (sizeof(PIPE_CONTROL));
    auto &rootDeviceEnvironment = csr->peekRootDeviceEnvironment();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto *releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, getCsrHw<FamilyType>()->isRcs(), releaseHelper);
    std::ignore = isExtendedWARequired;

    if (isBasicWARequired) {
        expSize += sizeof(PIPE_CONTROL);
    }

    overrideComputeModeRequest<FamilyType>(true, true, true);
    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(expSize, retSize);

    overrideComputeModeRequest<FamilyType>(true, false, true);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(expSize, retSize);
}

XE_HPG_CORETEST_F(ComputeModeRequirementsXeHpgCore, givenCoherencyWithoutSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    setUpImpl<FamilyType>();
    overrideComputeModeRequest<FamilyType>(false, false, false, false);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    overrideComputeModeRequest<FamilyType>(false, false, false, true);

    auto expSize = sizeof(STATE_COMPUTE_MODE);
    auto &rootDeviceEnvironment = csr->peekRootDeviceEnvironment();
    auto &productHelper = rootDeviceEnvironment.getHelper<ProductHelper>();
    auto *releaseHelper = rootDeviceEnvironment.getReleaseHelper();
    auto &hwInfo = *rootDeviceEnvironment.getHardwareInfo();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(hwInfo, getCsrHw<FamilyType>()->isRcs(), releaseHelper);
    std::ignore = isExtendedWARequired;

    if (isBasicWARequired) {
        expSize += sizeof(PIPE_CONTROL);
    }

    auto gotSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(expSize, gotSize);
}
