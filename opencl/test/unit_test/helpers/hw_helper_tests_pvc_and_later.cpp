/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/test_macros/test.h"

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

HWTEST2_F(HwHelperTestPvcAndLater, GivenVariousValuesWhenCallingCalculateAvailableThreadCountThenCorrectValueIsReturned, IsAtLeastXeHpcCore) {
    struct TestInput {
        uint32_t grfCount;
        uint32_t expectedThreadCountPerEu;
    };

    std::vector<TestInput> grfTestInputs = {
        {64, 16},
        {96, 10},
        {128, 8},
        {160, 6},
        {192, 5},
        {256, 4},
    };

    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);

    for (auto &testInput : grfTestInputs) {
        auto expected = testInput.expectedThreadCountPerEu * hardwareInfo.gtSystemInfo.EUCount;
        auto result = hwHelper.calculateAvailableThreadCount(
            hardwareInfo.platform.eProductFamily,
            testInput.grfCount,
            hardwareInfo.gtSystemInfo.EUCount,
            hardwareInfo.gtSystemInfo.ThreadCount / hardwareInfo.gtSystemInfo.EUCount);
        EXPECT_EQ(expected, result);
    }
}

HWTEST2_F(HwHelperTestPvcAndLater, GivenVariousValuesWhenCallingGetBarriersCountFromHasBarrierThenCorrectValueIsReturned, IsAtLeastXeHpcCore) {
    auto &hwHelper = HwHelper::get(hardwareInfo.platform.eRenderCoreFamily);

    EXPECT_EQ(0u, hwHelper.getBarriersCountFromHasBarriers(0u));
    EXPECT_EQ(1u, hwHelper.getBarriersCountFromHasBarriers(1u));

    EXPECT_EQ(2u, hwHelper.getBarriersCountFromHasBarriers(2u));
    EXPECT_EQ(4u, hwHelper.getBarriersCountFromHasBarriers(3u));
    EXPECT_EQ(8u, hwHelper.getBarriersCountFromHasBarriers(4u));
    EXPECT_EQ(16u, hwHelper.getBarriersCountFromHasBarriers(5u));
    EXPECT_EQ(24u, hwHelper.getBarriersCountFromHasBarriers(6u));
    EXPECT_EQ(32u, hwHelper.getBarriersCountFromHasBarriers(7u));
}

HWTEST2_F(HwHelperTestPvcAndLater, givenCooperativeContextSupportedWhenGetEngineInstancesThenReturnCorrectAmountOfCooperativeCcs, IsAtLeastXeHpcCore) {
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
        if (hwHelper.isCooperativeEngineSupported(hwInfo)) {
            EXPECT_EQ(ccsCount, cooperativeCcsCount);
        } else {
            EXPECT_EQ(0u, cooperativeCcsCount);
        }
    }
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
        bool isCooperativeEngineSupported(const HardwareInfo &hwInfo) const override {
            return isCooperativeEngineSupportedValue;
        }
        bool isRcsAvailable(const HardwareInfo &hwInfo) const override {
            return isRcsAvailableValue;
        }
        bool isCooperativeEngineSupportedValue = true;
        bool isRcsAvailableValue = true;
    };
    MockHwHelper hwHelper{};

    auto hwInfo = *::defaultHwInfo;
    for (auto isCooperativeEngineSupported : ::testing::Bool()) {
        hwHelper.isCooperativeEngineSupportedValue = isCooperativeEngineSupported;
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
