/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

using namespace NEO;

template <typename GfxFamily>
struct CommandStreamReceiverHwTest : public DeviceFixture,
                                     public HardwareParse,
                                     public ::testing::Test {

    void SetUp() override {
        DeviceFixture::SetUp();
        HardwareParse::SetUp();
    }

    void TearDown() override {
        HardwareParse::TearDown();
        DeviceFixture::TearDown();
    }

    void givenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigImpl();
    void givenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigAfterUnblockingImpl();
};

template <typename GfxFamily>
void CommandStreamReceiverHwTest<GfxFamily>::givenKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigImpl() {
    typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename GfxFamily::PIPE_CONTROL PIPE_CONTROL;
    size_t GWS = 1;
    MockContext ctx(pDevice);
    MockKernelWithInternals kernel(*pDevice);
    CommandQueueHw<GfxFamily> commandQueue(&ctx, pDevice, 0);
    auto commandStreamReceiver = new MockCsrHw<GfxFamily>(*pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);

    auto &commandStreamCSR = commandStreamReceiver->getCS();

    // Mark Preamble as sent, override L3Config to invalid to programL3
    commandStreamReceiver->isPreambleSent = true;
    commandStreamReceiver->lastSentL3Config = 0;

    static_cast<MockKernel *>(kernel)->setTotalSLMSize(1024);

    cmdList.clear();
    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 0, nullptr, nullptr);

    // Parse command list to verify that PC was added to taskCS
    parseCommands<GfxFamily>(commandStreamCSR, 0);

    auto itorCmd = findMmio<GfxFamily>(cmdList.begin(), cmdList.end(), L3CNTLRegisterOffset<GfxFamily>::registerOffset);
    ASSERT_NE(cmdList.end(), itorCmd);

    auto cmdMILoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorCmd);
    ASSERT_NE(nullptr, cmdMILoad);

    // MI_LOAD_REGISTER should be preceded by PC
    EXPECT_NE(cmdList.begin(), itorCmd);
    --itorCmd;
    auto cmdPC = genCmdCast<PIPE_CONTROL *>(*itorCmd);
    ASSERT_NE(nullptr, cmdPC);

    uint32_t L3Config = PreambleHelper<GfxFamily>::getL3Config(*platformDevices[0], true);
    EXPECT_EQ(L3Config, static_cast<uint32_t>(cmdMILoad->getDataDword()));
}

template <typename GfxFamily>
void CommandStreamReceiverHwTest<GfxFamily>::givenBlockedKernelWithSlmWhenPreviousNOSLML3WasSentThenProgramL3WithSLML3ConfigAfterUnblockingImpl() {

    typedef typename GfxFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t GWS = 1;
    MockContext ctx(pDevice);
    MockKernelWithInternals kernel(*pDevice);
    CommandQueueHw<GfxFamily> commandQueue(&ctx, pDevice, 0);
    auto commandStreamReceiver = new MockCsrHw<GfxFamily>(*pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(commandStreamReceiver);
    cl_event blockingEvent;
    MockEvent<UserEvent> mockEvent(&ctx);
    blockingEvent = &mockEvent;

    auto &commandStreamCSR = commandStreamReceiver->getCS();

    uint32_t L3Config = PreambleHelper<GfxFamily>::getL3Config(*platformDevices[0], false);

    // Mark Pramble as sent, override L3Config to SLM config
    commandStreamReceiver->isPreambleSent = true;
    commandStreamReceiver->lastSentL3Config = 0;

    static_cast<MockKernel *>(kernel)->setTotalSLMSize(1024);

    commandQueue.enqueueKernel(kernel, 1, nullptr, &GWS, nullptr, 1, &blockingEvent, nullptr);

    // Expect nothing was sent
    EXPECT_EQ(0u, commandStreamCSR.getUsed());

    // Unblock Event
    mockEvent.setStatus(CL_COMPLETE);

    cmdList.clear();
    // Parse command list
    parseCommands<GfxFamily>(commandStreamCSR, 0);

    // Expect L3 was programmed
    auto itorCmd = findMmio<GfxFamily>(cmdList.begin(), cmdList.end(), L3CNTLRegisterOffset<GfxFamily>::registerOffset);
    ASSERT_NE(cmdList.end(), itorCmd);

    auto cmdMILoad = genCmdCast<MI_LOAD_REGISTER_IMM *>(*itorCmd);
    ASSERT_NE(nullptr, cmdMILoad);

    L3Config = PreambleHelper<GfxFamily>::getL3Config(*platformDevices[0], true);
    EXPECT_EQ(L3Config, static_cast<uint32_t>(cmdMILoad->getDataDword()));
}
