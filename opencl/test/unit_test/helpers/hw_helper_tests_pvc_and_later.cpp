/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/helpers/mock_hw_info_config_hw.h"
#include "shared/test/common/test_macros/hw_test.h"

using HwHelperTestPvcAndLater = HwHelperTest;

HWTEST2_F(HwHelperTestPvcAndLater, givenVariousCachesRequestsThenProperMocsIndexesAreBeingReturned, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore;

    auto &helper = HwHelper::get(renderCoreFamily);
    auto gmmHelper = this->pDevice->getRootDeviceEnvironment().getGmmHelper();
    auto expectedMocsForL3off = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED) >> 1;
    auto expectedMocsForL3on = gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER) >> 1;

    auto mocsIndex = helper.getMocsIndex(*gmmHelper, false, true);
    EXPECT_EQ(expectedMocsForL3off, mocsIndex);

    mocsIndex = helper.getMocsIndex(*gmmHelper, true, false);
    EXPECT_EQ(expectedMocsForL3on, mocsIndex);

    mocsIndex = helper.getMocsIndex(*gmmHelper, true, true);
    EXPECT_EQ(expectedMocsForL3on, mocsIndex);
}

HWTEST2_F(HwHelperTestPvcAndLater, givenRenderEngineWhenRemapCalledThenUseCccs, IsAtLeastXeHpcCore) {
    hardwareInfo.featureTable.flags.ftrCCSNode = false;

    auto &helper = HwHelper::get(renderCoreFamily);
    helper.adjustDefaultEngineType(&hardwareInfo);

    EXPECT_EQ(aub_stream::EngineType::ENGINE_CCCS, EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, hardwareInfo));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_CCCS, EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_CCCS, hardwareInfo));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_CCS, EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_CCS, hardwareInfo));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_BCS, hardwareInfo));
}

HWTEST2_F(HwHelperTestPvcAndLater, GivenVariousValuesAndPvcAndLaterPlatformsWhenCallingCalculateAvailableThreadCountThenCorrectValueIsReturned, IsAtLeastXeHpcCore) {
    std::array<std::pair<uint32_t, uint32_t>, 6> grfTestInputs = {{{64, 16},
                                                                   {96, 10},
                                                                   {128, 8},
                                                                   {160, 6},
                                                                   {192, 5},
                                                                   {256, 4}}};
    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    for (const auto &[grfCount, expectedThreadCountPerEu] : grfTestInputs) {
        auto expected = expectedThreadCountPerEu * hardwareInfo.gtSystemInfo.EUCount;
        auto result = hwHelper.calculateAvailableThreadCount(hardwareInfo, grfCount);
        EXPECT_EQ(expected, result);
    }
}

HWTEST2_F(HwHelperTestPvcAndLater, GivenModifiedGtSystemInfoAndPvcAndLaterPlatformsWhenCallingCalculateAvailableThreadCountThenCorrectValueIsReturned, IsAtLeastXeHpcCore) {
    std::array<std::pair<uint32_t, uint32_t>, 3> testInputs = {{{64, 256},
                                                                {96, 384},
                                                                {128, 512}}};
    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    auto hwInfo = hardwareInfo;
    for (const auto &[euCount, expectedThreadCount] : testInputs) {
        hwInfo.gtSystemInfo.EUCount = euCount;
        auto result = hwHelper.calculateAvailableThreadCount(hwInfo, 256);
        EXPECT_EQ(expectedThreadCount, result);
    }
}

HWTEST2_F(HwHelperTestPvcAndLater, givenHwHelperWhenCheckIsUpdateTaskCountFromWaitSupportedThenReturnsTrue, IsAtLeastXeHpcCore) {
    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);

    EXPECT_TRUE(hwHelper.isUpdateTaskCountFromWaitSupported());
}

HWTEST2_F(HwHelperTestPvcAndLater, givenComputeEngineAndCooperativeUsageWhenGetEngineGroupTypeIsCalledThenCooperativeComputeGroupTypeIsReturned, IsAtLeastXeHpcCore) {
    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    auto hwInfo = *::defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;
    aub_stream::EngineType engineTypes[] = {aub_stream::EngineType::ENGINE_CCS, aub_stream::EngineType::ENGINE_CCS1,
                                            aub_stream::EngineType::ENGINE_CCS2, aub_stream::EngineType::ENGINE_CCS3};
    EngineUsage engineUsages[] = {EngineUsage::Regular, EngineUsage::LowPriority, EngineUsage::Internal, EngineUsage::Cooperative};

    for (auto engineType : engineTypes) {
        for (auto engineUsage : engineUsages) {
            if (engineUsage == EngineUsage::Cooperative) {
                EXPECT_EQ(EngineGroupType::CooperativeCompute, hwHelper.getEngineGroupType(engineType, engineUsage, hwInfo));
            } else {
                EXPECT_EQ(EngineGroupType::Compute, hwHelper.getEngineGroupType(engineType, engineUsage, hwInfo));
            }
        }
    }
}

HWTEST2_F(HwHelperTestPvcAndLater, givenPVCAndLaterPlatformWhenCheckingIfEngineTypeRemappingIsRequiredThenReturnTrue, IsAtLeastXeHpcCore) {
    const auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    EXPECT_TRUE(hwHelper.isEngineTypeRemappingToHwSpecificRequired());
}

