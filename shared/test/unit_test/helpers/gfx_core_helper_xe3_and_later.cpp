/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include <array>
#include <utility>

using namespace NEO;

using GfxCoreHelperXe3AndLaterTests = GfxCoreHelperTest;

HWTEST2_F(GfxCoreHelperXe3AndLaterTests, givenVariousValuesAndXe3AndLaterPlatformsWhenCallingCalculateAvailableThreadCountAndThreadCountAvailableIsBiggerThenCorrectValueIsReturned, IsAtLeastXe3Core) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    std::array<std::pair<uint32_t, uint32_t>, 7> grfTestInputs = {{{64, 10},
                                                                   {96, 10},
                                                                   {128, 8},
                                                                   {160, 6},
                                                                   {192, 5},
                                                                   {256, 4},
                                                                   {512, 2}}};

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    for (const auto &[grfCount, expectedThreadCountPerEu] : grfTestInputs) {
        auto calculatedThreadCount = expectedThreadCountPerEu * hardwareInfo.gtSystemInfo.EUCount;
        // force always bigger Thread Count available
        hardwareInfo.gtSystemInfo.ThreadCount = calculatedThreadCount * 2;
        auto result = gfxCoreHelper.calculateAvailableThreadCount(hardwareInfo, grfCount, rootDeviceEnvironment);
        EXPECT_EQ(calculatedThreadCount, result);
    }
}

HWTEST2_F(GfxCoreHelperXe3AndLaterTests, givenVariousValuesAndXe3AndLaterPlatformsWhenCallingCalculateAvailableThreadCountAndThreadCountAvailableIsSmallerThenCorrectValueIsReturned, IsAtLeastXe3Core) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    std::array<std::pair<uint32_t, uint32_t>, 7> grfTestInputs = {{{64, 10},
                                                                   {96, 10},
                                                                   {128, 8},
                                                                   {160, 6},
                                                                   {192, 5},
                                                                   {256, 4},
                                                                   {512, 2}}};

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    for (const auto &[grfCount, expectedThreadCountPerEu] : grfTestInputs) {
        auto calculatedThreadCount = expectedThreadCountPerEu * hardwareInfo.gtSystemInfo.EUCount;
        // force thread count smaller than calculation
        hardwareInfo.gtSystemInfo.ThreadCount = calculatedThreadCount / 2;
        auto result = gfxCoreHelper.calculateAvailableThreadCount(hardwareInfo, grfCount, rootDeviceEnvironment);
        EXPECT_EQ(hardwareInfo.gtSystemInfo.ThreadCount, result);
    }
}

HWTEST2_F(GfxCoreHelperXe3AndLaterTests, GivenModifiedGtSystemInfoAndXe3AndLaterPlatformsWhenCallingCalculateAvailableThreadCountThenCorrectValueIsReturned, IsAtLeastXe3Core) {
    std::array<std::pair<uint32_t, uint32_t>, 3> testInputs = {{{64, 256},
                                                                {96, 384},
                                                                {128, 512}}};
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto hwInfo = hardwareInfo;
    for (const auto &[euCount, expectedThreadCount] : testInputs) {
        hwInfo.gtSystemInfo.EUCount = euCount;
        auto result = gfxCoreHelper.calculateAvailableThreadCount(hwInfo, 256, *mockExecutionEnvironment.rootDeviceEnvironments[0]);
        EXPECT_EQ(expectedThreadCount, result);
    }
}
