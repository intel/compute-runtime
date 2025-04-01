/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/built_ins/sip.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/helpers/compiler_product_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/mocks/mock_builtins.h"
#include "shared/test/common/mocks/mock_csr.h"
#include "shared/test/common/mocks/mock_debugger.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

#include "test_traits_common.h"

using namespace NEO;

using XeHPAndLaterPreemptionTests = DevicePreemptionTests;

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterPreemptionTests, whenProgramStateSipIsCalledThenStateSipCmdIsNotAddedToStream) {
    size_t requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*device, false);
    EXPECT_EQ(0U, requiredSize);

    LinearStream cmdStream{nullptr, 0};
    PreemptionHelper::programStateSip<FamilyType>(cmdStream, *device, nullptr);
    EXPECT_EQ(0U, cmdStream.getUsed());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterPreemptionTests, WhenProgrammingPreemptionThenExpectLoadRegisterCommandRemapFlagEnabled) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    const size_t bufferSize = 128;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);

    PreemptionHelper::programCmdStream<FamilyType>(cmdStream, PreemptionMode::ThreadGroup, PreemptionMode::Initial, nullptr);
    auto lriCommand = genCmdCast<MI_LOAD_REGISTER_IMM *>(cmdStream.getCpuBase());
    ASSERT_NE(nullptr, lriCommand);
    EXPECT_TRUE(lriCommand->getMmioRemapEnable());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterPreemptionTests, GivenDebuggerUsedWhenProgrammingStateSipThenStateSipIsAdded) {
    using STATE_SIP = typename FamilyType::STATE_SIP;
    device->executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(new MockDebugger);
    auto &compilerProductHelper = device->getCompilerProductHelper();

    auto sipType = SipKernel::getSipKernelType(*device.get());
    SipKernel::initSipKernel(sipType, *device.get());

    size_t requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*device, false);
    EXPECT_EQ(sizeof(STATE_SIP), requiredSize);

    const size_t bufferSize = 128;
    uint64_t buffer[bufferSize];

    LinearStream cmdStream{buffer, bufferSize * sizeof(uint64_t)};
    PreemptionHelper::programStateSip<FamilyType>(cmdStream, *device, nullptr);
    EXPECT_EQ(sizeof(STATE_SIP), cmdStream.getUsed());

    auto sipAllocation = SipKernel::getSipKernel(*device, nullptr).getSipAllocation();
    auto sipCommand = genCmdCast<STATE_SIP *>(cmdStream.getCpuBase());
    auto sipAddress = sipCommand->getSystemInstructionPointer();

    auto expectedAddress = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo) ? sipAllocation->getGpuAddress() : sipAllocation->getGpuAddressToPatch();
    EXPECT_EQ(expectedAddress, sipAddress);

    SipKernel::freeSipKernels(&device->getRootDeviceEnvironmentRef(), device->getMemoryManager());
}

HWCMDTEST_F(IGFX_XE_HP_CORE, XeHPAndLaterPreemptionTests, GivenOfflineModeDebuggerWhenProgrammingStateSipWithContextThenStateSipIsAdded) {
    using STATE_SIP = typename FamilyType::STATE_SIP;
    VariableBackup<bool> mockSipBackup{&MockSipData::useMockSip, false};
    auto executionEnvironment = device->getExecutionEnvironment();
    auto builtIns = new NEO::MockBuiltins();
    builtIns->callBaseGetSipKernel = true;
    MockRootDeviceEnvironment::resetBuiltins(executionEnvironment->rootDeviceEnvironments[0].get(), builtIns);
    executionEnvironment->rootDeviceEnvironments[0]->debugger.reset(new MockDebugger);
    device->executionEnvironment->setDebuggingMode(DebuggingMode::offline);
    device->setPreemptionMode(MidThread);
    auto &compilerProductHelper = device->getCompilerProductHelper();

    const uint32_t contextId = 0u;
    std::unique_ptr<OsContext> osContext(OsContext::create(executionEnvironment->rootDeviceEnvironments[0]->osInterface.get(),
                                                           device->getRootDeviceIndex(), contextId,
                                                           EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_BCS, EngineUsage::regular}, PreemptionMode::MidThread, device->getDeviceBitfield())));
    osContext->setDefaultContext(true);

    auto csr = device->createCommandStreamReceiver();
    csr->setupContext(*osContext);

    const size_t bufferSize = 128;
    uint64_t buffer[bufferSize];

    LinearStream cmdStream{buffer, bufferSize * sizeof(uint64_t)};
    PreemptionHelper::programStateSip<FamilyType>(cmdStream, *device, osContext.get());
    EXPECT_EQ(sizeof(STATE_SIP), cmdStream.getUsed());

    auto contextSipKernel = builtIns->perContextSipKernels[contextId].first.get();
    auto sipAllocation = contextSipKernel->getSipAllocation();

    auto sipCommand = genCmdCast<STATE_SIP *>(cmdStream.getCpuBase());
    auto sipAddress = sipCommand->getSystemInstructionPointer();

    auto expectedAddress = compilerProductHelper.isHeaplessModeEnabled(*defaultHwInfo) ? sipAllocation->getGpuAddress() : sipAllocation->getGpuAddressToPatch();
    EXPECT_EQ(expectedAddress, sipAddress);
}
