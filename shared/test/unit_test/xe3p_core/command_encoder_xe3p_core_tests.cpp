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
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/libult/ult_command_stream_receiver.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/fixtures/direct_submission_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

#include "implicit_args.h"
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

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTest, givenProductHelperWithInvalidGrfNumbersWhenProgrammingIddThenErrorIsThrown) {
    struct ProductHelperWithInvalidGrfNumbers : public NEO::ProductHelperHw<IGFX_UNKNOWN> {
        const SupportedNumGrfs getSupportedNumGrfs(const NEO::ReleaseHelper *releaseHelper) const override {
            return invalidGrfs;
        }
        SupportedNumGrfs invalidGrfs = {127u, 255u};
    };

    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;

    MockExecutionEnvironment mockExecutionEnvironment{};
    RAIIProductHelperFactory<ProductHelperWithInvalidGrfNumbers> productHelperBackup{*mockExecutionEnvironment.rootDeviceEnvironments[0]};
    const auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0].get();

    INTERFACE_DESCRIPTOR_DATA_2 idd = FamilyType::cmdInitInterfaceDescriptorData2;
    size_t emptyValue = 0;

    std::vector<uint32_t> supportedNumGrfs = {128, 256};

    for (auto &numGrf : supportedNumGrfs) {
        EXPECT_THROW(EncodeDispatchKernel<FamilyType>::setGrfInfo(&idd, numGrf, emptyValue, emptyValue, rootDeviceEnvironment),
                     std::exception);
    }
}

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTest, givenPipelinedEuThreadArbitrationPolicyWhenEncodeEuSchedulingPolicyIsCalledThenIddContainsCorrectEuSchedulingPolicy) {
    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;

    INTERFACE_DESCRIPTOR_DATA_2 idd = FamilyType::cmdInitInterfaceDescriptorData2;
    KernelDescriptor kernelDescriptor;
    int32_t defaultPipelinedThreadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;

    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }

    defaultPipelinedThreadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;

    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }
    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::AgeBased;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_OLDEST_FIRST, idd.getEuThreadSchedulingModeOverride());
    }
    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }
    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobinAfterDependency;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }
}

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTest, givenPipelinedEuThreadArbitrationPolicyWhenEncodeEuSchedulingPolicyIsCalledThenIdd2ContainsCorrectEuSchedulingPolicy) {
    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;

    INTERFACE_DESCRIPTOR_DATA_2 idd = FamilyType::cmdInitInterfaceDescriptorData2;
    KernelDescriptor kernelDescriptor;
    int32_t defaultPipelinedThreadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;

    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }

    defaultPipelinedThreadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;

    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }
    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::AgeBased;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_OLDEST_FIRST, idd.getEuThreadSchedulingModeOverride());
    }
    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }
    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobinAfterDependency;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA_2::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }
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

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTest, givenEnable64BitSemaphoreFlagWhenProgrammingSemaphoreThenProperSemaphoreIsProgrammed) {
    using MI_SEMAPHORE_WAIT_64 = typename FamilyType::MI_SEMAPHORE_WAIT_64;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore debugRestorer;

    {
        debugManager.flags.Enable64BitSemaphore.set(0);

        uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
        LinearStream linearStream(buffer, sizeof(buffer));

        EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, nullptr);
        auto semaphore = genCmdCast<MI_SEMAPHORE_WAIT *>(buffer);
        EXPECT_NE(semaphore, nullptr);
    }

    {
        debugManager.flags.Enable64BitSemaphore.set(1);

        uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT_64)] = {};
        LinearStream linearStream(buffer, sizeof(buffer));

        EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, nullptr);
        auto semaphore64 = genCmdCast<MI_SEMAPHORE_WAIT_64 *>(buffer);
        EXPECT_NE(semaphore64, nullptr);
    }
}

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTest, givenEnable64BitSemaphoreFlagWhenProgrammingSemaphoreThenSetSwitchOnUnsuccessfulSwitchMode) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_SEMAPHORE_WAIT_64 = typename FamilyType::MI_SEMAPHORE_WAIT_64;

    DebugManagerStateRestore debugRestorer;
    {
        debugManager.flags.Enable64BitSemaphore.set(0);

        uint8_t buffer[2 * sizeof(MI_SEMAPHORE_WAIT)] = {};
        LinearStream linearStream(buffer, 2 * sizeof(MI_SEMAPHORE_WAIT));
        auto semaphoreCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(buffer);

        EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, nullptr);
        EXPECT_EQ(MI_SEMAPHORE_WAIT::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_AFTER_COMMAND_IS_PARSED, semaphoreCmd->getQueueSwitchMode());

        semaphoreCmd++;

        EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, true, nullptr);
        EXPECT_EQ(MI_SEMAPHORE_WAIT::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL, semaphoreCmd->getQueueSwitchMode());
    }
    {
        debugManager.flags.Enable64BitSemaphore.set(1);

        uint8_t buffer[2 * sizeof(MI_SEMAPHORE_WAIT_64)] = {};
        LinearStream linearStream(buffer, 2 * sizeof(MI_SEMAPHORE_WAIT_64));
        auto semaphoreCmd = reinterpret_cast<MI_SEMAPHORE_WAIT_64 *>(buffer);

        EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, nullptr);
        EXPECT_EQ(MI_SEMAPHORE_WAIT_64::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_AFTER_COMMAND_IS_PARSED, semaphoreCmd->getQueueSwitchMode());

        semaphoreCmd++;

        EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, true, nullptr);
        EXPECT_EQ(MI_SEMAPHORE_WAIT_64::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL, semaphoreCmd->getQueueSwitchMode());
    }
    {
        debugManager.flags.Enable64BitSemaphore.set(0);
        uint8_t buffer[2 * sizeof(MI_SEMAPHORE_WAIT)] = {};
        LinearStream linearStream(buffer, 2 * sizeof(MI_SEMAPHORE_WAIT));
        auto semaphoreCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(buffer);

        debugManager.flags.ForceSwitchQueueOnUnsuccessful.set(0);
        EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, true, nullptr);
        EXPECT_EQ(MI_SEMAPHORE_WAIT::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_AFTER_COMMAND_IS_PARSED, semaphoreCmd->getQueueSwitchMode());

        semaphoreCmd++;

        debugManager.flags.ForceSwitchQueueOnUnsuccessful.set(1);
        EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, nullptr);
        EXPECT_EQ(MI_SEMAPHORE_WAIT::QUEUE_SWITCH_MODE::QUEUE_SWITCH_MODE_SWITCH_QUEUE_ON_UNSUCCESSFUL, semaphoreCmd->getQueueSwitchMode());
    }
}

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTest, givenIndirectModeAndQwordDataWhenProgrammingSemaphoreThenEnable64bGprMode) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore debugRestorer;
    debugManager.flags.Enable64BitSemaphore.set(0);

    uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
    LinearStream linearStream(buffer, sizeof(MI_SEMAPHORE_WAIT));
    auto semaphoreCmd = reinterpret_cast<MI_SEMAPHORE_WAIT *>(buffer);

    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, nullptr);
    EXPECT_FALSE(semaphoreCmd->get64bCompareEnableWithGPR());
    EXPECT_FALSE(semaphoreCmd->getIndirectSemaphoreDataDword());

    linearStream.replaceBuffer(buffer, sizeof(MI_SEMAPHORE_WAIT));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, true, false, nullptr);
    EXPECT_FALSE(semaphoreCmd->get64bCompareEnableWithGPR());
    EXPECT_TRUE(semaphoreCmd->getIndirectSemaphoreDataDword());

    linearStream.replaceBuffer(buffer, sizeof(MI_SEMAPHORE_WAIT));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, true, false, false, nullptr);
    EXPECT_FALSE(semaphoreCmd->get64bCompareEnableWithGPR());
    EXPECT_FALSE(semaphoreCmd->getIndirectSemaphoreDataDword());

    linearStream.replaceBuffer(buffer, sizeof(MI_SEMAPHORE_WAIT));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, true, true, false, nullptr);
    EXPECT_TRUE(semaphoreCmd->get64bCompareEnableWithGPR());
    EXPECT_FALSE(semaphoreCmd->getIndirectSemaphoreDataDword());
}

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTest, givenIndirectModeAndQwordDataWhenProgrammingSemaphore64ThenEnable64bGprMode) {
    using MI_SEMAPHORE_WAIT_64 = typename FamilyType::MI_SEMAPHORE_WAIT_64;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore debugRestorer;
    debugManager.flags.Enable64BitSemaphore.set(1);

    uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT_64)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    auto semaphoreCmd = reinterpret_cast<MI_SEMAPHORE_WAIT_64 *>(buffer);

    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, false, false, nullptr);
    EXPECT_TRUE(semaphoreCmd->get64BCompareDisable());
    EXPECT_FALSE(semaphoreCmd->getIndirectSemaphoreDataDword());

    linearStream.replaceBuffer(buffer, sizeof(MI_SEMAPHORE_WAIT_64));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, false, true, false, nullptr);
    EXPECT_TRUE(semaphoreCmd->get64BCompareDisable());
    EXPECT_TRUE(semaphoreCmd->getIndirectSemaphoreDataDword());

    linearStream.replaceBuffer(buffer, sizeof(MI_SEMAPHORE_WAIT_64));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, true, false, false, nullptr);
    EXPECT_FALSE(semaphoreCmd->get64BCompareDisable());
    EXPECT_FALSE(semaphoreCmd->getIndirectSemaphoreDataDword());

    linearStream.replaceBuffer(buffer, sizeof(MI_SEMAPHORE_WAIT_64));
    EncodeSemaphore<FamilyType>::addMiSemaphoreWaitCommand(linearStream, 0x1230000, 0, MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD, false, true, true, false, nullptr);
    EXPECT_FALSE(semaphoreCmd->get64BCompareDisable());
    EXPECT_TRUE(semaphoreCmd->getIndirectSemaphoreDataDword());
}

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTest, GivenMiSemaphoreWait64WhenProgrammingWithSelectedWaitModeThenProperWaitModeIsSet) {
    using MI_SEMAPHORE_WAIT_64 = typename FamilyType::MI_SEMAPHORE_WAIT_64;
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore debugRestorer;
    debugManager.flags.Enable64BitSemaphore.set(1);

    uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT_64)] = {};
    auto miSemaphore = reinterpret_cast<MI_SEMAPHORE_WAIT *>(buffer);
    auto miSemaphore64 = reinterpret_cast<MI_SEMAPHORE_WAIT_64 *>(buffer);

    EncodeSemaphore<FamilyType>::programMiSemaphoreWait(miSemaphore,
                                                        0x123400,
                                                        4,
                                                        MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                        false,
                                                        true,
                                                        false,
                                                        false,
                                                        false);

    EXPECT_EQ(MI_SEMAPHORE_WAIT_64::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, miSemaphore64->getCompareOperation());
    EXPECT_EQ(4u, miSemaphore64->getSemaphoreDataDword());
    EXPECT_EQ(0x123400u, miSemaphore64->getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT_64::WAIT_MODE::WAIT_MODE_POLLING_MODE, miSemaphore64->getWaitMode());

    memset(buffer, 0, sizeof(MI_SEMAPHORE_WAIT));
    EXPECT_ANY_THROW(EncodeSemaphore<FamilyType>::programMiSemaphoreWait(miSemaphore,
                                                                         0x123400,
                                                                         4,
                                                                         MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                                         false,
                                                                         false,
                                                                         false,
                                                                         false,
                                                                         false));
}

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTest, GivenMiSemaphoreWaitWhenProgrammingWithSelectedWaitModeThenProperWaitModeIsSet) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;

    DebugManagerStateRestore debugRestorer;
    debugManager.flags.Enable64BitSemaphore.set(0);

    MI_SEMAPHORE_WAIT miSemaphore;
    EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&miSemaphore,
                                                        0x123400,
                                                        4,
                                                        MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                        false,
                                                        true,
                                                        false,
                                                        false,
                                                        false);

    EXPECT_EQ(MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD, miSemaphore.getCompareOperation());
    EXPECT_EQ(4u, miSemaphore.getSemaphoreDataDword());
    EXPECT_EQ(0x123400u, miSemaphore.getSemaphoreGraphicsAddress());
    EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_POLLING_MODE, miSemaphore.getWaitMode());

    memset(&miSemaphore, 0, sizeof(MI_SEMAPHORE_WAIT));
    EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&miSemaphore,
                                                        0x123400,
                                                        4,
                                                        MI_SEMAPHORE_WAIT::COMPARE_OPERATION::COMPARE_OPERATION_SAD_NOT_EQUAL_SDD,
                                                        false,
                                                        false,
                                                        false,
                                                        false,
                                                        false);
    EXPECT_EQ(MI_SEMAPHORE_WAIT::WAIT_MODE::WAIT_MODE_SIGNAL_MODE, miSemaphore.getWaitMode());
}

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTest, givenEnable64BitSemaphoreFlagWhenSettingSemaphoreValueThenProperValueIsSet) {
    DebugManagerStateRestore debugRestorer;

    const uint64_t testValue = 0x123456789ABCDEF0ull;
    {
        using MI_SEMAPHORE_WAIT_64 = typename FamilyType::MI_SEMAPHORE_WAIT_64;
        debugManager.flags.Enable64BitSemaphore.set(1);
        uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT_64)] = {};
        auto miSemaphore = reinterpret_cast<MI_SEMAPHORE_WAIT_64 *>(buffer);

        EncodeSemaphore<FamilyType>::setMiSemaphoreWaitValue(miSemaphore, testValue);
        EXPECT_EQ(testValue, miSemaphore->getSemaphoreDataDword());
    }
    {
        using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
        debugManager.flags.Enable64BitSemaphore.set(0);
        uint8_t buffer[sizeof(MI_SEMAPHORE_WAIT)] = {};
        auto miSemaphore = reinterpret_cast<MI_SEMAPHORE_WAIT *>(buffer);

        EncodeSemaphore<FamilyType>::setMiSemaphoreWaitValue(miSemaphore, testValue);
        EXPECT_EQ(static_cast<uint32_t>(testValue), miSemaphore->getSemaphoreDataDword());
    }
}

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTest, given57bitVaForDestinationAddressWhenProgrammingMiFlushDwThenVerifyAll57bitsAreUsed) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    MockExecutionEnvironment mockExecutionEnvironment{};
    const uint64_t setGpuAddress = 0xffffffffffffffff;
    const uint64_t verifyGpuAddress = 0xfffffffffffffff8;

    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;

    EncodeMiFlushDW<FamilyType>::programWithWa(linearStream, setGpuAddress, 0, args);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);

    EXPECT_EQ(verifyGpuAddress, miFlushDwCmd->getDestinationAddress());
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

