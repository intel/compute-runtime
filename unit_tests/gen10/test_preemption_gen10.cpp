/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/built_ins/built_ins.h"
#include "runtime/command_stream/preemption.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/preemption_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"

namespace OCLRT {

template <>
void HardwareParse::findCsrBaseAddress<CNLFamily>() {
    typedef typename GEN10::GPGPU_CSR_BASE_ADDRESS GPGPU_CSR_BASE_ADDRESS;
    itorGpgpuCsrBaseAddress = find<GPGPU_CSR_BASE_ADDRESS *>(cmdList.begin(), itorWalker);
    if (itorGpgpuCsrBaseAddress != itorWalker) {
        cmdGpgpuCsrBaseAddress = *itorGpgpuCsrBaseAddress;
    }
}
} // namespace OCLRT

using namespace OCLRT;

using Gen10PreemptionTests = DevicePreemptionTests;
using Gen10PreemptionEnqueueKernelTest = PreemptionEnqueueKernelTest;
using Gen10MidThreadPreemptionEnqueueKernelTest = MidThreadPreemptionEnqueueKernelTest;
using Gen10ThreadGroupPreemptionEnqueueKernelTest = ThreadGroupPreemptionEnqueueKernelTest;

template <>
PreemptionTestHwDetails GetPreemptionTestHwDetails<CNLFamily>() {
    PreemptionTestHwDetails ret;
    ret.modeToRegValueMap[PreemptionMode::ThreadGroup] = DwordBuilder::build(1, true) | DwordBuilder::build(2, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidBatch] = DwordBuilder::build(2, true) | DwordBuilder::build(1, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidThread] = DwordBuilder::build(2, true, false) | DwordBuilder::build(1, true, false);
    ret.defaultRegValue = ret.modeToRegValueMap[PreemptionMode::MidBatch];
    ret.regAddress = 0x2580u;
    return ret;
}

GEN10TEST_F(Gen10PreemptionTests, whenMidThreadPreemptionIsNotAvailableThenDoesNotProgramPreamble) {
    device->setPreemptionMode(PreemptionMode::ThreadGroup);

    size_t requiredSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*device);
    EXPECT_EQ(0U, requiredSize);

    LinearStream cmdStream{nullptr, 0};
    PreemptionHelper::programPreamble<FamilyType>(cmdStream, *device, nullptr);
    EXPECT_EQ(0U, cmdStream.getUsed());
}

GEN10TEST_F(Gen10PreemptionTests, whenMidThreadPreemptionIsAvailableThenProgramsPreamble) {
    using GPGPU_CSR_BASE_ADDRESS = typename FamilyType::GPGPU_CSR_BASE_ADDRESS;
    using STATE_SIP = typename FamilyType::STATE_SIP;

    device->setPreemptionMode(PreemptionMode::MidThread);
    executionEnvironment->DisableMidThreadPreemption = 0;

    size_t minCsrSize = device->getHardwareInfo().pSysInfo->CsrSizeInMb * MemoryConstants::megaByte;
    uint64_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface((void *)minCsrAlignment, minCsrSize);

    // verify preamble programming
    size_t requiredPreambleSize = PreemptionHelper::getRequiredPreambleSize<FamilyType>(*device);
    size_t expectedPreambleSize = sizeof(GPGPU_CSR_BASE_ADDRESS) + sizeof(STATE_SIP);
    EXPECT_EQ(expectedPreambleSize, requiredPreambleSize);

    StackVec<char, 8192> preambleStorage(requiredPreambleSize);
    ASSERT_LE(requiredPreambleSize, preambleStorage.size());
    LinearStream preambleCmdStream{preambleStorage.begin(), preambleStorage.size()};
    PreemptionHelper::programPreamble<FamilyType>(preambleCmdStream, *device, &csrSurface);

    HardwareParse hwParsePreamble;
    hwParsePreamble.parseCommands<FamilyType>(preambleCmdStream);

    auto csrBaseAddressCmd = hwParsePreamble.getCommand<GPGPU_CSR_BASE_ADDRESS>();
    ASSERT_NE(nullptr, csrBaseAddressCmd);
    EXPECT_EQ(csrSurface.getGpuAddressToPatch(), csrBaseAddressCmd->getGpgpuCsrBaseAddress());

    auto stateSipCmd = hwParsePreamble.getCommand<STATE_SIP>();
    ASSERT_NE(nullptr, stateSipCmd);
    EXPECT_EQ(device->getExecutionEnvironment()->getBuiltIns()->getSipKernel(SipKernelType::Csr, *device).getSipAllocation()->getGpuAddressToPatch(), stateSipCmd->getSystemInstructionPointer());
}

