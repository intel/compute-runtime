/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/engine_node_helper.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/gfx_core_helper_tests.h"
#include "shared/test/common/helpers/mock_product_helper_hw.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

using GfxCoreHelperTestPvcAndLater = GfxCoreHelperTest;

HWTEST2_F(GfxCoreHelperTestPvcAndLater, givenVariousCachesRequestsThenProperMocsIndexesAreBeingReturned, IsAtLeastXeHpcCore) {
    DebugManagerStateRestore restore;

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto gmmHelper = this->pDevice->getRootDeviceEnvironment().getGmmHelper();
    auto expectedMocsForL3off = gmmHelper->getUncachedMOCS() >> 1;
    auto expectedMocsForL3on = gmmHelper->getL3EnabledMOCS() >> 1;

    auto mocsIndex = gfxCoreHelper.getMocsIndex(*gmmHelper, false, true);
    EXPECT_EQ(expectedMocsForL3off, mocsIndex);

    mocsIndex = gfxCoreHelper.getMocsIndex(*gmmHelper, true, false);
    EXPECT_EQ(expectedMocsForL3on, mocsIndex);

    mocsIndex = gfxCoreHelper.getMocsIndex(*gmmHelper, true, true);
    EXPECT_EQ(expectedMocsForL3on, mocsIndex);
}

HWTEST2_F(GfxCoreHelperTestPvcAndLater, givenRenderEngineWhenRemapCalledThenUseCccs, IsAtLeastXeHpcCore) {
    hardwareInfo.featureTable.flags.ftrCCSNode = false;

    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto &productHelper = getHelper<ProductHelper>();

    gfxCoreHelper.adjustDefaultEngineType(&hardwareInfo, productHelper, nullptr);
    auto &rootDeviceEnvironment = pDevice->getRootDeviceEnvironment();

    EXPECT_EQ(aub_stream::EngineType::ENGINE_CCCS, EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_RCS, rootDeviceEnvironment));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_CCCS, EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_CCCS, rootDeviceEnvironment));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_CCS, EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_CCS, rootDeviceEnvironment));
    EXPECT_EQ(aub_stream::EngineType::ENGINE_BCS, EngineHelpers::remapEngineTypeToHwSpecific(aub_stream::EngineType::ENGINE_BCS, rootDeviceEnvironment));
}

HWTEST2_F(GfxCoreHelperTestPvcAndLater, givenGfxCoreHelperWhenCheckIsUpdateTaskCountFromWaitSupportedThenReturnsTrue, IsAtLeastXeHpcCore) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();

    EXPECT_TRUE(gfxCoreHelper.isUpdateTaskCountFromWaitSupported());
}

HWTEST2_F(GfxCoreHelperTestPvcAndLater, givenComputeEngineAndCooperativeUsageWhenGetEngineGroupTypeIsCalledThenCooperativeComputeGroupTypeIsReturned, IsAtLeastXeHpcCore) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto hwInfo = *::defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 4;
    aub_stream::EngineType engineTypes[] = {aub_stream::EngineType::ENGINE_CCS, aub_stream::EngineType::ENGINE_CCS1,
                                            aub_stream::EngineType::ENGINE_CCS2, aub_stream::EngineType::ENGINE_CCS3};
    EngineUsage engineUsages[] = {EngineUsage::regular, EngineUsage::lowPriority, EngineUsage::internal, EngineUsage::cooperative};

    for (auto engineType : engineTypes) {
        for (auto engineUsage : engineUsages) {
            if (engineUsage == EngineUsage::cooperative) {
                EXPECT_EQ(EngineGroupType::cooperativeCompute, gfxCoreHelper.getEngineGroupType(engineType, engineUsage, hwInfo));
            } else {
                EXPECT_EQ(EngineGroupType::compute, gfxCoreHelper.getEngineGroupType(engineType, engineUsage, hwInfo));
            }
        }
    }
}

HWTEST2_F(GfxCoreHelperTestPvcAndLater, givenPVCAndLaterPlatformWhenCheckingIfEngineTypeRemappingIsRequiredThenReturnTrue, IsAtLeastXeHpcCore) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    EXPECT_TRUE(gfxCoreHelper.isEngineTypeRemappingToHwSpecificRequired());
}

HWTEST2_F(GfxCoreHelperTestPvcAndLater, WhenIsRcsAvailableIsCalledThenCorrectValueIsReturned, IsAtLeastXeHpcCore) {
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
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

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    RAIIProductHelperFactory<MockProductHelperHw<productFamily>> raii(rootDeviceEnvironment);
    MockGfxCoreHelper gfxCoreHelper{};

    for (auto isCooperativeEngineSupported : ::testing::Bool()) {
        raii.mockProductHelper->isCooperativeEngineSupportedValue = isCooperativeEngineSupported;
        for (auto isRcsAvailable : ::testing::Bool()) {
            gfxCoreHelper.isRcsAvailableValue = isRcsAvailable;
            for (auto engineGroupType : {EngineGroupType::renderCompute, EngineGroupType::compute,
                                         EngineGroupType::cooperativeCompute}) {

                auto isCooperativeDispatchSupported = gfxCoreHelper.isCooperativeDispatchSupported(engineGroupType, rootDeviceEnvironment);
                if (isCooperativeEngineSupported) {
                    switch (engineGroupType) {
                    case EngineGroupType::renderCompute:
                        EXPECT_FALSE(isCooperativeDispatchSupported);
                        break;
                    case EngineGroupType::compute:
                        EXPECT_EQ(!isRcsAvailable, isCooperativeDispatchSupported);
                        break;
                    default: // EngineGroupType::cooperativeCompute
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
    debugManager.flags.ForceBCSForInternalCopyEngine.set(2);

    auto device = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(&hwInfo, 0));
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto &engines = gfxCoreHelper.getGpgpuEngineInstances(device->getRootDeviceEnvironment());
    EXPECT_GE(engines.size(), 9u);

    bool found = false;
    for (auto engine : engines) {
        if ((engine.first == aub_stream::ENGINE_BCS2) && (engine.second == EngineUsage::internal)) {
            found = true;
        }
    }
    EXPECT_TRUE(found);
}

using GfxCoreHelperTestCooperativeEngine = GfxCoreHelperTestPvcAndLater;
HWTEST2_F(GfxCoreHelperTestCooperativeEngine, givenCooperativeContextSupportedWhenGetEngineInstancesThenReturnCorrectAmountOfCooperativeCcs, IsXeHpcCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};

    HardwareInfo &hwInfo = *mockExecutionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo();
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;
    hwInfo.featureTable.flags.ftrCCSNode = true;
    auto &gfxCoreHelper = getHelper<GfxCoreHelper>();
    auto &productHelper = getHelper<ProductHelper>();

    uint32_t revisions[] = {REVISION_A0, REVISION_B};
    for (auto &revision : revisions) {
        auto hwRevId = productHelper.getHwRevIdFromStepping(revision, hwInfo);
        if (hwRevId == CommonConstants::invalidStepping) {
            continue;
        }
        hwInfo.platform.usRevId = hwRevId;
        auto engineInstances = gfxCoreHelper.getGpgpuEngineInstances(*mockExecutionEnvironment.rootDeviceEnvironments[0]);
        size_t ccsCount = 0u;
        size_t cooperativeCcsCount = 0u;
        for (auto &engineInstance : engineInstances) {
            if (EngineHelpers::isCcs(engineInstance.first)) {
                if (engineInstance.second == EngineUsage::regular) {
                    ccsCount++;
                } else if (engineInstance.second == EngineUsage::cooperative) {
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