XE3P_CORETEST_F(CommandEncodeStatesXe3pTest, givenHeapSharingEnabledWhenRetrievingNotInitializedSshThenExpectCorrectSbaCommand) {
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    using _3DSTATE_BINDING_TABLE_POOL_ALLOC = typename FamilyType::_3DSTATE_BINDING_TABLE_POOL_ALLOC;

    cmdContainer->enableHeapSharing();
    cmdContainer->setHeapDirty(NEO::HeapType::surfaceState);

    auto gmmHelper = cmdContainer->getDevice()->getRootDeviceEnvironment().getGmmHelper();
    uint32_t statelessMocsIndex = (gmmHelper->getL3EnabledMOCS() >> 1);

    STATE_BASE_ADDRESS sba;
    EncodeStateBaseAddressArgs<FamilyType> args = createDefaultEncodeStateBaseAddressArgs<FamilyType>(cmdContainer.get(), sba, statelessMocsIndex);

    EncodeStateBaseAddress<FamilyType>::encode(args);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands,
                                             cmdContainer->getCommandStream()->getCpuBase(),
                                             cmdContainer->getCommandStream()->getUsed());

    auto itorCmd = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_NE(commands.end(), itorCmd);
    auto sbaCmd = genCmdCast<STATE_BASE_ADDRESS *>(*itorCmd);

    EXPECT_EQ(0u, sbaCmd->getSurfaceStateBaseAddress());

    itorCmd = find<_3DSTATE_BINDING_TABLE_POOL_ALLOC *>(commands.begin(), commands.end());
    EXPECT_EQ(commands.end(), itorCmd);
}

