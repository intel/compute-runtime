/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_base.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using XeHpgCoreHwCmdTest = ::testing::Test;

XE_HPG_CORETEST_F(XeHpgCoreHwCmdTest, givenMediaSurfaceStateWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto mediaSurfaceState = FamilyType::cmdInitMediaSurfaceState;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    mediaSurfaceState.setSurfaceMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, mediaSurfaceState.TheStructure.Common.SurfaceMemoryObjectControlState_IndexToMocsTables);
}

XE_HPG_CORETEST_F(XeHpgCoreHwCmdTest, givenRenderSurfaceStateWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto renderSurfaceState = FamilyType::cmdInitRenderSurfaceState;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    renderSurfaceState.setMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, renderSurfaceState.TheStructure.Common.MemoryObjectControlStateIndexToMocsTables);
}

XE_HPG_CORETEST_F(XeHpgCoreHwCmdTest, givenRenderSurfaceStateThenDefaultHorizontalAlignmentIs128) {
    auto defaultHorizontalAlignmentValue = FamilyType::RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_DEFAULT;
    auto horizontalAlignment128Value = FamilyType::RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_128;
    EXPECT_EQ(defaultHorizontalAlignmentValue, horizontalAlignment128Value);
}

XE_HPG_CORETEST_F(XeHpgCoreHwCmdTest, givenStateBaseAddressWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto stateBaseAddress = FamilyType::cmdInitStateBaseAddress;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    stateBaseAddress.setGeneralStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.GeneralStateMemoryObjectControlState_IndexToMocsTables);

    stateBaseAddress.setStatelessDataPortAccessMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.StatelessDataPortAccessMemoryObjectControlState_IndexToMocsTables);

    stateBaseAddress.setSurfaceStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.SurfaceStateMemoryObjectControlState_IndexToMocsTables);

    stateBaseAddress.setDynamicStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.DynamicStateMemoryObjectControlState_IndexToMocsTables);

    stateBaseAddress.setInstructionMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.InstructionMemoryObjectControlState_IndexToMocsTables);

    stateBaseAddress.setBindlessSurfaceStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.BindlessSurfaceStateMemoryObjectControlState_IndexToMocsTables);

    stateBaseAddress.setBindlessSamplerStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.BindlessSamplerStateMemoryObjectControlState_IndexToMocsTables);
}

XE_HPG_CORETEST_F(XeHpgCoreHwCmdTest, givenPostSyncDataWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto postSyncData = FamilyType::cmdInitPostSyncData;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    postSyncData.setMocs(mocs);
    EXPECT_EQ(expectedMocsIndex, postSyncData.TheStructure.Common.MocsIndexToMocsTables);
}

XE_HPG_CORETEST_F(XeHpgCoreHwCmdTest, givenLoadRegisterMemWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto loadRegisterMem = FamilyType::cmdInitLoadRegisterMem;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    loadRegisterMem.setMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, loadRegisterMem.TheStructure.Common.MemoryObjectControlStateIndexToMocsTables);
}

XE_HPG_CORETEST_F(XeHpgCoreHwCmdTest, givenStoreRegisterMemWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto storeRegisterMem = FamilyType::cmdInitStoreRegisterMem;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    storeRegisterMem.setMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, storeRegisterMem.TheStructure.Common.MemoryObjectControlStateIndexToMocsTables);
}

XE_HPG_CORETEST_F(XeHpgCoreHwCmdTest, givenStoreDataImmWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto storeDataImm = FamilyType::cmdInitStoreDataImm;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    storeDataImm.setMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, storeDataImm.TheStructure.Common.MemoryObjectControlStateIndexToMocsTables);
}
