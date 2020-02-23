/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

using namespace NEO;

template <typename GfxFamily>
void SourceLevelDebuggerPreambleTest<GfxFamily>::givenMidThreadPreemptionAndDebuggingActiveWhenStateSipIsProgrammedThenCorrectSipKernelIsUsedTest() {
    using STATE_SIP = typename GfxFamily::STATE_SIP;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    mockDevice->setDebuggerActive(true);
    mockDevice->setPreemptionMode(PreemptionMode::MidThread);
    auto cmdSizePreemptionMidThread = PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(*mockDevice);

    StackVec<char, 4096> preambleBuffer;
    preambleBuffer.resize(cmdSizePreemptionMidThread);
    LinearStream preambleStream(&*preambleBuffer.begin(), preambleBuffer.size());

    PreemptionHelper::programStateSip<GfxFamily>(preambleStream, *mockDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<GfxFamily>(preambleStream);
    auto itorStateSip = find<STATE_SIP *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorStateSip);
    STATE_SIP *stateSipCmd = (STATE_SIP *)*itorStateSip;
    auto sipAddress = stateSipCmd->getSystemInstructionPointer();

    auto sipType = SipKernel::getSipKernelType(mockDevice->getHardwareInfo().platform.eRenderCoreFamily, mockDevice->isDebuggerActive());
    EXPECT_EQ(mockDevice->getExecutionEnvironment()->getBuiltIns()->getSipKernel(sipType, *mockDevice).getSipAllocation()->getGpuAddressToPatch(), sipAddress);
}

template <typename GfxFamily>
void SourceLevelDebuggerPreambleTest<GfxFamily>::givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleIsProgrammedThenCorrectSipKernelIsUsedTest() {
    using STATE_SIP = typename GfxFamily::STATE_SIP;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    mockDevice->setDebuggerActive(false);
    mockDevice->setPreemptionMode(PreemptionMode::MidThread);
    auto cmdSizePreemptionMidThread = PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(*mockDevice);

    StackVec<char, 4096> preambleBuffer;
    preambleBuffer.resize(cmdSizePreemptionMidThread);
    LinearStream preambleStream(&*preambleBuffer.begin(), preambleBuffer.size());

    PreemptionHelper::programStateSip<GfxFamily>(preambleStream, *mockDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<GfxFamily>(preambleStream);
    auto itorStateSip = find<STATE_SIP *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorStateSip);
    STATE_SIP *stateSipCmd = (STATE_SIP *)*itorStateSip;
    auto sipAddress = stateSipCmd->getSystemInstructionPointer();

    auto sipType = SipKernel::getSipKernelType(mockDevice->getHardwareInfo().platform.eRenderCoreFamily, mockDevice->isDebuggerActive());
    EXPECT_EQ(mockDevice->getExecutionEnvironment()->getBuiltIns()->getSipKernel(sipType, *mockDevice).getSipAllocation()->getGpuAddressToPatch(), sipAddress);
}

template <typename GfxFamily>
void SourceLevelDebuggerPreambleTest<GfxFamily>::givenPreemptionDisabledAndDebuggingActiveWhenPreambleIsProgrammedThenCorrectSipKernelIsUsedTest() {
    using STATE_SIP = typename GfxFamily::STATE_SIP;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    mockDevice->setDebuggerActive(true);
    mockDevice->setPreemptionMode(PreemptionMode::Disabled);
    auto cmdSizePreemptionMidThread = PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(*mockDevice);

    StackVec<char, 4096> preambleBuffer;
    preambleBuffer.resize(cmdSizePreemptionMidThread);
    LinearStream preambleStream(&*preambleBuffer.begin(), preambleBuffer.size());

    PreemptionHelper::programStateSip<GfxFamily>(preambleStream, *mockDevice);

    HardwareParse hwParser;
    hwParser.parseCommands<GfxFamily>(preambleStream);
    auto itorStateSip = find<STATE_SIP *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    ASSERT_NE(hwParser.cmdList.end(), itorStateSip);
    STATE_SIP *stateSipCmd = (STATE_SIP *)*itorStateSip;
    auto sipAddress = stateSipCmd->getSystemInstructionPointer();

    auto sipType = SipKernel::getSipKernelType(mockDevice->getHardwareInfo().platform.eRenderCoreFamily, mockDevice->isDebuggerActive());
    EXPECT_EQ(mockDevice->getExecutionEnvironment()->getBuiltIns()->getSipKernel(sipType, *mockDevice).getSipAllocation()->getGpuAddressToPatch(), sipAddress);
}

