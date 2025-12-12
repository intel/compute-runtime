/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/test_macros/test.h"

#include "per_product_test_definitions.h"

using namespace NEO;

using Xe3pCoreHwCmdTest = ::testing::Test;

XE3P_CORETEST_F(Xe3pCoreHwCmdTest, givenMediaSurfaceStateWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto mediaSurfaceState = FamilyType::cmdInitMediaSurfaceState;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    mediaSurfaceState.setSurfaceMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, mediaSurfaceState.TheStructure.Common.SurfaceMemoryObjectControlStateIndexToMocsTables);
}

XE3P_CORETEST_F(Xe3pCoreHwCmdTest, givenMediaSurfaceStateWhenSettingSurfaceBaseAddressThenCorrectvalueIsSet) {
    auto mediaSurfaceState = FamilyType::cmdInitMediaSurfaceState;
    uint64_t address = 0x12345678ABCD;
    uint32_t expectedAddressLow = 0x5678ABCD;
    uint32_t expectedAddressHigh = 0x1234;
    mediaSurfaceState.setSurfaceBaseAddress(address);
    EXPECT_EQ(address, mediaSurfaceState.getSurfaceBaseAddress());
    EXPECT_EQ(expectedAddressLow, mediaSurfaceState.TheStructure.Common.SurfaceBaseAddress);
    EXPECT_EQ(expectedAddressHigh, mediaSurfaceState.TheStructure.Common.SurfaceBaseAddressHigh);
}

XE3P_CORETEST_F(Xe3pCoreHwCmdTest, givenRenderSurfaceStateThenDefaultHorizontalAlignmentIs128) {
    auto defaultHorizontalAlignmentValue = FamilyType::RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_DEFAULT;
    auto horizontalAlignment128Value = FamilyType::RENDER_SURFACE_STATE::SURFACE_HORIZONTAL_ALIGNMENT_HALIGN_128;
    EXPECT_EQ(defaultHorizontalAlignmentValue, horizontalAlignment128Value);
}

XE3P_CORETEST_F(Xe3pCoreHwCmdTest, givenRenderSurfaceStateWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto renderSurfaceState = FamilyType::cmdInitRenderSurfaceState;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    renderSurfaceState.setMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, renderSurfaceState.TheStructure.Common.MemoryObjectControlStateIndexToMocsTables);
}

XE3P_CORETEST_F(Xe3pCoreHwCmdTest, givenStateBaseAddressWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto stateBaseAddress = FamilyType::cmdInitStateBaseAddress;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    stateBaseAddress.setGeneralStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.GeneralStateMemoryObjectControlStateIndexToMocsTables);

    stateBaseAddress.setStatelessDataPortAccessMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.StatelessDataPortAccessMemoryObjectControlStateIndexToMocsTables);

    stateBaseAddress.setSurfaceStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.SurfaceStateMemoryObjectControlStateIndexToMocsTables);

    stateBaseAddress.setDynamicStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.DynamicStateMemoryObjectControlStateIndexToMocsTables);

    stateBaseAddress.setInstructionMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.InstructionMemoryObjectControlStateIndexToMocsTables);

    stateBaseAddress.setBindlessSurfaceStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.BindlessSurfaceStateMemoryObjectControlStateIndexToMocsTables);

    stateBaseAddress.setBindlessSamplerStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.BindlessSamplerStateMemoryObjectControlStateIndexToMocsTables);
}

XE3P_CORETEST_F(Xe3pCoreHwCmdTest, givenPostSyncDataWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto postSyncData = FamilyType::cmdInitPostSyncData;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    postSyncData.setMocs(mocs);
    EXPECT_EQ(expectedMocsIndex, postSyncData.TheStructure.Common.MocsIndexToMocsTables);
}

XE3P_CORETEST_F(Xe3pCoreHwCmdTest, givenArbCheckWhenProgrammingPreParserDisableThenMaskBitIsSet) {
    {
        auto arbCheck = FamilyType::cmdInitArbCheck;
        EXPECT_EQ(0u, arbCheck.TheStructure.Common.MaskBits);
        arbCheck.setPreParserDisable(true);
        EXPECT_EQ(1u, arbCheck.TheStructure.Common.MaskBits);
    }
    {
        auto arbCheck = FamilyType::cmdInitArbCheck;
        EXPECT_EQ(0u, arbCheck.TheStructure.Common.MaskBits);
        arbCheck.setPreParserDisable(false);
        EXPECT_EQ(1u, arbCheck.TheStructure.Common.MaskBits);
    }
}

