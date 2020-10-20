/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/test/unit_test/fixtures/device_fixture.h"
#include "shared/test/unit_test/helpers/debug_manager_state_restore.h"

#include "test.h"

using namespace NEO;

typedef Test<DeviceFixture> HwHelperTest;

using Platforms = IsWithinProducts<IGFX_SKYLAKE, IGFX_ICELAKE_LP>;

HWTEST2_F(HwHelperTest, givenHwHelperWhenGetGpgpuEnginesThenReturnThreeRcsEnginesAndOneBcs, Platforms) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.featureTable.ftrCCSNode = false;
    hwInfo.featureTable.ftrBcsInfo = 1;
    hwInfo.capabilityTable.defaultEngineType = aub_stream::ENGINE_RCS;
    auto gpgpuEngines = HwHelperHw<FamilyType>::get().getGpgpuEngineInstances(hwInfo);
    EXPECT_EQ(4u, gpgpuEngines.size());
    EXPECT_EQ(aub_stream::ENGINE_RCS, gpgpuEngines[0].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, gpgpuEngines[1].first);
    EXPECT_EQ(aub_stream::ENGINE_RCS, gpgpuEngines[2].first);
    EXPECT_EQ(aub_stream::ENGINE_BCS, gpgpuEngines[3].first);
}

HWTEST_F(HwHelperTest, givenHwHelperWhenEnableBlitterOperationsSupportEnabledThenBcsInCopyGroupVecotr) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableBlitterOperationsSupport.set(1);
    std::unique_ptr<OsContext> bcsOsContext;
    bcsOsContext.reset(OsContext::create(nullptr, 1, 0, aub_stream::ENGINE_BCS, PreemptionMode::Disabled,
                                         false, false, false));
    EngineControl engineControl;
    engineControl.osContext = bcsOsContext.get();
    std::vector<std::vector<EngineControl>> engineGroups;
    engineGroups.resize(static_cast<uint32_t>(EngineGroupType::MaxEngineGroups));
    HwHelperHw<FamilyType>::get().addEngineToEngineGroup(engineGroups, engineControl, pDevice->getHardwareInfo());
    EXPECT_EQ(engineGroups[static_cast<uint32_t>(EngineGroupType::Copy)].size(), 1u);
}

HWTEST_F(HwHelperTest, givenHwHelperWhenEnableBlitterOperationsSupportDisabledThenCopyGroupVecotrIsEmpty) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.EnableBlitterOperationsSupport.set(0);
    std::unique_ptr<OsContext> bcsOsContext;
    bcsOsContext.reset(OsContext::create(nullptr, 1, 0, aub_stream::ENGINE_BCS, PreemptionMode::Disabled,
                                         false, false, false));
    EngineControl engineControl;
    engineControl.osContext = bcsOsContext.get();
    std::vector<std::vector<EngineControl>> engineGroups;
    engineGroups.resize(static_cast<uint32_t>(EngineGroupType::MaxEngineGroups));
    HwHelperHw<FamilyType>::get().addEngineToEngineGroup(engineGroups, engineControl, pDevice->getHardwareInfo());
    EXPECT_EQ(engineGroups[static_cast<uint32_t>(EngineGroupType::Copy)].size(), 0u);
}