GEN10TEST_F(Gen10ThreadGroupPreemptionEnqueueKernelTest, givenSecondEnqueueWithTheSamePreemptionRequestThenDontReprogram) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    WhitelistedRegisters regs = {};
    regs.csChicken1_0x2580 = true;
    pDevice->setForceWhitelistedRegs(true, &regs);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.overrideMediaVFEStateDirty(false);
    auto csrSurface = csr.getPreemptionCsrAllocation();
    EXPECT_EQ(nullptr, csrSurface);
    HardwareParse hwParser;
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwParser.parseCommands<FamilyType>(csr.commandStream);
    hwParser.findHardwareCommands<FamilyType>();
    auto offset = csr.commandStream.getUsed();

    bool foundOne = false;
    for (auto it : hwParser.lriList) {
        auto cmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_IMM *>(it);
        if (cmd->getRegisterOffset() == 0x2580u) {
            EXPECT_FALSE(foundOne);
            foundOne = true;
        }
    }
    EXPECT_TRUE(foundOne);

    hwParser.cmdList.clear();
    hwParser.lriList.clear();

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwParser.parseCommands<FamilyType>(csr.commandStream, offset);
    hwParser.findHardwareCommands<FamilyType>();

    for (auto it : hwParser.lriList) {
        auto cmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_IMM *>(it);
        EXPECT_FALSE(cmd->getRegisterOffset() == 0x2580u);
    }
}

GEN10TEST_F(Gen10PreemptionEnqueueKernelTest, givenValidKernelForPreemptionWhenEnqueueKernelCalledThenPassDevicePreemptionMode) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    WhitelistedRegisters regs = {};
    regs.csChicken1_0x2580 = true;
    pDevice->setForceWhitelistedRegs(true, &regs);
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*pDevice, mockKernel.mockKernel));

    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    EXPECT_EQ(1, mockCsr->flushCalledCount);
    EXPECT_EQ(PreemptionMode::ThreadGroup, mockCsr->passedDispatchFlags.preemptionMode);
}

GEN10TEST_F(Gen10PreemptionEnqueueKernelTest, givenValidKernelForPreemptionWhenEnqueueKernelCalledAndBlockedThenPassDevicePreemptionMode) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    WhitelistedRegisters regs = {};
    regs.csChicken1_0x2580 = true;
    pDevice->setForceWhitelistedRegs(true, &regs);
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*pDevice, mockKernel.mockKernel));

    UserEvent userEventObj;
    cl_event userEvent = &userEventObj;
    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 1, &userEvent, nullptr);
    pCmdQ->flush();
    EXPECT_EQ(0, mockCsr->flushCalledCount);

    userEventObj.setStatus(CL_COMPLETE);
    pCmdQ->flush();
    EXPECT_EQ(1, mockCsr->flushCalledCount);
    EXPECT_EQ(PreemptionMode::ThreadGroup, mockCsr->passedDispatchFlags.preemptionMode);
}

