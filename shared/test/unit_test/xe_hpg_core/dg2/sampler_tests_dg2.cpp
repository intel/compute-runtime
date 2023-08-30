/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <memory>

using namespace NEO;

using SamplerTestDG2 = ::testing::Test;

HWTEST2_F(SamplerTestDG2, giveDG2G10BelowC0WhenProgrammingSamplerForNearestFilterWithMirrorAddressThenRoundEnableForRDirectionIsEnabled, IsDG2) {
    auto state = FamilyType::cmdInitSamplerState;
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    state.setTcxAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR);
    state.setMinModeFilter(SAMPLER_STATE::MIN_MODE_FILTER_NEAREST);

    MockExecutionEnvironment executionEnvironment{};
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = dg2G10DeviceIds[0];

    uint32_t revisions[] = {REVISION_A0, REVISION_B, REVISION_C};
    for (auto &revision : revisions) {
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);

        state.setRAddressMinFilterRoundingEnable(false);
        state.setRAddressMagFilterRoundingEnable(false);
        productHelper.adjustSamplerState(&state, hwInfo);
        if (revision < REVISION_C) {
            EXPECT_TRUE(state.getRAddressMinFilterRoundingEnable());
            EXPECT_TRUE(state.getRAddressMagFilterRoundingEnable());
        } else {
            EXPECT_FALSE(state.getRAddressMinFilterRoundingEnable());
            EXPECT_FALSE(state.getRAddressMagFilterRoundingEnable());
        }
    }
}

HWTEST2_F(SamplerTestDG2, giveDG2G10A0WhenProgrammingSamplerForNotNearestFilterOrWithoutMirrorAddressThenRoundEnableForRDirectionWAIsNotApplied, IsDG2) {
    auto state = FamilyType::cmdInitSamplerState;
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    MockExecutionEnvironment executionEnvironment{};
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = dg2G10DeviceIds[0];

    hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(REVISION_A0, hwInfo);

    state.setRAddressMinFilterRoundingEnable(false);
    state.setRAddressMagFilterRoundingEnable(false);

    state.setTcxAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_WRAP);
    state.setMinModeFilter(SAMPLER_STATE::MIN_MODE_FILTER_NEAREST);

    productHelper.adjustSamplerState(&state, hwInfo);
    EXPECT_FALSE(state.getRAddressMinFilterRoundingEnable());
    EXPECT_FALSE(state.getRAddressMagFilterRoundingEnable());

    state.setTcxAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR);
    state.setMinModeFilter(SAMPLER_STATE::MIN_MODE_FILTER_LINEAR);

    productHelper.adjustSamplerState(&state, hwInfo);

    EXPECT_FALSE(state.getRAddressMinFilterRoundingEnable());
    EXPECT_FALSE(state.getRAddressMagFilterRoundingEnable());
}

HWTEST2_F(SamplerTestDG2, giveDG2G11BelowB0WhenProgrammingSamplerForNearestFilterWithMirrorAddressThenRoundEnableForRDirectionIsEnabled, IsDG2) {
    auto state = FamilyType::cmdInitSamplerState;
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    state.setTcxAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR);
    state.setMinModeFilter(SAMPLER_STATE::MIN_MODE_FILTER_NEAREST);

    MockExecutionEnvironment executionEnvironment{};
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = dg2G11DeviceIds[0];

    uint32_t revisions[] = {REVISION_A0, REVISION_B, REVISION_C};
    for (auto &revision : revisions) {
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);

        state.setRAddressMinFilterRoundingEnable(false);
        state.setRAddressMagFilterRoundingEnable(false);
        productHelper.adjustSamplerState(&state, hwInfo);
        if (revision < REVISION_B) {
            EXPECT_TRUE(state.getRAddressMinFilterRoundingEnable());
            EXPECT_TRUE(state.getRAddressMagFilterRoundingEnable());
        } else {
            EXPECT_FALSE(state.getRAddressMinFilterRoundingEnable());
            EXPECT_FALSE(state.getRAddressMagFilterRoundingEnable());
        }
    }
}

HWTEST2_F(SamplerTestDG2, giveDG2G12WhenProgrammingSamplerForNearestFilterWithMirrorAddressThenRoundEnableForRDirectionIsDisabled, IsDG2) {
    auto state = FamilyType::cmdInitSamplerState;
    using SAMPLER_STATE = typename FamilyType::SAMPLER_STATE;

    state.setTcxAddressControlMode(SAMPLER_STATE::TEXTURE_COORDINATE_MODE_MIRROR);
    state.setMinModeFilter(SAMPLER_STATE::MIN_MODE_FILTER_NEAREST);

    MockExecutionEnvironment executionEnvironment{};
    auto &productHelper = executionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    auto hwInfo = *defaultHwInfo;
    hwInfo.platform.usDeviceID = dg2G12DeviceIds[0];

    uint32_t revisions[] = {REVISION_A0, REVISION_B, REVISION_C};
    for (auto &revision : revisions) {
        hwInfo.platform.usRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);

        state.setRAddressMinFilterRoundingEnable(false);
        state.setRAddressMagFilterRoundingEnable(false);
        productHelper.adjustSamplerState(&state, hwInfo);
        EXPECT_FALSE(state.getRAddressMinFilterRoundingEnable());
        EXPECT_FALSE(state.getRAddressMagFilterRoundingEnable());
    }
}
