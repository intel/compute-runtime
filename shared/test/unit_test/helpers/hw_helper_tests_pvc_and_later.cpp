/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/hw_helper_tests.h"
#include "shared/test/common/helpers/mock_hw_info_config_hw.h"
#include "shared/test/common/test_macros/hw_test.h"

using GfxCoreHelperTestPvcAndLater = GfxCoreHelperTest;

HWTEST2_F(GfxCoreHelperTestPvcAndLater, givenVariousCachesRequestsThenProperMocsIndexesAreBeingReturned, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore;

    auto &helper = GfxCoreHelper::get(renderCoreFamily);
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

HWTEST2_F(GfxCoreHelperTestPvcAndLater, givenRenderEngineWhenRemapCalledThenUseCccs, IsAtLeastXeHpcCore) {
    hardwareInfo.featureTable.flags.ftrCCSNode = false;

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo);

    EXPECT_EQ(aub_stream::EngineType::ENGINE_CCCS, EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, hardwareInfo));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_CCCS, EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_CCCS, hardwareInfo));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_CCS, EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_CCS, hardwareInfo));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_BCS, hardwareInfo));
}

HWTEST2_F(GfxCoreHelperTestPvcAndLater, GivenVariousValuesAndPvcAndLaterPlatformsWhenCallingCalculateAvailableThreadCountThenCorrectValueIsReturned, IsAtLeastXeHpcCore) {
    std::array<std::pair<uint32_t, uint32_t>, 6> grfTestInputs = {{{64, 16},
                                                                   {96, 10},
                                                                   {128, 8},
                                                                   {160, 6},
                                                                   {192, 5},
                                                                   {256, 4}}};
    auto &gfxCoreHelper = GfxCoreHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    for (const auto &[grfCount, expectedThreadCountPerEu] : grfTestInputs) {
        auto expected = expectedThreadCountPerEu * hardwareInfo.gtSystemInfo.EUCount;
        auto result = gfxCoreHelper.calculateAvailableThreadCount(hardwareInfo, grfCount);
        EXPECT_EQ(expected, result);
    }
}

HWTEST2_F(GfxCoreHelperTestPvcAndLater, GivenModifiedGtSystemInfoAndPvcAndLaterPlatformsWhenCallingCalculateAvailableThreadCountThenCorrectValueIsReturned, IsAtLeastXeHpcCore) {
    std::array<std::pair<uint32_t, uint32_t>, 3> testInputs = {{{64, 256},
                                                                {96, 384},
                                                                {128, 512}}};
    auto &gfxCoreHelper = GfxCoreHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    auto hwInfo = hardwareInfo;
    for (const auto &[euCount, expectedThreadCount] : testInputs) {
        hwInfo.gtSystemInfo.EUCount = euCount;
        auto result = gfxCoreHelper.calculateAvailableThreadCount(hwInfo, 256);
        EXPECT_EQ(expectedThreadCount, result);
    }
}

HWTEST2_F(GfxCoreHelperTestPvcAndLater, givenGfxCoreHelperWhenCheckIsUpdateTaskCountFromWaitSupportedThenReturnsTrue, IsAtLeastXeHpcCore) {
    auto &gfxCoreHelper = GfxCoreHelper::get(hardwareInfo.platform.eRenderCoreFamily);

    EXPECT_TRUE(gfxCoreHelper.isUpdateTaskCountFromWaitSupported());
}

HWTEST2_F(GfxCoreHelperTestPvcAndLater, givenComputeEngineAndCooperativeUsageWhenGetEngineGroupTypeIsCalledThenCooperativeComputeGroupTypeIsReturned, IsAtLeastXeHpcCore) {
    auto &gfxCoreHelper = GfxCoreHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    auto hwInfo = *::defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;
    aub_stream::EngineType engineTypes[] = {aub_stream::EngineType::ENGINE_CCS, aub_stream::EngineType::ENGINE_CCS1,
                                            aub_stream::EngineType::ENGINE_CCS2, aub_stream::EngineType::ENGINE_CCS3};
    EngineUsage engineUsages[] = {EngineUsage::Regular, EngineUsage::LowPriority, EngineUsage::Internal, EngineUsage::Cooperative};

    for (auto engineType : engineTypes) {
        for (auto engineUsage : engineUsages) {
            if (engineUsage == EngineUsage::Cooperative) {
                EXPECT_EQ(EngineGroupType::CooperativeCompute, gfxCoreHelper.getEngineGroupType(engineType, engineUsage, hwInfo));
            } else {
                EXPECT_EQ(EngineGroupType::Compute, gfxCoreHelper.getEngineGroupType(engineType, engineUsage, hwInfo));
            }
        }
    }
}

