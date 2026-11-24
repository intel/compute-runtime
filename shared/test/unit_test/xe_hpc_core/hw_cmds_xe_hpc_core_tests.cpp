/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_xe_hpc_core_base.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using XeHpcCoreHwCmdTest = ::testing::Test;

XE_HPC_CORETEST_F(XeHpcCoreHwCmdTest, givenRenderSurfaceStateWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto renderSurfaceState = FamilyType::cmdInitRenderSurfaceState;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    renderSurfaceState.setMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, renderSurfaceState.TheStructure.Common.IndexToMocsTables);
}

XE_HPC_CORETEST_F(XeHpcCoreHwCmdTest, givenRenderSurfaceStateThenDefaultHorizontalAlignmentIs128) {
    auto defaultHorizontalAlignmentValue = FamilyType::RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_DEFAULT;
    auto horizontalAlignment128Value = FamilyType::RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_128;
    EXPECT_EQ(defaultHorizontalAlignmentValue, horizontalAlignment128Value);
}

XE_HPC_CORETEST_F(XeHpcCoreHwCmdTest, givenStateBaseAddressWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto stateBaseAddress = FamilyType::cmdInitStateBaseAddress;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    stateBaseAddress.setGeneralStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.GeneralStateMemoryObjectControlStateIndexToMOCSTables);

    stateBaseAddress.setStatelessDataPortAccessMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.StatelessDataPortAccessMemoryObjectControlStateIndexToMOCSTables);

    stateBaseAddress.setSurfaceStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.SurfaceStateMemoryObjectControlStateIndexToMOCSTables);

    stateBaseAddress.setDynamicStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.DynamicStateMemoryObjectControlStateIndexToMOCSTables);

    stateBaseAddress.setInstructionMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.InstructionMemoryObjectControlStateIndexToMOCSTables);

    stateBaseAddress.setBindlessSurfaceStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.BindlessSurfaceStateMemoryObjectControlStateIndexToMOCSTables);
}

XE_HPC_CORETEST_F(XeHpcCoreHwCmdTest, givenOddMocsWhenProgrammingStateBaseAddressMocsThenEncryptedDataBitIsSet) {
    auto stateBaseAddress = FamilyType::cmdInitStateBaseAddress;
    constexpr uint32_t mocs = 5u;
    stateBaseAddress.setGeneralStateMemoryObjectControlState(mocs);
    EXPECT_EQ(1u, stateBaseAddress.TheStructure.Common.GeneralStateMemoryObjectControlStateEncryptedData);
    EXPECT_EQ(mocs, stateBaseAddress.getGeneralStateMemoryObjectControlState());
    stateBaseAddress.setStatelessDataPortAccessMemoryObjectControlState(mocs);
    EXPECT_EQ(1u, stateBaseAddress.TheStructure.Common.StatelessDataPortAccessMemoryObjectControlStateEncryptedData);
    EXPECT_EQ(mocs, stateBaseAddress.getStatelessDataPortAccessMemoryObjectControlState());
    stateBaseAddress.setSurfaceStateMemoryObjectControlState(mocs);
    EXPECT_EQ(1u, stateBaseAddress.TheStructure.Common.SurfaceStateMemoryObjectControlStateEncryptedData);
    EXPECT_EQ(mocs, stateBaseAddress.getSurfaceStateMemoryObjectControlState());
    stateBaseAddress.setDynamicStateMemoryObjectControlState(mocs);
    EXPECT_EQ(1u, stateBaseAddress.TheStructure.Common.DynamicStateMemoryObjectControlStateEncryptedData);
    EXPECT_EQ(mocs, stateBaseAddress.getDynamicStateMemoryObjectControlState());
    stateBaseAddress.setIndirectObjectMemoryObjectControlState(mocs);
    EXPECT_EQ(1u, stateBaseAddress.TheStructure.Common.IndirectObjectMemoryObjectControlStateEncryptedData);
    EXPECT_EQ(mocs, stateBaseAddress.getIndirectObjectMemoryObjectControlState());
    stateBaseAddress.setInstructionMemoryObjectControlState(mocs);
    EXPECT_EQ(1u, stateBaseAddress.TheStructure.Common.InstructionMemoryObjectControlStateEncryptedData);
    EXPECT_EQ(mocs, stateBaseAddress.getInstructionMemoryObjectControlState());
    stateBaseAddress.setBindlessSurfaceStateMemoryObjectControlState(mocs);
    EXPECT_EQ(1u, stateBaseAddress.TheStructure.Common.BindlessSurfaceStateMemoryObjectControlStateEncryptedData);
    EXPECT_EQ(mocs, stateBaseAddress.getBindlessSurfaceStateMemoryObjectControlState());
}

XE_HPC_CORETEST_F(XeHpcCoreHwCmdTest, givenPostSyncDataWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto postSyncData = FamilyType::cmdInitPostSyncData;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    postSyncData.setMocs(mocs);
    EXPECT_EQ(expectedMocsIndex, postSyncData.TheStructure.Common.IndexToMocsTables);
}