XE3P_CORETEST_F(CommandEncodeStatesXe3pTest, givenEncodeDispatchKernelWhenGettingInlineDataOffsetInHeaplessModeThenReturnWalker2InlineOffset) {
    using WalkerType = typename FamilyType::DefaultWalkerType;

    EncodeDispatchKernelArgs dispatchArgs = {};
    dispatchArgs.isHeaplessModeEnabled = true;

    size_t expectedOffset = offsetof(WalkerType, TheStructure.Common.InlineData);

    EXPECT_EQ(expectedOffset, EncodeDispatchKernel<FamilyType>::getInlineDataOffset(dispatchArgs));
}

XE3P_CORETEST_F(CommandEncodeStatesXe3pTest, givenEncodeDispatchKernelWhenGettingInlineDataOffsetInNotHeaplessModeThenReturnWalkerInlineOffset) {
    using WalkerType = typename FamilyType::COMPUTE_WALKER;

    EncodeDispatchKernelArgs dispatchArgs = {};
    dispatchArgs.isHeaplessModeEnabled = false;

    size_t expectedOffset = offsetof(WalkerType, TheStructure.Common.InlineData);

    EXPECT_EQ(expectedOffset, EncodeDispatchKernel<FamilyType>::getInlineDataOffset(dispatchArgs));
}