HWTEST2_F(HwHelperTestPvcAndLater, WhenIsRcsAvailableIsCalledThenCorrectValueIsReturned, IsAtLeastXeHpcCore) {
    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    auto hwInfo = *::defaultHwInfo;
    aub_stream::EngineType defaultEngineTypes[] = {aub_stream::EngineType::ENGINE_RCS, aub_stream::EngineType::ENGINE_CCCS,
                                                   aub_stream::EngineType::ENGINE_BCS, aub_stream::EngineType::ENGINE_BCS2,
                                                   aub_stream::EngineType::ENGINE_CCS, aub_stream::EngineType::ENGINE_CCS2};
    for (auto defaultEngineType : defaultEngineTypes) {
        hwInfo.capabilityTable.defaultEngineType = defaultEngineType;
        for (auto ftrRcsNode : ::testing::Bool()) {
            hwInfo.featureTable.flags.ftrRcsNode = ftrRcsNode;
            if (ftrRcsNode ||
                (defaultEngineType == aub_stream::EngineType::ENGINE_RCS) ||
                (defaultEngineType == aub_stream::EngineType::ENGINE_CCCS)) {
                EXPECT_TRUE(hwHelper.isRcsAvailable(hwInfo));
            } else {
                EXPECT_FALSE(hwHelper.isRcsAvailable(hwInfo));
            }
        }
    }
}

HWTEST2_F(HwHelperTestPvcAndLater, WhenIsCooperativeDispatchSupportedThenCorrectValueIsReturned, IsAtLeastXeHpcCore) {
    struct MockHwHelper : NEO::HwHelperHw<FamilyType> {
        bool isRcsAvailable(const HardwareInfo &hwInfo) const override {
            return isRcsAvailableValue;
        }
        bool isRcsAvailableValue = true;
    };

    MockHwInfoConfigHw<productFamily> hwInfoConfig;
    auto hwInfo = *::defaultHwInfo;
    VariableBackup<HwInfoConfig *> hwInfoConfigFactoryBackup{&NEO::hwInfoConfigFactory[static_cast<size_t>(hwInfo.platform.eProductFamily)]};
    hwInfoConfigFactoryBackup = &hwInfoConfig;

    MockHwHelper hwHelper{};

    for (auto isCooperativeEngineSupported : ::testing::Bool()) {
        hwInfoConfig.isCooperativeEngineSupportedValue = isCooperativeEngineSupported;
        for (auto isRcsAvailable : ::testing::Bool()) {
            hwHelper.isRcsAvailableValue = isRcsAvailable;
            for (auto engineGroupType : {EngineGroupType::RenderCompute, EngineGroupType::Compute,
                                         EngineGroupType::CooperativeCompute}) {

                auto isCooperativeDispatchSupported = hwHelper.isCooperativeDispatchSupported(engineGroupType, hwInfo);
                if (isCooperativeEngineSupported) {
                    switch (engineGroupType) {
                    case EngineGroupType::RenderCompute:
                        EXPECT_FALSE(isCooperativeDispatchSupported);
                        break;
                    case EngineGroupType::Compute:
                        EXPECT_EQ(!isRcsAvailable, isCooperativeDispatchSupported);
                        break;
                    default: // EngineGroupType::CooperativeCompute
                        EXPECT_TRUE(isCooperativeDispatchSupported);
                    }
                } else {
                    EXPECT_TRUE(isCooperativeDispatchSupported);
                }
            }
        }
    }
}

HWTEST2_F(HwHelperTestPvcAndLater, givenHwHelperWhenGettingISAPaddingThenCorrectValueIsReturned, IsAtLeastXeHpcCore) {
    auto &hwHelper = HwHelper::get(pDevice->getHardwareInfo().platform.eRenderCoreFamily);
    EXPECT_EQ(hwHelper.getPaddingForISAAllocation(), 0xE00u);
}

using HwHelperTestCooperativeEngine = HwHelperTestPvcAndLater;
HWTEST2_F(HwHelperTestCooperativeEngine, givenCooperativeContextSupportedWhenGetEngineInstancesThenReturnCorrectAmountOfCooperativeCcs, IsXeHpcCore) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    auto &hwHelper = HwHelperHw<FamilyType>::get();
    auto &hwInfoConfig = *HwInfoConfig::get(hwInfo.platform.eProductFamily);

    uint32_t revisions[] = {REVISION_A0, REVISION_B};
    for (auto &revision : revisions) {
        auto hwRevId = hwInfoConfig.getHwRevIdFromStepping(revision, hwInfo);
        if (hwRevId == CommonConstants::invalidStepping) {
            continue;
        }
        hwInfo.platform.usRevId = hwRevId;
        auto engineInstances = hwHelper.getGpgpuEngineInstances(hwInfo);
        size_t ccsCount = 0u;
        size_t cooperativeCcsCount = 0u;
        for (auto &engineInstance : engineInstances) {
            if (EngineHelpers::isCcs(engineInstance.first)) {
                if (engineInstance.second == EngineUsage::Regular) {
                    ccsCount++;
                } else if (engineInstance.second == EngineUsage::Cooperative) {
                    cooperativeCcsCount++;
                }
            }
        }
        EXPECT_EQ(2u, ccsCount);
        if (hwInfoConfig.isCooperativeEngineSupported(hwInfo)) {
            EXPECT_EQ(ccsCount, cooperativeCcsCount);
        } else {
            EXPECT_EQ(0u, cooperativeCcsCount);
        }
    }
}
