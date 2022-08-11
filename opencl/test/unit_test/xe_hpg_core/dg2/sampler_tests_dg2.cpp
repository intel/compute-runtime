/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/utilities/base_object_utils.h"

#include "opencl/source/sampler/sampler.h"
#include "opencl/test/unit_test/fixtures/cl_device_fixture.h"
#include "opencl/test/unit_test/mocks/mock_context.h"

#include <memory>

using namespace NEO;

using SamplerTest = Test<ClDeviceFixture>;

HWTEST2_F(SamplerTest, givenDg2SamplerWhenUsingDefaultFilteringAndAppendSamplerStateParamsThenNotEnableLowQualityFilter, IsDG2) {
    EXPECT_FALSE(DebugManager.flags.ForceSamplerLowFilteringPrecision.get());
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;

    auto state = FamilyType::cmdInitSamplerState;
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
    HwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->adjustSamplerState(&state, *defaultHwInfo);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
}

HWTEST2_F(SamplerTest, givenDg2SamplerWhenForcingLowQualityFilteringAndAppendSamplerStateParamsThenEnableLowQualityFilter, IsDG2) {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForceSamplerLowFilteringPrecision.set(true);
    EXPECT_TRUE(DebugManager.flags.ForceSamplerLowFilteringPrecision.get());
    typedef typename FamilyType::SAMPLER_STATE SAMPLER_STATE;
    auto state = FamilyType::cmdInitSamplerState;
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_DISABLE, state.getLowQualityFilter());
    HwInfoConfig::get(defaultHwInfo->platform.eProductFamily)->adjustSamplerState(&state, *defaultHwInfo);
    EXPECT_EQ(SAMPLER_STATE::LOW_QUALITY_FILTER_ENABLE, state.getLowQualityFilter());
}

HWTEST2_F(SamplerTest, givenDg2BelowC0WhenProgrammingSamplerForNearestFilterWithMirrorAddressThenRoundEnableForRDirectionIsEnabled, IsDG2) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    uint32_t revisions[] = {REVISION_A0, REVISION_B, REVISION_C};
    for (auto &revision : revisions) {
        pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(revision, *defaultHwInfo);
        auto context = clUniquePtr(new MockContext());
        auto sampler = clUniquePtr(new SamplerHw<FamilyType>(context.get(), CL_FALSE, CL_ADDRESS_MIRRORED_REPEAT, CL_FILTER_NEAREST));
        auto state = FamilyType::cmdInitSamplerState;
        EXPECT_FALSE(state.getRAddressMinFilterRoundingEnable());
        EXPECT_FALSE(state.getRAddressMagFilterRoundingEnable());
        sampler->setArg(&state, pDevice->getHardwareInfo());
        if (REVISION_C == revision) {
            EXPECT_FALSE(state.getRAddressMinFilterRoundingEnable());
            EXPECT_FALSE(state.getRAddressMagFilterRoundingEnable());
        } else {
            EXPECT_TRUE(state.getRAddressMinFilterRoundingEnable());
            EXPECT_TRUE(state.getRAddressMagFilterRoundingEnable());
        }
    }
}

HWTEST2_F(SamplerTest, givenDg2BelowC0WhenProgrammingSamplerForNearestFilterWitouthMirrorAddressThenRoundEnableForRDirectionIsDisabled, IsDG2) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    uint32_t revisions[] = {REVISION_A0, REVISION_B, REVISION_C};
    for (auto &revision : revisions) {
        pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(revision, *defaultHwInfo);
        auto context = clUniquePtr(new MockContext());
        auto sampler = clUniquePtr(new SamplerHw<FamilyType>(context.get(), CL_FALSE, CL_ADDRESS_NONE, CL_FILTER_NEAREST));
        auto state = FamilyType::cmdInitSamplerState;
        EXPECT_FALSE(state.getRAddressMinFilterRoundingEnable());
        EXPECT_FALSE(state.getRAddressMagFilterRoundingEnable());
        sampler->setArg(&state, pDevice->getHardwareInfo());
        EXPECT_FALSE(state.getRAddressMinFilterRoundingEnable());
        EXPECT_FALSE(state.getRAddressMagFilterRoundingEnable());
    }
}

HWTEST2_F(SamplerTest, givenDg2BelowC0WhenProgrammingSamplerForLinearFilterWithMirrorAddressThenRoundEnableForRDirectionIsEnabled, IsDG2) {
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    const auto &hwInfoConfig = *HwInfoConfig::get(defaultHwInfo->platform.eProductFamily);

    uint32_t revisions[] = {REVISION_A0, REVISION_B, REVISION_C};
    for (auto &revision : revisions) {
        pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->platform.usRevId = hwInfoConfig.getHwRevIdFromStepping(revision, *defaultHwInfo);
        auto context = clUniquePtr(new MockContext());
        auto sampler = clUniquePtr(new SamplerHw<FamilyType>(context.get(), CL_FALSE, CL_ADDRESS_MIRRORED_REPEAT, CL_FILTER_LINEAR));
        auto state = FamilyType::cmdInitSamplerState;
        EXPECT_FALSE(state.getRAddressMinFilterRoundingEnable());
        EXPECT_FALSE(state.getRAddressMagFilterRoundingEnable());
        sampler->setArg(&state, pDevice->getHardwareInfo());
        EXPECT_TRUE(state.getRAddressMinFilterRoundingEnable());
        EXPECT_TRUE(state.getRAddressMagFilterRoundingEnable());
    }
}