XE3P_CORETEST_F(CommandEncodeStatesXe3pTest, givenEncodeSurfaceStateAndFlagEnableExtendedScratchSurfaceSizeDisabledWhenSetPitchForScratchThenPitchIsCorrect) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableExtendedScratchSurfaceSize.set(0);
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    uint32_t pitch = 128u;
    const auto &productHelper = getHelper<ProductHelper>();
    EncodeSurfaceState<FamilyType>::setPitchForScratch(&surfaceState, pitch, productHelper);
    auto pitchFromEncode = EncodeSurfaceState<FamilyType>::getPitchForScratchInBytes(&surfaceState, productHelper);
    EXPECT_EQ(pitch, pitchFromEncode);
}

XE3P_CORETEST_F(CommandEncodeStatesXe3pTest, givenDisabledFlagEnableExtendedScratchSurfaceSizeWhenCallGetPitchForScratchInBytesThenPitchIsAsSet) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableExtendedScratchSurfaceSize.set(0);
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    uint32_t pitch = 64u;
    surfaceState.setSurfacePitch(pitch);
    const auto &productHelper = getHelper<ProductHelper>();
    EXPECT_EQ(pitch, EncodeSurfaceState<FamilyType>::getPitchForScratchInBytes(&surfaceState, productHelper));
}