HWTEST2_F(GfxCoreHelperTestPvcAndLater, givenPVCAndLaterPlatformWhenCheckingIfEngineTypeRemappingIsRequiredThenReturnTrue, IsAtLeastXeHpcCore) {
    const auto &gfxCoreHelper = GfxCoreHelper::get(hardwareInfo.platform.eRenderCoreFamily);
    EXPECT_TRUE(gfxCoreHelper.isEngineTypeRemappingToHwSpecificRequired());
}

HWTEST2_F(GfxCoreHelperTestPvcAndLater, WhenIsRcsAvailableIsCalledThenCorrectValueIsReturned, IsAtLeastXeHpcCore) {
    auto &gfxCoreHelper = GfxCoreHelper::get(hardwareInfo.platform.eRenderCoreFamily);
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
                EXPECT_TRUE(gfxCoreHelper.isRcsAvailable(hwInfo));
            } else {
                EXPECT_FALSE(gfxCoreHelper.isRcsAvailable(hwInfo));
            }
        }
    }
}

HWTEST2_F(GfxCoreHelperTestPvcAndLater, WhenIsCooperativeDispatchSupportedThenCorrectValueIsReturned, IsAtLeastXeHpcCore) {
    struct MockGfxCoreHelper : NEO::GfxCoreHelperHw<FamilyType> {
        bool isRcsAvailable(const HardwareInfo &hwInfo) const override {
            return isRcsAvailableValue;
        }
        bool isRcsAvailableValue = true;
    };

    MockProductHelperHw<productFamily> productHelper;
    auto hwInfo = *::defaultHwInfo;
    VariableBackup<ProductHelper *> productHelperFactoryBackup{&NEO::productHelperFactory[static_cast<size_t>(hwInfo.platform.eProductFamily)]};
    productHelperFactoryBackup = &productHelper;

    MockGfxCoreHelper gfxCoreHelper{};

    for (auto isCooperativeEngineSupported : ::testing::Bool()) {
        productHelper.isCooperativeEngineSupportedValue = isCooperativeEngineSupported;
        for (auto isRcsAvailable : ::testing::Bool()) {
            gfxCoreHelper.isRcsAvailableValue = isRcsAvailable;
            for (auto engineGroupType : {EngineGroupType::RenderCompute, EngineGroupType::Compute,
                                         EngineGroupType::CooperativeCompute}) {

                auto isCooperativeDispatchSupported = gfxCoreHelper.isCooperativeDispatchSupported(engineGroupType, hwInfo);
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

HWTEST2_F(GfxCoreHelperTestPvcAndLater, givenGfxCoreHelperWhenGettingISAPaddingThenCorrectValueIsReturned, IsAtLeastXeHpcCore) {
    auto &helper = getHelper<GfxCoreHelper>();
    EXPECT_EQ(helper.getPaddingForISAAllocation(), 0xE00u);
}

HWTEST2_F(GfxCoreHelperTestPvcAndLater, givenForceBCSForInternalCopyEngineVariableSetWhenGetGpgpuEnginesCalledThenForceInternalBCSEngineIndex, IsAtLeastXeHpcCore) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrBcsInfo = maxNBitValue(9);
    hwInfo.capabilityTable.blitterOperationsSupported = true;

    DebugManagerStateRestore restore;
    DebugManager.flags.ForceBCSForInternalCopyEngine.set(2);

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));

    auto &engines = GfxCoreHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_GE(engines.size(), 9u);

    bool found = false;
    for (auto engine : engines) {
        if ((engine.first == aub_stream::ENGINE_BCS2) && (engine.second == EngineUsage::Internal)) {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

using GfxCoreHelperTestCooperativeEngine = GfxCoreHelperTestPvcAndLater;
HWTEST2_F(GfxCoreHelperTestCooperativeEngine, givenCooperativeContextSupportedWhenGetEngineInstancesThenReturnCorrectAmountOfCooperativeCcs, IsXeHpcCore) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    auto &gfxCoreHelper = GfxCoreHelperHw<FamilyType>::get();
    auto &productHelper = *ProductHelper::get(hwInfo.platform.eProductFamily);

    uint32_t revisions[] = {REVISION_A0, REVISION_B};
    for (auto &revision : revisions) {
        auto hwRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
        if (hwRevId == CommonConstants::invalidStepping) {
            continue;
        }
        hwInfo.platform.usRevId = hwRevId;
        auto engineInstances = gfxCoreHelper.getGpgpuEngineInstances(hwInfo);
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
        if (productHelper.isCooperativeEngineSupported(hwInfo)) {
            EXPECT_EQ(ccsCount, cooperativeCcsCount);
        } else {
            EXPECT_EQ(0u, cooperativeCcsCount);
        }
    }
}