GEN10TEST_F(Gen10MidThreadPreemptionEnqueueKernelTest, givenSecondEnqueueWithTheSamePreemptionRequestThenDontReprogramMidThread) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename FamilyType::GPGPU_CSR_BASE_ADDRESS GPGPU_CSR_BASE_ADDRESS;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.overrideMediaVFEStateDirty(false);
    auto csrSurface = csr.getPreemptionCsrAllocation();
    ASSERT_NE(nullptr, csrSurface);
    HardwareParse hwParser;
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwParser.parseCommands<FamilyType>(csr.commandStream);
    hwParser.findHardwareCommands<FamilyType>();
    auto offset = csr.commandStream.getUsed();

    bool foundOneLri = false;
    for (auto it : hwParser.lriList) {
        auto cmdLri = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (cmdLri->getRegisterOffset() == 0x2580u) {
            EXPECT_FALSE(foundOneLri);
            foundOneLri = true;
        }
    }
    EXPECT_TRUE(foundOneLri);
    hwParser.findCsrBaseAddress<FamilyType>();
    ASSERT_NE(nullptr, hwParser.cmdGpgpuCsrBaseAddress);
    auto cmdCsr = genCmdCast<GPGPU_CSR_BASE_ADDRESS *>(hwParser.cmdGpgpuCsrBaseAddress);
    ASSERT_NE(nullptr, cmdCsr);
    EXPECT_EQ(csrSurface->getGpuAddressToPatch(), cmdCsr->getGpgpuCsrBaseAddress());

    hwParser.cmdList.clear();
    hwParser.lriList.clear();
    hwParser.cmdGpgpuCsrBaseAddress = nullptr;

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwParser.parseCommands<FamilyType>(csr.commandStream, offset);
    hwParser.findHardwareCommands<FamilyType>();

    for (auto it : hwParser.lriList) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        EXPECT_FALSE(cmd->getRegisterOffset() == 0x2580u);
    }

    hwParser.findCsrBaseAddress<FamilyType>();
    EXPECT_EQ(nullptr, hwParser.cmdGpgpuCsrBaseAddress);
}

GEN10TEST_F(Gen10PreemptionEnqueueKernelTest, givenDisabledPreemptionWhenEnqueueKernelCalledThenPassDisabledPreemptionMode) {
    pDevice->setPreemptionMode(PreemptionMode::Disabled);
    WhitelistedRegisters regs = {};
    pDevice->setForceWhitelistedRegs(true, &regs);
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo(), *pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    EXPECT_EQ(PreemptionMode::Disabled, PreemptionHelper::taskPreemptionMode(*pDevice, mockKernel.mockKernel));

    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    EXPECT_EQ(1, mockCsr->flushCalledCount);
    EXPECT_EQ(PreemptionMode::Disabled, mockCsr->passedDispatchFlags.preemptionMode);
}

GEN10TEST_F(Gen10PreemptionTests, getRequiredCmdQSize) {
    size_t expectedSize = 0;
    EXPECT_EQ(expectedSize, PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device));
}

GEN10TEST_F(Gen10PreemptionTests, applyPreemptionWaCmds) {
    size_t usedSize = 0;
    auto &cmdStream = cmdQ->getCS(0);

    PreemptionHelper::applyPreemptionWaCmdsBegin<FamilyType>(&cmdStream, *device);
    EXPECT_EQ(usedSize, cmdStream.getUsed());
    PreemptionHelper::applyPreemptionWaCmdsEnd<FamilyType>(&cmdStream, *device);
    EXPECT_EQ(usedSize, cmdStream.getUsed());
}

GEN10TEST_F(Gen10PreemptionTests, givenInterfaceDescriptorDataWhenMidThreadPreemptionModeThenSetDisableThreadPreemptionBitToDisable) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA iddArg;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;

    iddArg.setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidThread);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE, iddArg.getThreadPreemptionDisable());
}

GEN10TEST_F(Gen10PreemptionTests, givenInterfaceDescriptorDataWhenNoMidThreadPreemptionModeThenSetDisableThreadPreemptionBitToEnable) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA iddArg;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;

    iddArg.setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::Disabled);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE, iddArg.getThreadPreemptionDisable());

    iddArg.setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidBatch);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE, iddArg.getThreadPreemptionDisable());

    iddArg.setThreadPreemptionDisable(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_DISABLE);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::ThreadGroup);
    EXPECT_EQ(INTERFACE_DESCRIPTOR_DATA::THREAD_PREEMPTION_DISABLE_ENABLE, iddArg.getThreadPreemptionDisable());
}
