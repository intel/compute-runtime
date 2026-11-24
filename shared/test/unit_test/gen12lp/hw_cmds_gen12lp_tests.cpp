/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using Gen12lpHwCmdTest = ::testing::Test;

GEN12LPTEST_F(Gen12lpHwCmdTest, givenRenderSurfaceStateWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto renderSurfaceState = FamilyType::cmdInitRenderSurfaceState;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    renderSurfaceState.setMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, renderSurfaceState.TheStructure.Common.IndexToMocsTables);
}

GEN12LPTEST_F(Gen12lpHwCmdTest, givenStateBaseAddressWhenProgrammingMocsThenMocsIndexIsSetProperly) {
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

    stateBaseAddress.setIndirectObjectMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.IndirectObjectMemoryObjectControlStateIndexToMOCSTables);

    stateBaseAddress.setInstructionMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.InstructionMemoryObjectControlStateIndexToMOCSTables);

    stateBaseAddress.setBindlessSurfaceStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.BindlessSurfaceStateMemoryObjectControlStateIndexToMOCSTables);

    stateBaseAddress.setBindlessSamplerStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.BindlessSamplerStateMemoryObjectControlStateIndexToMOCSTables);
}

GEN12LPTEST_F(Gen12lpHwCmdTest, givenOddMocsWhenProgrammingStateBaseAddressMocsThenEncryptedDataBitIsSet) {
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
    stateBaseAddress.setBindlessSamplerStateMemoryObjectControlState(mocs);
    EXPECT_EQ(1u, stateBaseAddress.TheStructure.Common.BindlessSamplerStateMemoryObjectControlStateEncryptedData);
    EXPECT_EQ(mocs, stateBaseAddress.getBindlessSamplerStateMemoryObjectControlState());
}

GEN12LPTEST_F(Gen12lpHwCmdTest, givenInterfaceDescriptorDataWhenSettingKernelStartPointerAbove4GbThenHighBitsAreKept) {
    auto idd = FamilyType::cmdInitInterfaceDescriptorData;
    constexpr uint64_t kernelStartPointer = 0x1'2345'6780ull;
    idd.setKernelStartPointer(kernelStartPointer);
    EXPECT_EQ(kernelStartPointer, idd.getKernelStartPointer());
}
