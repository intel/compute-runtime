/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/command_container_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_dispatch_kernel_encoder_interface.h"
#include "shared/test/unit_test/encoders/test_encode_dispatch_kernel_dg2_and_later.h"

#include "test.h"

#include "hw_cmds.h"

using namespace NEO;

using CommandEncodeStatesTestPvcAndLater = Test<CommandEncodeStatesFixture>;

HWTEST2_F(CommandEncodeStatesTestPvcAndLater, givenOverrideSlmTotalSizeDebugVariableWhenDispatchingKernelThenSharedMemorySizeIsSetCorrectly, IsAtLeastXeHpcCore) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using WALKER_TYPE = typename FamilyType::WALKER_TYPE;
    DebugManagerStateRestore restorer;
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    uint32_t slmTotalSize = 0;
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(slmTotalSize));

    bool requiresUncachedMocs = false;

    int32_t maxValueToProgram = 0xC;

    for (int32_t valueToProgram = 0x0; valueToProgram < maxValueToProgram; valueToProgram++) {
        DebugManager.flags.OverrideSlmAllocationSize.set(valueToProgram);
        cmdContainer->reset();
        uint32_t partitionCount = 0;
        EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                                 pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, false, partitionCount,
                                                 false, false);

        GenCmdList commands;
        CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());
        auto itor = find<WALKER_TYPE *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());
        auto cmd = genCmdCast<WALKER_TYPE *>(*itor);
        auto &idd = cmd->getInterfaceDescriptor();

        EXPECT_EQ(valueToProgram, idd.getSharedLocalMemorySize());
    }
}

HWCMDTEST_F(IGFX_XE_HP_CORE, CommandEncodeStatesTestPvcAndLater, givenCommandContainerWhenNumGrfRequiredIsGreaterThanDefaultThenLargeGrfModeEnabled) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    cmdContainer->lastSentNumGrfRequired = GrfConfig::DefaultGrfNumber;
    StreamProperties streamProperties{};
    streamProperties.stateComputeMode.setProperties(false, GrfConfig::LargeGrfNumber, 0u);
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*cmdContainer->getCommandStream(), streamProperties.stateComputeMode, *defaultHwInfo);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<STATE_COMPUTE_MODE *>(commands.begin(), commands.end());
    ASSERT_NE(itorCmd, commands.end());

    auto cmd = genCmdCast<STATE_COMPUTE_MODE *>(*itorCmd);
    EXPECT_TRUE(cmd->getLargeGrfMode());
}

HWTEST2_F(CommandEncodeStatesTestPvcAndLater, GivenVariousSlmTotalSizesAndSettingRevIDToDifferentValuesWhenSetAdditionalInfoIsCalledThenCorrectValuesAreSet, IsXeHpcCore) {
    using PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS = typename FamilyType::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS;

    const std::vector<PreferredSlmTestValues<FamilyType>> valuesToTest = {
        {0, PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_0K},
        {16 * KB, PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_16K},
        {32 * KB, PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_32K},
        {64 * KB, PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_64K},
        {96 * KB, PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_96K},
        {128 * KB, PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_128K},
    };

    const std::vector<PreferredSlmTestValues<FamilyType>> valuesToTestForPvcAStep = {
        {0, PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_16K},
        {16 * KB, PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_16K},
        {32 * KB, PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_32K},
        {64 * KB, PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_64K},
        {96 * KB, PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_96K},
        {128 * KB, PREFERRED_SLM_ALLOCATION_SIZE_PER_DSS::PREFERRED_SLM_SIZE_IS_128K},
    };

    const std::array<REVID, 5> revs{REVISION_A0, REVISION_B, REVISION_C, REVISION_D, REVISION_K};
    auto &hwInfo = *pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();
    for (auto rev : revs) {
        hwInfo.platform.usRevId = HwInfoConfig::get(productFamily)->getHwRevIdFromStepping(rev, hwInfo);
        if ((hwInfo.platform.eProductFamily == IGFX_PVC) && (rev == REVISION_A0)) {
            verifyPreferredSlmValues<FamilyType>(valuesToTestForPvcAStep, hwInfo);
        } else {
            verifyPreferredSlmValues<FamilyType>(valuesToTest, hwInfo);
        }
    }
}