XE3P_CORETEST_F(CommandEncodeStatesXe3pTest, givenDisabledFlagEnableExtendedScratchSurfaceSizeWhenCallSetPitchForScratchThenPitchIsAsSetAndExtendScratchSurfaceSizeIsNotSet) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.EnableExtendedScratchSurfaceSize.set(0);
    auto surfaceState = FamilyType::cmdInitRenderSurfaceState;
    uint32_t pitch = 128u;
    const auto &productHelper = getHelper<ProductHelper>();
    EncodeSurfaceState<FamilyType>::setPitchForScratch(&surfaceState, pitch, productHelper);
    EXPECT_EQ(pitch, surfaceState.getSurfacePitch());
    EXPECT_FALSE(surfaceState.getExtendScratchSurfaceSize());
}

using WalkerDispatchTestsXe3pCore = ::testing::Test;
using Walker2DispatchTestsXe3pCore = ::testing::Test;

template <typename WalkerType, typename InterfaceDescriptorDataType, typename FamilyType>
static void whenEncodeAdditionalWalkerFieldsIsCalledThenComputeDispatchAllIsCorrectlySetFunction() {
    DebugManagerStateRestore debugRestorer;
    MockExecutionEnvironment executionEnvironment;
    auto walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();

    EncodeWalkerArgs walkerArgs{
        .kernelExecutionType = KernelExecutionType::concurrent,
        .requiredDispatchWalkOrder = NEO::RequiredDispatchWalkOrder::none,
        .maxFrontEndThreads = 113,
        .requiredSystemFence = true,
        .hasSample = false};

    {
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_TRUE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    VariableBackup<uint32_t> sliceCountBackup(&executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo()->gtSystemInfo.SliceCount, 4);

    {
        walkerArgs.kernelExecutionType = KernelExecutionType::defaultType;
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        walkerCmd.getInterfaceDescriptor().setThreadGroupDispatchSize(InterfaceDescriptorDataType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_1);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        VariableBackup<uint32_t> sliceCountBackup(&executionEnvironment.rootDeviceEnvironments[0]->getMutableHardwareInfo()->gtSystemInfo.SliceCount, 2);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        EncodeDispatchKernel<FamilyType>::template encodeComputeDispatchAllWalker<WalkerType, InterfaceDescriptorDataType>(walkerCmd, nullptr, *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        VariableBackup<uint32_t> maxFrontEndThreadsBackup(&walkerArgs.maxFrontEndThreads, 0u);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        walkerCmd.getInterfaceDescriptor().setThreadGroupDispatchSize(InterfaceDescriptorDataType::THREAD_GROUP_DISPATCH_SIZE_TG_SIZE_2);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_FALSE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }

    {
        debugManager.flags.ComputeDispatchAllWalkerEnableInComputeWalker.set(1);
        EncodeDispatchKernel<FamilyType>::encodeComputeDispatchAllWalker(walkerCmd, &walkerCmd.getInterfaceDescriptor(), *executionEnvironment.rootDeviceEnvironments[0], walkerArgs);
        EXPECT_TRUE(walkerCmd.getComputeDispatchAllWalkerEnable());
    }
}

XE3P_CORETEST_F(WalkerDispatchTestsXe3pCore, whenEncodeAdditionalWalkerFieldsIsCalledThenComputeDispatchAllIsCorrectlySet) {
    whenEncodeAdditionalWalkerFieldsIsCalledThenComputeDispatchAllIsCorrectlySetFunction<typename FamilyType::COMPUTE_WALKER, typename FamilyType::INTERFACE_DESCRIPTOR_DATA, FamilyType>();
}

XE3P_CORETEST_F(Walker2DispatchTestsXe3pCore, whenEncodeAdditionalWalkerFieldsIsCalledThenComputeDispatchAllIsCorrectlySet) {
    whenEncodeAdditionalWalkerFieldsIsCalledThenComputeDispatchAllIsCorrectlySetFunction<typename FamilyType::DefaultWalkerType, typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2, FamilyType>();
}

template <typename WalkerType, typename FamilyType>
static void givenSampleSetWhenEncodingExtraParamsThenSetCorrectFieldsFunction() {

    using DISPATCH_WALK_ORDER = typename WalkerType::DISPATCH_WALK_ORDER;
    using THREAD_GROUP_BATCH_SIZE = typename WalkerType::THREAD_GROUP_BATCH_SIZE;

    auto walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    KernelDescriptor kernelDescriptor;
    EncodeWalkerArgs walkerArgs = CommandEncodeStatesFixture::createDefaultEncodeWalkerArgs(kernelDescriptor);

    {
        walkerArgs.hasSample = false;
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);
        EXPECT_NE(DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_MORTON_WALK, walkerCmd.getDispatchWalkOrder());
        EXPECT_EQ(THREAD_GROUP_BATCH_SIZE::THREAD_GROUP_BATCH_SIZE_TG_BATCH_1, walkerCmd.getThreadGroupBatchSize());
    }

    {
        walkerArgs.hasSample = true;
        EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);
        EXPECT_EQ(DISPATCH_WALK_ORDER::DISPATCH_WALK_ORDER_MORTON_WALK, walkerCmd.getDispatchWalkOrder());
        EXPECT_EQ(THREAD_GROUP_BATCH_SIZE::THREAD_GROUP_BATCH_SIZE_TG_BATCH_4, walkerCmd.getThreadGroupBatchSize());
    }
}