template <typename GfxFamily>
void SourceLevelDebuggerPreambleTest<GfxFamily>::givenMidThreadPreemptionAndDebuggingActiveWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest() {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    mockDevice->setDebuggerActive(true);
    mockDevice->setPreemptionMode(PreemptionMode::MidThread);
    size_t requiredPreambleSize = PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(*mockDevice);
    auto sizeExpected = sizeof(typename GfxFamily::STATE_SIP);
    EXPECT_EQ(sizeExpected, requiredPreambleSize);
}

template <typename GfxFamily>
void SourceLevelDebuggerPreambleTest<GfxFamily>::givenPreemptionDisabledAndDebuggingActiveWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest() {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    mockDevice->setDebuggerActive(true);
    mockDevice->setPreemptionMode(PreemptionMode::Disabled);
    size_t requiredPreambleSize = PreemptionHelper::getRequiredStateSipCmdSize<GfxFamily>(*mockDevice);
    auto sizeExpected = sizeof(typename GfxFamily::STATE_SIP);
    EXPECT_EQ(sizeExpected, requiredPreambleSize);
}

template <typename GfxFamily>
void SourceLevelDebuggerPreambleTest<GfxFamily>::givenMidThreadPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest() {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    mockDevice->setDebuggerActive(false);
    mockDevice->setPreemptionMode(PreemptionMode::MidThread);
    size_t requiredPreambleSize = PreemptionHelper::getRequiredPreambleSize<GfxFamily>(*mockDevice);
    auto sizeExpected = sizeof(typename GfxFamily::GPGPU_CSR_BASE_ADDRESS);
    EXPECT_EQ(sizeExpected, requiredPreambleSize);
}

template <typename GfxFamily>
void SourceLevelDebuggerPreambleTest<GfxFamily>::givenDisabledPreemptionAndDisabledDebuggingWhenPreambleSizeIsQueriedThenCorrecrSizeIsReturnedTest() {
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));
    mockDevice->setDebuggerActive(false);
    mockDevice->setPreemptionMode(PreemptionMode::Disabled);
    size_t requiredPreambleSize = PreemptionHelper::getRequiredPreambleSize<GfxFamily>(*mockDevice);
    size_t sizeExpected = 0u;
    EXPECT_EQ(sizeExpected, requiredPreambleSize);
}

template <typename GfxFamily>
void SourceLevelDebuggerPreambleTest<GfxFamily>::givenKernelDebuggingActiveAndDisabledPreemptionWhenGetAdditionalCommandsSizeIsCalledThen2MiLoadRegisterImmCmdsAreInlcudedTest() {
    DebugManagerStateRestore dbgRestore;
    DebugManager.flags.ForcePreemptionMode.set(static_cast<int32_t>(PreemptionMode::Disabled));
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    mockDevice->setDebuggerActive(false);
    size_t withoutDebugging = PreambleHelper<GfxFamily>::getAdditionalCommandsSize(*mockDevice);
    mockDevice->setDebuggerActive(true);
    size_t withDebugging = PreambleHelper<GfxFamily>::getAdditionalCommandsSize(*mockDevice);
    EXPECT_LT(withoutDebugging, withDebugging);

    size_t diff = withDebugging - withoutDebugging;
    size_t sizeExpected = 2 * sizeof(typename GfxFamily::MI_LOAD_REGISTER_IMM);
    EXPECT_EQ(sizeExpected, diff);
}

template class SourceLevelDebuggerPreambleTest<GfxFamily>;
