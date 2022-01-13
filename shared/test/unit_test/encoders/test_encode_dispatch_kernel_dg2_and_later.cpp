/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/unit_test/encoders/test_encode_dispatch_kernel_dg2_and_later.h"

#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/command_container_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_dispatch_kernel_encoder_interface.h"
#include "shared/test/common/test_macros/test.h"

#include "hw_cmds.h"
#include "test_traits_common.h"

using namespace NEO;

using CommandEncodeStatesTestDg2AndLater = Test<CommandEncodeStatesFixture>;

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, givenEventAddressWhenEncodeAndPVCAndDG2ThenSetDataportSubsliceCacheFlushIstSet, IsAtLeastXeHpgCore) {
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint64_t eventAddress = MemoryConstants::cacheLineSize * 123;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.eventAddress = eventAddress;
    dispatchArgs.isTimestampEvent = true;
    dispatchArgs.L3FlushEnable = true;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    EXPECT_EQ(true, cmd->getPostSync().getDataportSubsliceCacheFlush());
}

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, givenEventAddressWhenEncodeAndDG2ThenMocsIndex2IsSet, IsDG2) {
    using POSTSYNC_DATA = typename FamilyType::POSTSYNC_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint64_t eventAddress = MemoryConstants::cacheLineSize * 123;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.eventAddress = eventAddress;
    dispatchArgs.isTimestampEvent = true;
    dispatchArgs.L3FlushEnable = true;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());
    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);

    auto gmmHelper = pDevice->getGmmHelper();

    EXPECT_EQ(gmmHelper->getMOCS(GMM_RESOURCE_USAGE_OCL_BUFFER_CACHELINE_MISALIGNED), cmd->getPostSync().getMocs());
}

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, GivenVariousSlmTotalSizesAndSettingRevIDToDifferentValuesWhenSetAdditionalInfoIsCalledThenCorrectValuesAreSet, IsXeHpgCore) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename FamilyType::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    const std::vector<PreferredSlmTestValues<FamilyType>> valuesToTest = {
        {0, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K},
        {16 * KB, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16K},
        {32 * KB, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32K},
        //since we can't set 48KB as SLM size for workgroup, we need to ask for 64KB here.
        {64 * KB, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64K},
    };

    const std::vector<PreferredSlmTestValues<FamilyType>> valuesToTestForDg2AStep = {
        {0, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128K},
        {16 * KB, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128K},
        {32 * KB, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128K},
        {64 * KB, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128K},
    };

    const std::array<REVID, 5> revs{REVISION_A0, REVISION_B, REVISION_C, REVISION_D, REVISION_K};
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    for (auto rev : revs) {
        hwInfo.platform.usRevId = HwInfoConfig::get(productFamily)->getHwRevIdFromStepping(rev, hwInfo);
        if ((hwInfo.platform.eProductFamily == IGFX_DG2) && (rev == REVISION_A0)) {
            verifyPreferredSlmValues<FamilyType>(valuesToTestForDg2AStep, hwInfo);
        } else {
            verifyPreferredSlmValues<FamilyType>(valuesToTest, hwInfo);
        }
    }
}

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, GivenDebugOverrideWhenSetAdditionalInfoIsCalledThenDebugValuesAreSet, IsAtLeastXeHpgCore) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename FamilyType::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    DebugManagerStateRestore stateRestore;
    PREFERRED_SLM_ALLOCATION_SIZE debugOverrideValues[] = {PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K,
                                                           PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32K,
                                                           PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128K};

    for (auto debugOverrideValue : debugOverrideValues) {
        DebugManager.flags.OverridePreferredSlmAllocationSizePerDss.set(debugOverrideValue);
        const std::vector<PreferredSlmTestValues<FamilyType>> valuesToTest = {
            {0, debugOverrideValue},
            {32 * KB, debugOverrideValue},
            {64 * KB, debugOverrideValue},
        };
        verifyPreferredSlmValues<FamilyType>(valuesToTest, hardwareInfo);
    }
}

HWTEST2_F(CommandEncodeStatesTestDg2AndLater, givenOverridePreferredSlmAllocationSizePerDssWhenDispatchingKernelThenCorrectValueIsSet, IsAtLeastXeHpgCore) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;
    DebugManagerStateRestore restorer;
    DebugManager.flags.OverridePreferredSlmAllocationSizePerDss.set(5);
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 1;

    dispatchInterface->getSlmTotalSizeResult = slmTotalSize;

    bool requiresUncachedMocs = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
    ASSERT_NE(itor, commands.end());

    auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
    auto &idd = cmd->getInterfaceDescriptor();

    EXPECT_EQ(5u, static_cast<uint32_t>(idd.getPreferredSlmAllocationSize()));
}