XE3P_CORETEST_F(WalkerDispatchTestsXe3pCore, givenSampleSetWhenEncodingExtraParamsThenSetCorrectFields) {
    givenSampleSetWhenEncodingExtraParamsThenSetCorrectFieldsFunction<typename FamilyType::COMPUTE_WALKER, FamilyType>();
}

XE3P_CORETEST_F(Walker2DispatchTestsXe3pCore, givenSampleSetWhenEncodingExtraParamsThenSetCorrectFields) {
    givenSampleSetWhenEncodingExtraParamsThenSetCorrectFieldsFunction<typename FamilyType::DefaultWalkerType, FamilyType>();
}

XE3P_CORETEST_F(Walker2DispatchTestsXe3pCore, givenMaximumNumberOfThreadsWhenEncodingExtraParamsThenSetCorrectFields) {

    DebugManagerStateRestore restore;
    using WalkerType = typename FamilyType::DefaultWalkerType;
    auto walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0];

    KernelDescriptor kernelDescriptor;
    uint32_t maximumNumberOfThreads = 64u;
    EncodeWalkerArgs walkerArgs = CommandEncodeStatesFixture::createDefaultEncodeWalkerArgs(kernelDescriptor);
    walkerArgs.maxFrontEndThreads = maximumNumberOfThreads;

    EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);
    EXPECT_EQ(maximumNumberOfThreads, walkerCmd.getMaximumNumberOfThreads());

    uint32_t maximumNumberOfThreadsOverride = 32u;
    debugManager.flags.MaximumNumberOfThreads.set(static_cast<int32_t>(maximumNumberOfThreadsOverride));
    EncodeDispatchKernel<FamilyType>::encodeAdditionalWalkerFields(rootDeviceEnvironment, walkerCmd, walkerArgs);
    EXPECT_EQ(maximumNumberOfThreadsOverride, walkerCmd.getMaximumNumberOfThreads());
}

