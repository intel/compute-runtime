/*
 * Copyright (C) 2021-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/helpers/state_base_address_tests.h"

using namespace NEO;

using XeHpgCoreHwCmdTest = ::testing::Test;

XE_HPG_CORETEST_F(XeHpgCoreHwCmdTest, givenComputeWalkerThenPostSyncTypeIsPostSyncData) {
    auto postSyncType = FamilyType::template getPostSyncType<typename FamilyType::DefaultWalkerType>();
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    EXPECT_TRUE((std::is_same<POSTSYNC_DATA, decltype(postSyncType)>::value));
}

XE_HPG_CORETEST_F(XeHpgCoreHwCmdTest, givenRenderSurfaceStateWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto renderSurfaceState = FamilyType::cmdInitRenderSurfaceState;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    renderSurfaceState.setMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, renderSurfaceState.TheStructure.Common.IndexToMocsTables);
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

    stateBaseAddress.setBindlessSamplerStateMemoryObjectControlState(mocs);
    EXPECT_EQ(expectedMocsIndex, stateBaseAddress.TheStructure.Common.BindlessSamplerStateMemoryObjectControlStateIndexToMOCSTables);
}

XE_HPG_CORETEST_F(XeHpgCoreHwCmdTest, givenOddMocsWhenProgrammingStateBaseAddressMocsThenEncryptedDataBitIsSet) {
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

XE_HPG_CORETEST_F(XeHpgCoreHwCmdTest, givenPostSyncDataWhenProgrammingMocsThenMocsIndexIsSetProperly) {
    auto postSyncData = FamilyType::cmdInitPostSyncData;
    uint32_t mocs = 4u;
    uint32_t expectedMocsIndex = (mocs >> 1);
    postSyncData.setMocs(mocs);
    EXPECT_EQ(expectedMocsIndex, postSyncData.TheStructure.Common.IndexToMocsTables);
}

XE_HPG_CORETEST_F(XeHpgCoreHwCmdTest, givenArbCheckWhenProgrammingPreParserDisableThenMaskBitIsSet) {
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

using XeHpgSbaTest = SbaTest;

XE_HPG_CORETEST_F(XeHpgSbaTest, givenSpecificProductFamilyWhenAppendingSbaThenProgramWBPL1CachePolicy) {
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getRootDeviceEnvironment().getGmmHelper(), &ssh, nullptr, nullptr);
    args.setGeneralStateBaseAddress = true;

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(FamilyType::STATE_BASE_ADDRESS::L1_CACHE_CONTROL_WBP, sbaCmd.getL1CacheControl());
}

XE_HPG_CORETEST_F(XeHpgSbaTest, givenL1CachingOverrideWhenStateBaseAddressIsProgrammedThenItMatchesTheOverrideValue) {
    DebugManagerStateRestore restorer;
    debugManager.flags.ForceStatelessL1CachingPolicy.set(0u);
    auto sbaCmd = FamilyType::cmdInitStateBaseAddress;
    StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getRootDeviceEnvironment().getGmmHelper(), &ssh, nullptr, nullptr);
    args.setGeneralStateBaseAddress = true;

    auto &productHelper = getHelper<ProductHelper>();
    updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(0u, sbaCmd.getL1CacheControl());

    debugManager.flags.ForceStatelessL1CachingPolicy.set(2u);
    updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(2u, sbaCmd.getL1CacheControl());

    debugManager.flags.ForceAllResourcesUncached.set(true);
    updateSbaHelperArgsL1CachePolicy<FamilyType>(args, productHelper);

    StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);

    EXPECT_EQ(1u, sbaCmd.getL1CacheControl());
}

XE_HPG_CORETEST_F(XeHpgSbaTest, givenMemoryCompressionEnabledWhenAppendingSbaThenEnableStatelessCompressionForAllStatelessAccesses) {
    for (auto memoryCompressionState : {MemoryCompressionState::notApplicable, MemoryCompressionState::disabled, MemoryCompressionState::enabled}) {
        auto sbaCmd = FamilyType::cmdInitStateBaseAddress;
        StateBaseAddressHelperArgs<FamilyType> args = createSbaHelperArgs<FamilyType>(&sbaCmd, pDevice->getRootDeviceEnvironment().getGmmHelper(), &ssh, nullptr, nullptr);
        args.setGeneralStateBaseAddress = true;
        args.memoryCompressionState = memoryCompressionState;

        StateBaseAddressHelper<FamilyType>::appendStateBaseAddressParameters(args);
        EXPECT_EQ(memoryCompressionState == MemoryCompressionState::enabled, sbaCmd.getEnableMemoryCompressionForAllStatelessAccesses());
    }
}
