/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/encoders/test_encode_dispatch_kernel_dg2_and_later.h"

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

#include "test_traits_common.h"

using namespace NEO;

using CommandEncodeStatesTestDg2AndLater = Test<CommandEncodeStatesFixture>;

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, givenEventAddressWhenEncodeAndPVCAndDG2ThenSetDataportSubsliceCacheFlushIstSet, IsAtLeastXeCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint64_t eventAddress = MemoryConstants::cacheLineSize * 123;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.postSyncArgs.eventAddress = eventAddress;
    dispatchArgs.postSyncArgs.isTimestampEvent = true;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(true, cmd->getPostSync().getDataportSubsliceCacheFlush());
}

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, givenDebugVariableToForceL1FlushWhenWalkerIsProgramedThenCacheFlushIsDisabled, IsAtLeastXeCore) {
    DebugManagerStateRestore restore;
    NEO::debugManager.flags.ForcePostSyncL1Flush.set(0);
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint64_t eventAddress = MemoryConstants::cacheLineSize * 123;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.postSyncArgs.eventAddress = eventAddress;
    dispatchArgs.postSyncArgs.isTimestampEvent = true;

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    EXPECT_EQ(false, cmd->getPostSync().getDataportPipelineFlush());
    EXPECT_EQ(false, cmd->getPostSync().getDataportSubsliceCacheFlush());
}

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, givenEventAddressWhenEncodeThenMocsIndex2IsSet, IsXeHpgCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint64_t eventAddress = MemoryConstants::cacheLineSize * 123;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.postSyncArgs.eventAddress = eventAddress;
    dispatchArgs.postSyncArgs.isTimestampEvent = true;
    dispatchArgs.postSyncArgs.dcFlushEnable = MemorySynchronizationCommands<FamilyType>::getDcFlushEnable(true, pDevice->getRootDeviceEnvironment());

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);

    auto gmmHelper = pDevice->getGmmHelper();

    EXPECT_EQ(gmmHelper->getUncachedMOCS(), cmd->getPostSync().getMocs());
}

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, GivenVariousSlmTotalSizesWhenSetPreferredSlmIsCalledThenCorrectValuesAreSet, IsXeHpgCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;

    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    const std::vector<PreferredSlmTestValues<FamilyType>> valuesToTest = {
        {0, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0KB},
        {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16KB},
        {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32KB},
        // since we can't set 48KB as SLM size for workgroup, we need to ask for 64KB here.
        {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64KB},
    };

    verifyPreferredSlmValues<FamilyType>(valuesToTest, pDevice->getRootDeviceEnvironment());
}

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, GivenDebugOverrideWhenSetAdditionalInfoIsCalledThenDebugValuesAreSet, IsXeCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;

    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    DebugManagerStateRestore stateRestore;
    PREFERRED_SLM_ALLOCATION_SIZE debugOverrideValues[] = {PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0KB,
                                                           PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32KB,
                                                           PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128KB};

    for (auto debugOverrideValue : debugOverrideValues) {
        debugManager.flags.OverridePreferredSlmAllocationSizePerDss.set(debugOverrideValue);
        const std::vector<PreferredSlmTestValues<FamilyType>> valuesToTest = {
            {0, debugOverrideValue},
            {32 * MemoryConstants::kiloByte, debugOverrideValue},
            {64 * MemoryConstants::kiloByte, debugOverrideValue},
        };
        verifyPreferredSlmValues<FamilyType>(valuesToTest, pDevice->getRootDeviceEnvironment());
    }
}

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, GivenDebugOverrideWhenSetAdditionalInfoIsCalledThenDebugValuesAreSet, IsAtLeastXe2HpgCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA = typename DefaultWalkerType::InterfaceDescriptorType;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    DebugManagerStateRestore stateRestore;
    PREFERRED_SLM_ALLOCATION_SIZE debugOverrideValues[] = {PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_0K,
                                                           PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_32K,
                                                           PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_SLM_ENCODES_128K};

    for (auto debugOverrideValue : debugOverrideValues) {
        debugManager.flags.OverridePreferredSlmAllocationSizePerDss.set(debugOverrideValue);
        const std::vector<PreferredSlmTestValues<FamilyType>> valuesToTest = {
            {0, debugOverrideValue},
            {32 * MemoryConstants::kiloByte, debugOverrideValue},
            {64 * MemoryConstants::kiloByte, debugOverrideValue},
        };
        verifyPreferredSlmValues<FamilyType>(valuesToTest, pDevice->getRootDeviceEnvironment());
    }
}

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, givenOverridePreferredSlmAllocationSizePerDssWhenDispatchingKernelThenCorrectValueIsSet, IsAtLeastXeCore) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    DebugManagerStateRestore restorer;
    debugManager.flags.OverridePreferredSlmAllocationSizePerDss.set(5);
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 1;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<DefaultWalkerType *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();

    EXPECT_EQ(5u, static_cast<uint32_t>(idd.getPreferredSlmAllocationSize()));
}