XE3P_CORETEST_F(Walker2DispatchTestsXe3pCore, givenDebugFlagSetWhenProgrammingAdditionalWalkerFieldsThenSetThreadArbitrationPolicy) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;
    using THREAD_ARBITRATION_POLICY = typename WalkerType::THREAD_ARBITRATION_POLICY;

    DebugManagerStateRestore restore;

    auto walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();
    auto idd = FamilyType::template getInitInterfaceDescriptor<INTERFACE_DESCRIPTOR_DATA_2>();

    EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walkerCmd, idd);

    EXPECT_EQ(THREAD_ARBITRATION_POLICY::THREAD_ARBITRATION_POLICY_ALWAYS_ROUND_ROBIN, walkerCmd.getThreadArbitrationPolicy());

    THREAD_ARBITRATION_POLICY expectedValue = THREAD_ARBITRATION_POLICY::THREAD_ARBITRATION_POLICY_ABRITRATION_SWITCH_AT_50;
    debugManager.flags.OverrideComputeWalker2ThreadArbitrationPolicy.set(static_cast<int32_t>(expectedValue));

    EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walkerCmd, idd);
    EXPECT_EQ(expectedValue, walkerCmd.getThreadArbitrationPolicy());
}

XE3P_CORETEST_F(Walker2DispatchTestsXe3pCore, givenOverDispatchControlDebugFlagWhenProgrammingAdditionalWalkerFieldsThenOverDispatchControlIsSetCorrectly) {
    using WalkerType = typename FamilyType::DefaultWalkerType;
    using INTERFACE_DESCRIPTOR_DATA_2 = typename FamilyType::INTERFACE_DESCRIPTOR_DATA_2;

    DebugManagerStateRestore restore;
    auto walkerCmd = FamilyType::template getInitGpuWalker<WalkerType>();
    auto idd = FamilyType::template getInitInterfaceDescriptor<INTERFACE_DESCRIPTOR_DATA_2>();

    EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walkerCmd, idd);
    EXPECT_EQ(WalkerType::OVER_DISPATCH_CONTROL::OVER_DISPATCH_CONTROL_NORMAL, walkerCmd.getOverDispatchControl());

    for (auto overDispatchControl : {WalkerType::OVER_DISPATCH_CONTROL::OVER_DISPATCH_CONTROL_NONE,
                                     WalkerType::OVER_DISPATCH_CONTROL::OVER_DISPATCH_CONTROL_LOW,
                                     WalkerType::OVER_DISPATCH_CONTROL::OVER_DISPATCH_CONTROL_NORMAL,
                                     WalkerType::OVER_DISPATCH_CONTROL::OVER_DISPATCH_CONTROL_HIGH}) {

        debugManager.flags.OverDispatchControl.set(overDispatchControl);
        EncodeDispatchKernel<FamilyType>::overrideDefaultValues(walkerCmd, idd);
        EXPECT_EQ(overDispatchControl, walkerCmd.getOverDispatchControl());
    }
}

