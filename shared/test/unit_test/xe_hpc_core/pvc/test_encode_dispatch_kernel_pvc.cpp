/*
 * Copyright (C) 2021-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"
#include "shared/source/xe_hpc_core/pvc/device_ids_configs_pvc.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/encoders/test_encode_dispatch_kernel_dg2_and_later.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

using namespace NEO;

using CommandEncodeStatesPvcTest = ::testing::Test;

PVCTEST_F(CommandEncodeStatesPvcTest, GivenZeroSlmSizeWhenSetAdditionalInfoIsCalledThenCorrectValuesAreSet) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;
    using PREFERRED_SLM_ALLOCATION_SIZE = typename INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];
    uint32_t threadsCount = 1;
    uint32_t slmTotalSize = 0;

    INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
    EncodeDispatchKernel<FamilyType>::setupPreferredSlmSize(&idd, rootDeviceEnvironment, threadsCount, slmTotalSize, SlmPolicy::slmPolicyNone);
    EXPECT_EQ(PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K, idd.getPreferredSlmAllocationSize());
}

using EncodeKernelPvcTest = Test<CommandEncodeStatesFixture>;

PVCTEST_F(EncodeKernelPvcTest, givenRevisionBAndAboveWhenSpecialModeRequiredThenDontReprogramPipelineSelect) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    bool requiresUncachedMocs = false;
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();

    uint32_t dims[] = {1, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode = true;

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
    auto &productHelper = pDevice->getProductHelper();
    for (const auto &testInput : testInputs) {
        for (const auto &deviceId : pvcXlDeviceIds) {
            hwInfo->platform.usDeviceID = deviceId;
            hwInfo->platform.usRevId = testInput.revId;
            cmdContainer->systolicModeSupportRef() = productHelper.isSystolicModeConfigurable(*hwInfo);
            cmdContainer->lastPipelineSelectModeRequiredRef() = false;

            EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
            dispatchArgs.preemptionMode = NEO::PreemptionMode::Initial;

            EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);
            EXPECT_EQ(testInput.expectedValue, cmdContainer->lastPipelineSelectModeRequiredRef());
        }
    }
}

PVCTEST_F(EncodeKernelPvcTest, givenRevisionBAndAboveWhenSpecialModeRequiredAndAdjustPipelineSelectCalledThenDontEnableSystolicMode) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    auto hwInfo = pDevice->getRootDeviceEnvironment().getMutableHardwareInfo();

    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    dispatchInterface->kernelDescriptor.kernelAttributes.flags.usesSystolicPipelineSelectMode = true;

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
    auto &productHelper = pDevice->getProductHelper();
    for (const auto &testInput : testInputs) {
        for (const auto &deviceId : pvcXlDeviceIds) {
            hwInfo->platform.usDeviceID = deviceId;
            hwInfo->platform.usRevId = testInput.revId;
            cmdContainer->systolicModeSupportRef() = productHelper.isSystolicModeConfigurable(*hwInfo);
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
using CommandEncodeStatesTestPvc = Test<CommandEncodeStatesFixture>;
PVCTEST_F(CommandEncodeStatesTestPvc, GivenVariousSlmTotalSizesWhenSetPreferredSlmIsCalledThenCorrectValuesAreSet) {
    using PREFERRED_SLM_ALLOCATION_SIZE = typename FamilyType::INTERFACE_DESCRIPTOR_DATA::PREFERRED_SLM_ALLOCATION_SIZE;

    const std::vector<PreferredSlmTestValues<FamilyType>> valuesToTest = {
        {0, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_0K},
        {16 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_16K},
        {32 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_32K},
        {64 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_64K},
        {96 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_96K},
        {128 * MemoryConstants::kiloByte, PREFERRED_SLM_ALLOCATION_SIZE::PREFERRED_SLM_ALLOCATION_SIZE_128K},
    };

    verifyPreferredSlmValues<FamilyType>(valuesToTest, pDevice->getRootDeviceEnvironment());
}
