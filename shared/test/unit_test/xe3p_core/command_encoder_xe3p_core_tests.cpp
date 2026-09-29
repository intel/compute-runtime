/*
 * Copyright (C) 2025-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_container/encode_surface_state.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/in_order_cmd_helpers.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_memory_manager.h"
#include "shared/test/common/mocks/mock_timestamp_container.h"
#include "shared/test/common/test_macros/header/common_matchers.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/encoders/test_encode_slm_xe2_and_later.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/fixtures/direct_submission_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

#include "per_product_test_definitions.h"

using namespace NEO;

using Xe3pCoreCommandEncoderTest = ::testing::Test;

using REGISTERS_PER_THREAD = typename Xe3pCoreFamily::INTERFACE_DESCRIPTOR_DATA_2::REGISTERS_PER_THREAD;
struct NumGrfsForIddXe3p {
    bool operator==(uint32_t numGrf) const { return this->numGrf == numGrf; }
    uint32_t numGrf;
    REGISTERS_PER_THREAD valueForIdd;
};

constexpr std::array<NumGrfsForIddXe3p, 8> validNumGrfsForIdd{{{32u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_32},
                                                               {64u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_64},
                                                               {96u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_96},
                                                               {128u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_128},
                                                               {160u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_160},
                                                               {192u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_192},
                                                               {256u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_256},
                                                               {512u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_512}}};

static std::vector<NumGrfsForIddXe3p> getSupportedNumGrfsForIdd(const RootDeviceEnvironment &rootDeviceEnvironment) {
    const auto &productHelper = rootDeviceEnvironment.template getHelper<ProductHelper>();
    const auto supportedNumGrfs = productHelper.getSupportedNumGrfs(rootDeviceEnvironment.getReleaseHelper());

    std::vector<NumGrfsForIddXe3p> supportedNumGrfsForIdd;
    for (const auto &supportedNumGrf : supportedNumGrfs) {
        auto value = std::find(validNumGrfsForIdd.begin(), validNumGrfsForIdd.end(), supportedNumGrf);
        if (value != validNumGrfsForIdd.end()) {
            supportedNumGrfsForIdd.push_back(*value);
        }
    }

    return supportedNumGrfsForIdd;
}

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTest, givenGrfSizeWhenProgrammingIddThenSetCorrectNumRegistersPerThread) {
    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;

    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0].get();

    INTERFACE_DESCRIPTOR_DATA_2 idd = FamilyType::cmdInitInterfaceDescriptorData2;
    size_t emptyValue = 0;

    std::vector<NumGrfsForIddXe3p> supportedNumGrfsForIdd = getSupportedNumGrfsForIdd(rootDeviceEnvironment);

    for (auto &value : supportedNumGrfsForIdd) {
        EncodeDispatchKernel<FamilyType>::setGrfInfo(&idd, value.numGrf, emptyValue, emptyValue, rootDeviceEnvironment);

        EXPECT_EQ(value.valueForIdd, idd.getRegistersPerThread());
    }

    EXPECT_THROW(EncodeDispatchKernel<FamilyType>::setGrfInfo(&idd, 1024, emptyValue, emptyValue, rootDeviceEnvironment),
                 std::exception);
}

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTest, givenDebugVariableSetWhenProgrammingInterfaceDescriptorThenSetDynamicSlmSizePerIncrease) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;
    using DYNAMIC_PREF_SLM_INCREASE = typename INTERFACE_DESCRIPTOR_DATA_2::DYNAMIC_PREF_SLM_INCREASE;

    DebugManagerStateRestore debugRestorer;

    DefaultWalkerType walker = FamilyType::cmdInitGpgpuWalker2;
    INTERFACE_DESCRIPTOR_DATA_2 idd = FamilyType::cmdInitInterfaceDescriptorData2;
    EXPECT_EQ(DYNAMIC_PREF_SLM_INCREASE::DYNAMIC_PREF_SLM_INCREASE_DISABLE, idd.getDynamicPrefSlmIncrease());

    EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walker, idd);
    EXPECT_EQ(DYNAMIC_PREF_SLM_INCREASE::DYNAMIC_PREF_SLM_INCREASE_MAX_FULL, idd.getDynamicPrefSlmIncrease());

    debugManager.flags.OverrideDynamicPrefSlmIncrease.set(static_cast<int32_t>(DYNAMIC_PREF_SLM_INCREASE::DYNAMIC_PREF_SLM_INCREASE_MAX_2X));

    EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walker, idd);
    EXPECT_EQ(DYNAMIC_PREF_SLM_INCREASE::DYNAMIC_PREF_SLM_INCREASE_MAX_2X, idd.getDynamicPrefSlmIncrease());
}

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTest, givenXe3pWhenEncodeAdditionalWalkerFieldsIsCalledThenComputeOverdispatchDisableIsCorrectlySet) {
    DebugManagerStateRestore debugRestorer;

    using WalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;

    WalkerType walkerCmd = FamilyType::cmdInitGpgpuWalker2;
    INTERFACE_DESCRIPTOR_DATA_2 idd = FamilyType::cmdInitInterfaceDescriptorData2;

    {
        debugManager.flags.ComputeOverdispatchDisable.set(-1);
        EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walkerCmd, idd);
        EXPECT_TRUE(walkerCmd.getComputeOverdispatchDisable());
    }
    {
        debugManager.flags.ComputeOverdispatchDisable.set(0);
        EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walkerCmd, idd);
        EXPECT_FALSE(walkerCmd.getComputeOverdispatchDisable());
    }

    {
        debugManager.flags.ComputeOverdispatchDisable.set(1);
        EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walkerCmd, idd);
        EXPECT_TRUE(walkerCmd.getComputeOverdispatchDisable());
    }
}

using Xe3pCoreCommandEncoderTests = Test<CommandEncodeStatesFixture>;

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTests, whenEncodeDispatchKernelThenCorrectGrfInfoIsProgrammedInInterfaceDescriptorData) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    constexpr uint32_t dims[] = {1, 1, 1};
    constexpr bool requiresUncachedMocs = false;

    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0].get();

    std::vector<NumGrfsForIddXe3p> supportedNumGrfsForIdd = getSupportedNumGrfsForIdd(rootDeviceEnvironment);

    for (auto &value : supportedNumGrfsForIdd) {

        std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
        dispatchInterface->kernelDescriptor.kernelAttributes.numGrfRequired = value.numGrf;

        EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);

        EncodeDispatchKernel<FamilyType>::template encode<DefaultWalkerType>(*cmdContainer.get(), dispatchArgs);

        GenCmdList commands;
        auto commandStream = cmdContainer->getCommandStream();
        CmdParse<FamilyType>::parseCommandBuffer(commands, commandStream->getCpuBase(), commandStream->getUsed());

        auto itor = find<DefaultWalkerType *>(commands.begin(), commands.end());
        ASSERT_NE(itor, commands.end());

        auto walkerCmd = genCmdCast<DefaultWalkerType *>(*itor);
        auto &idd = walkerCmd->getInterfaceDescriptor();
        EXPECT_EQ(value.valueForIdd, idd.getRegistersPerThread());

        memset(commandStream->getCpuBase(), 0u, commandStream->getUsed());
        commandStream->replaceBuffer(commandStream->getCpuBase(), commandStream->getMaxAvailableSpace());
    }
}

using CommandEncodeStatesXe3pTest = Test<CommandEncodeStatesFixture>;

XE3P_CORETEST_F(CommandEncodeStatesXe3pTest, givenEncodeDispatchKernelWhenGettingInlineDataOffsetInHeaplessModeThenReturnWalker2InlineOffset) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    EncodeDispatchKernelArgs dispatchArgs = {};
    dispatchArgs.isHeaplessModeEnabled = true;

    size_t expectedOffset = offsetof(WalkerType, TheStructure.Common.InlineData);

    EXPECT_EQ(expectedOffset, EncodeDispatchKernel<FamilyType>::getInlineDataOffset(dispatchArgs));
}

using WalkerDispatchTestsXe3pCore = ::testing::Test;

const std::vector<uint32_t> slmSizesPerThreadGroupXe3pIgpu = slmSizesInBytes({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 24, 32, 48, 64, 96, 128, 192});
const std::vector<uint32_t> slmSizesPerThreadGroupCri = slmSizesInBytes({0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 24, 32, 48, 64, 96, 128, 192, 256, 320, 384});

HWTEST2_F(CommandEncodeStatesSlmTestXe2AndLater, GivenSlmTotalSizePerThreadGroupEdgeValuesWhenCallingAlignSlmSizePerThreadGroupThenSizeIsAlignedUpToTheNextProgrammableSize, IsXe3pLpg) {
    verifySlmSizePerThreadGroupAlignment<FamilyType>(slmSizesPerThreadGroupXe3pIgpu);
}

HWTEST2_F(CommandEncodeStatesSlmTestXe2AndLater, GivenSlmTotalSizePerThreadGroupEdgeValuesWhenCallingAlignSlmSizePerThreadGroupThenSizeIsAlignedUpToTheNextProgrammableSize, IsCRI) {
    verifySlmSizePerThreadGroupAlignment<FamilyType>(slmSizesPerThreadGroupCri);
}
