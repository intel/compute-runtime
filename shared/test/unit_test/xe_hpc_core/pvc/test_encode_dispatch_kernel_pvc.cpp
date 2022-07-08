/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/command_container_fixture.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_dispatch_kernel_encoder_interface.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using CommandEncodeStatesPvcTest = ::testing::Test;

PVCTEST_F(CommandEncodeStatesPvcTest, GivenSmallSlmTotalSizesWhenSetAdditionalInfoIsCalledThenCorrectValuesAreSet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    HardwareInfo hwInfo = *defaultHwInfo;
    uint32_t threadsCount = 1;
    uint32_t slmTotalSize = 0;

    struct {
        unsigned short revisionId;
        bool isWaRequired;
    } revisionsToTest[] = {
        {0x0, true},
        {0x1, true},
        {0x2, true},
        {0x41, true},
        {0x3, false},
        {0x9d, false},
    };

    for (const auto &revisionToTest : revisionsToTest) {
        for (const auto &deviceId : pvcXlDeviceIds) {
            hwInfo.platform.usDeviceID = deviceId;
            hwInfo.platform.usRevId = revisionToTest.revisionId;
            INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
            EncodeDispatchKernel<FamilyType>::appendAdditionalIDDFields(&idd, hwInfo, threadsCount, slmTotalSize, SlmPolicy::SlmPolicyNone);
            if (revisionToTest.isWaRequired) {
                EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16K, idd.getPreferredSlmAllocationSize());
            } else {
                EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K, idd.getPreferredSlmAllocationSize());
            }
        }
    }
}

using EncodeKernelPvcTest = Test<CommandEncodeStatesFixture>;

PVCTEST_F(EncodeKernelPvcTest, givenRevisionBAndAboveWhenSpecialModeRequiredThenDontReprogramPipelineSelect) {
    bool requiresUncachedMocs = false;
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.usesSpecialPipelineSelectMode = true;

    struct {
        unsigned short revId;
        bool expectedValue;
    } testInputs[] = {
        {0x0, true},
        {0x1, true},
        {0x3, true},
        {0x5, false},
        {0x6, false},
        {0x7, false},
    };
    for (const auto &testInput : testInputs) {
        for (const auto &deviceId : pvcXlDeviceIds) {
            hwInfo->platform.usDeviceID = deviceId;
            hwInfo->platform.usRevId = testInput.revId;
            cmdContainer->lastPipelineSelectModeRequired = false;

            EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
            dispatchArgs.preemptionMode = NEO::PreemptionMode::Initial;

            EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs);
            EXPECT_EQ(testInput.expectedValue, cmdContainer->lastPipelineSelectModeRequired);
        }
    }
}

PVCTEST_F(EncodeKernelPvcTest, givenRevisionBAndAboveWhenSpecialModeRequiredAndAdjustPipelineSelectCalledThenDontEnableSystolicMode) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();

    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.usesSpecialPipelineSelectMode = true;

    struct {
        unsigned short revId;
        bool expectedValue;
    } testInputs[] = {
        {0x0, true},
        {0x1, true},
        {0x3, true},
        {0x5, false},
        {0x6, false},
        {0x7, false},
    };
    for (const auto &testInput : testInputs) {
        for (const auto &deviceId : pvcXlDeviceIds) {
            hwInfo->platform.usDeviceID = deviceId;
            hwInfo->platform.usRevId = testInput.revId;
            EncodeComputeMode<FamilyType>::adjustPipelineSelect(*cmdContainer.get(), dispatchInterface->kernelDescriptor);
            GenCmdList commands;
            CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

            auto itor = find<PIPELINE_SELECT *>(commands.begin(), commands.end());
            ASSERT_NE(itor, commands.end());

            auto pipelineSelectCmd = genCmdCast<PIPELINE_SELECT *>(*itor);
            EXPECT_EQ(testInput.expectedValue, pipelineSelectCmd->getSystolicModeEnable());
            cmdContainer->reset();
        }
    }
}