XE3P_CORETEST_F(Xe3pCoreHwCmdTest, givenXyColorBltkWhenSetFillColorThenProperValuesAreSet) {
    auto xyColorBlt = FamilyType::cmdInitXyColorBlt;
    uint32_t fillColor[4] = {0x1234, 0x5678, 0x9ABC, 0xDEF0};

    xyColorBlt.setFillColor(fillColor);
    EXPECT_EQ(0x1234u, xyColorBlt.TheStructure.Common.FillColorValue[0]);
    EXPECT_EQ(0x5678u, xyColorBlt.TheStructure.Common.FillColorValue[1]);
    EXPECT_EQ(0x9ABCu, xyColorBlt.TheStructure.Common.FillColorValue[2]);
    EXPECT_EQ(0xDEF0u, xyColorBlt.TheStructure.Common.FillColorValue[3]);
}

XE3P_CORETEST_F(Xe3pCoreHwCmdTest, givenXyColorBltkWhenSettingDestinationMocsThenProperValuesAreSet) {
    auto xyColorBlt = FamilyType::cmdInitXyColorBlt;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    xyColorBlt.setDestinationMOCS(mocs);
    EXPECT_EQ(expectedMocsIndex, xyColorBlt.TheStructure.Common.DestinationMocs);
}

XE3P_CORETEST_F(Xe3pCoreHwCmdTest, givenXyBlockCopyBltkWhenSettingDestinationMocsThenProperValuesAreSet) {
    auto xyBlockCopyBlt = FamilyType::XY_BLOCK_COPY_BLT::sInit();
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    xyBlockCopyBlt.setDestinationMOCS(mocs);
    EXPECT_EQ(expectedMocsIndex, xyBlockCopyBlt.TheStructure.Common.DestinationMocs);

    xyBlockCopyBlt.setSourceMOCS(mocs);
    EXPECT_EQ(expectedMocsIndex, xyBlockCopyBlt.TheStructure.Common.SourceMocs);
}

XE3P_CORETEST_F(Xe3pCoreHwCmdTest, givenMemCopyWhenSettingMocsThenProperValuesAreSet) {
    auto memCopy = FamilyType::MEM_COPY::sInit();
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    memCopy.setDestinationMOCS(mocs);
    EXPECT_EQ(expectedMocsIndex, memCopy.TheStructure.Common.DestinationMocs);

    memCopy.setSourceMOCS(mocs);
    EXPECT_EQ(expectedMocsIndex, memCopy.TheStructure.Common.SourceMocs);
}

XE3P_CORETEST_F(Xe3pCoreHwCmdTest, givenStatePrefetchWhenSettingMocsThenProperValuesAreSet) {
    auto statePrefetch = FamilyType::cmdInitStatePrefetch;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    statePrefetch.setMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, statePrefetch.TheStructure.Common.MemoryObjectControlStateIndexToMocsTables);
}

XE3P_CORETEST_F(Xe3pCoreHwCmdTest, givenMemSetWhenSettingDestinationMocsThenProperValuesAreSet) {
    auto memSet = FamilyType::cmdInitMemSet;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    memSet.setDestinationMOCS(mocs);
    EXPECT_EQ(expectedMocsIndex, memSet.TheStructure.Common.DestinationMocs);
}

XE3P_CORETEST_F(Xe3pCoreHwCmdTest, givenMemSetWhenSettingDestinationStartAddressThenProperValuesAreSet) {
    auto memSet = FamilyType::cmdInitMemSet;
    uint64_t address = 0x12345678ABCD;
    uint32_t expectedAddressLow = 0x5678ABCD;
    uint32_t expectedAddressHigh = 0x1234;
    memSet.setDestinationStartAddress(address);
    EXPECT_EQ(address, memSet.getDestinationStartAddress());
    EXPECT_EQ(expectedAddressLow, memSet.TheStructure.Common.DestinationStartAddressLow);
    EXPECT_EQ(expectedAddressHigh, memSet.TheStructure.Common.DestinationStartAddressHigh);
}
