/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/direct_submission/dispatchers/render_dispatcher.h"
#include "shared/source/gmm_helper/gmm_helper.h"
#include "shared/source/gmm_helper/gmm_lib.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/flush_stamp.h"
#include "shared/source/helpers/simd_helper.h"
#include "shared/source/os_interface/product_helper_hw.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/raii_product_helper.h"
#include "shared/test/common/mocks/mock_direct_submission_hw.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/fixtures/direct_submission_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

#include "hw_cmds_xe3_core.h"

using namespace NEO;

using Xe3CoreCommandEncoderTest = ::testing::Test;

using REGISTERS_PER_THREAD = typename Xe3CoreFamily::INTERFACE_DESCRIPTOR_DATA::REGISTERS_PER_THREAD;
struct NumGrfsForIdd {
    bool operator==(uint32_t numGrf) const { return this->numGrf == numGrf; }
    uint32_t numGrf;
    REGISTERS_PER_THREAD valueForIdd;
};

constexpr std::array<NumGrfsForIdd, 8> validNumGrfsForIdd{{{32u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_32},
                                                           {64u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_64},
                                                           {96u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_96},
                                                           {128u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_128},
                                                           {160u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_160},
                                                           {192u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_192},
                                                           {256u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_256},
                                                           {512u, REGISTERS_PER_THREAD::REGISTERS_PER_THREAD_REGISTERS_512}}};

static std::vector<NumGrfsForIdd> getSupportedNumGrfsForIdd(const RootDeviceEnvironment &rootDeviceEnvironment) {
    const auto &productHelper = rootDeviceEnvironment.template getHelper<ProductHelper>();
    const auto supportedNumGrfs = productHelper.getSupportedNumGrfs(rootDeviceEnvironment.getReleaseHelper());

    std::vector<NumGrfsForIdd> supportedNumGrfsForIdd;
    for (const auto &supportedNumGrf : supportedNumGrfs) {
        auto value = std::find(validNumGrfsForIdd.begin(), validNumGrfsForIdd.end(), supportedNumGrf);
        if (value != validNumGrfsForIdd.end()) {
            supportedNumGrfsForIdd.push_back(*value);
        }
    }

    return supportedNumGrfsForIdd;
}

XE3_CORETEST_F(Xe3CoreCommandEncoderTest, givenGrfSizeWhenProgrammingIddThenSetCorrectNumRegistersPerThread) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0].get();

    INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
    size_t emptyValue = 0;

    std::vector<NumGrfsForIdd> supportedNumGrfsForIdd = getSupportedNumGrfsForIdd(rootDeviceEnvironment);

    for (auto &value : supportedNumGrfsForIdd) {
        EncodeDispatchKernel<FamilyType>::setGrfInfo(&idd, value.numGrf, emptyValue, emptyValue, rootDeviceEnvironment);

        EXPECT_EQ(value.valueForIdd, idd.getRegistersPerThread());
    }

    EXPECT_THROW(EncodeDispatchKernel<FamilyType>::setGrfInfo(&idd, 1024, emptyValue, emptyValue, rootDeviceEnvironment),
                 std::exception);
}

XE3_CORETEST_F(Xe3CoreCommandEncoderTest, givenProductHelperWithInvalidGrfNumbersWhenProgrammingIddThenErrorIsThrown) {
    struct ProductHelperWithInvalidGrfNumbers : public NEO::ProductHelperHw<IGFX_UNKNOWN> {
        std::vector<uint32_t> getSupportedNumGrfs(const NEO::ReleaseHelper *releaseHelper) const override {
            return invalidGrfs;
        }
        std::vector<uint32_t> invalidGrfs = {127, 255};
    };

    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    MockExecutionEnvironment mockExecutionEnvironment{};
    RAIIProductHelperFactory<ProductHelperWithInvalidGrfNumbers> productHelperBackup{*mockExecutionEnvironment.rootDeviceEnvironments[0]};
    const auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0].get();

    INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
    size_t emptyValue = 0;

    std::vector<uint32_t> supportedNumGrfs = {128, 256};

    for (auto &numGrf : supportedNumGrfs) {
        EXPECT_THROW(EncodeDispatchKernel<FamilyType>::setGrfInfo(&idd, numGrf, emptyValue, emptyValue, rootDeviceEnvironment),
                     std::exception);
    }
}

XE3_CORETEST_F(Xe3CoreCommandEncoderTest, given57bitVaForDestinationAddressWhenProgrammingMiFlushDwThenVerifyAll57bitsAreUsed) {
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

using Xe3CoreCommandEncoderTests = Test<CommandEncodeStatesFixture>;

XE3_CORETEST_F(Xe3CoreCommandEncoderTests, whenEncodeDispatchKernelThenCorrectGrfInfoIsProgrammedInInterfaceDescriptorData) {
    using DefaultWalkerType = typename FamilyType::DefaultWalkerType;

    constexpr uint32_t dims[] = {1, 1, 1};
    constexpr bool requiresUncachedMocs = false;

    MockExecutionEnvironment mockExecutionEnvironment{};
    const auto &rootDeviceEnvironment = *mockExecutionEnvironment.rootDeviceEnvironments[0].get();

    std::vector<NumGrfsForIdd> supportedNumGrfsForIdd = getSupportedNumGrfsForIdd(rootDeviceEnvironment);

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

using CommandEncodeStatesXe3Test = Test<CommandEncodeStatesFixture>;

XE3_CORETEST_F(CommandEncodeStatesXe3Test, givenHeapSharingEnabledWhenRetrievingNotInitializedSshThenExpectCorrectSbaCommand) {
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

XE3_CORETEST_F(Xe3CoreCommandEncoderTest, givenPipelinedEuThreadArbitrationPolicyWhenEncodeEuSchedulingPolicyIsCalledThenIddContainsCorrectEuSchedulingPolicy) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA idd = FamilyType::cmdInitInterfaceDescriptorData;
    KernelDescriptor kernelDescriptor;
    int32_t defaultPipelinedThreadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;

    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }

    defaultPipelinedThreadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;

    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }
    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::AgeBased;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_OLDEST_FIRST, idd.getEuThreadSchedulingModeOverride());
    }
    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobin;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }
    {
        kernelDescriptor.kernelAttributes.threadArbitrationPolicy = ThreadArbitrationPolicy::RoundRobinAfterDependency;
        EncodeDispatchKernel<FamilyType>::encodeEuSchedulingPolicy(&idd, kernelDescriptor, defaultPipelinedThreadArbitrationPolicy);
        EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::EU_THREAD_SCHEDULING_MODE_OVERRIDE::EU_THREAD_SCHEDULING_MODE_OVERRIDE_STALL_BASED_ROUND_ROBIN, idd.getEuThreadSchedulingModeOverride());
    }
}