XE3P_CORETEST_F(Xe3pCoreCommandEncoderTests, givenHeaplessModeEnabledWhenPatchScratchAddressInImplicitArgsIsCalledThenScratchIsPatchedCorrectly) {

    {
        ImplicitArgs implicitArgs{};
        implicitArgs.v1.header.structVersion = 1;
        implicitArgs.v1.scratchPtr = 0u;
        uint64_t scratchAddress = 0x80;

        bool scratchPtrPatchingRequired = false;
        constexpr bool heaplessModeEnabled = false;
        EncodeDispatchKernel<FamilyType>::template patchScratchAddressInImplicitArgs<heaplessModeEnabled>(implicitArgs, scratchAddress, scratchPtrPatchingRequired);

        EXPECT_NE(scratchAddress, implicitArgs.v1.scratchPtr);
    }
    {
        ImplicitArgs implicitArgs{};
        implicitArgs.v1.header.structVersion = 1;
        implicitArgs.v1.scratchPtr = 0u;
        uint64_t scratchAddress = 0x80;

        bool scratchPtrPatchingRequired = true;
        constexpr bool heaplessModeEnabled = false;
        EncodeDispatchKernel<FamilyType>::template patchScratchAddressInImplicitArgs<heaplessModeEnabled>(implicitArgs, scratchAddress, scratchPtrPatchingRequired);

        EXPECT_NE(scratchAddress, implicitArgs.v1.scratchPtr);
    }
    {
        ImplicitArgs implicitArgs{};
        implicitArgs.v1.header.structVersion = 1;
        implicitArgs.v1.scratchPtr = 0u;
        uint64_t scratchAddress = 0x80;

        bool scratchPtrPatchingRequired = false;
        constexpr bool heaplessModeEnabled = true;
        EncodeDispatchKernel<FamilyType>::template patchScratchAddressInImplicitArgs<heaplessModeEnabled>(implicitArgs, scratchAddress, scratchPtrPatchingRequired);

        EXPECT_NE(scratchAddress, implicitArgs.v1.scratchPtr);
    }
    {
        ImplicitArgs implicitArgs{};
        implicitArgs.v1.header.structVersion = 1;
        implicitArgs.v1.scratchPtr = 0u;
        uint64_t scratchAddress = 0x80;

        bool scratchPtrPatchingRequired = true;
        constexpr bool heaplessModeEnabled = true;
        EncodeDispatchKernel<FamilyType>::template patchScratchAddressInImplicitArgs<heaplessModeEnabled>(implicitArgs, scratchAddress, scratchPtrPatchingRequired);

        EXPECT_EQ(scratchAddress, implicitArgs.v1.scratchPtr);
    }
}
