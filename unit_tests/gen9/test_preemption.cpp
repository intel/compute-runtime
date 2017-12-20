/*
 * Copyright (c) 2017, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"
#include "unit_tests/preemption/preemption_tests.h"

namespace OCLRT {

template <>
void HardwareParse::findCsrBaseAddress<SKLFamily>() {
    typedef typename GEN9::GPGPU_CSR_BASE_ADDRESS GPGPU_CSR_BASE_ADDRESS;
    itorGpgpuCsrBaseAddress = find<GPGPU_CSR_BASE_ADDRESS *>(cmdList.begin(), itorWalker);
    if (itorGpgpuCsrBaseAddress != itorWalker) {
        cmdGpgpuCsrBaseAddress = *itorGpgpuCsrBaseAddress;
    }
}
} // namespace OCLRT

using namespace OCLRT;

typedef DevicePreemptionTests Gen9PreemptionTests;
typedef PreemptionEnqueueKernelTest Gen9PreemptionEnqueueKernelTest;
typedef MidThreadPreemptionEnqueueKernelTest Gen9MidThreadPreemptionEnqueueKernelTest;
typedef ThreadGroupPreemptionEnqueueKernelTest Gen9ThreadGroupPreemptionEnqueueKernelTest;

GEN9TEST_F(Gen9PreemptionTests, programThreadGroupPreemptionLri) {
    preemptionMode = PreemptionMode::ThreadGroup;
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t requiredSize = PreemptionHelper::getRequiredCsrSize<FamilyType>(preemptionMode);
    size_t expectedSize = sizeof(MI_LOAD_REGISTER_IMM);
    EXPECT_EQ(expectedSize, requiredSize);

    auto &cmdStream = cmdQ->getCS(requiredSize);

    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(kernel, waTable));
    PreemptionHelper::programPreemptionMode<FamilyType>(&cmdStream, preemptionMode, nullptr, nullptr);
    EXPECT_EQ(requiredSize, cmdStream.getUsed());

    auto lri = (MI_LOAD_REGISTER_IMM *)cmdStream.getBase();
    EXPECT_EQ(0x2580u, lri->getRegisterOffset());
    uint32_t expectedData = DwordBuilder::build(1, true) | DwordBuilder::build(2, true, false);
    EXPECT_EQ(expectedData, lri->getDataDword());
}

GEN9TEST_F(Gen9PreemptionTests, programMidBatchPreemptionLri) {
    preemptionMode = PreemptionMode::MidBatch;
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t requiredSize = PreemptionHelper::getRequiredCsrSize<FamilyType>(preemptionMode);
    size_t expectedSize = sizeof(MI_LOAD_REGISTER_IMM);
    EXPECT_EQ(expectedSize, requiredSize);
    auto &cmdStream = cmdQ->getCS(requiredSize);
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(kernel, waTable));

    PreemptionHelper::programPreemptionMode<FamilyType>(&cmdStream, preemptionMode, nullptr, nullptr);
    EXPECT_EQ(requiredSize, cmdStream.getUsed());

    auto lri = (MI_LOAD_REGISTER_IMM *)cmdStream.getBase();
    EXPECT_EQ(0x2580u, lri->getRegisterOffset());
    uint32_t expectedData = DwordBuilder::build(2, true) | DwordBuilder::build(1, true, false);
    EXPECT_EQ(expectedData, lri->getDataDword());
}

GEN9TEST_F(Gen9PreemptionTests, programMidThreadPreemptionLri) {
    preemptionMode = PreemptionMode::MidThread;
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename FamilyType::GPGPU_CSR_BASE_ADDRESS GPGPU_CSR_BASE_ADDRESS;
    size_t requiredSize = PreemptionHelper::getRequiredCsrSize<FamilyType>(preemptionMode);
    size_t expectedSize = sizeof(MI_LOAD_REGISTER_IMM) + sizeof(GPGPU_CSR_BASE_ADDRESS);
    EXPECT_EQ(expectedSize, requiredSize);
    auto &cmdStream = cmdQ->getCS(requiredSize);
    size_t minSize = device->getHardwareInfo().pSysInfo->CsrSizeInMb * MemoryConstants::megaByte;
    uint64_t minAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface((void *)minAlignment, minSize);
    executionEnvironment.DisableMidThreadPreemption = 0;

    device->setPreemptionMode(preemptionMode);
    EXPECT_TRUE(PreemptionHelper::allowMidThreadPreemption(kernel, *device));

    PreemptionHelper::programPreemptionMode<FamilyType>(&cmdStream, preemptionMode, &csrSurface, nullptr);
    EXPECT_EQ(requiredSize, cmdStream.getUsed());

    auto lri = (MI_LOAD_REGISTER_IMM *)cmdStream.getBase();
    EXPECT_EQ(0x2580u, lri->getRegisterOffset());
    uint32_t expectedData = DwordBuilder::build(2, true, false) | DwordBuilder::build(1, true, false);
    EXPECT_EQ(expectedData, lri->getDataDword());

    auto gpgpuCsr = (GPGPU_CSR_BASE_ADDRESS *)((uintptr_t)lri + sizeof(MI_LOAD_REGISTER_IMM));
    EXPECT_EQ(minAlignment, gpgpuCsr->getGpgpuCsrBaseAddress());
}

GEN9TEST_F(Gen9ThreadGroupPreemptionEnqueueKernelTest, givenSecondEnqueueWithTheSamePreemptionRequestThenDontReprogramThreadGroup) {
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

GEN9TEST_F(Gen9PreemptionEnqueueKernelTest, givenValidKernelForPreemptionWhenEnqueueKernelCalledThenPassDevicePreemptionModeThreadGroup) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    WhitelistedRegisters regs = {};
    regs.csChicken1_0x2580 = true;
    pDevice->setForceWhitelistedRegs(true, &regs);
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo());
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*pDevice, mockKernel.mockKernel));

    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    EXPECT_EQ(1, mockCsr->flushCalledCount);
    EXPECT_EQ(PreemptionMode::ThreadGroup, mockCsr->passedDispatchFlags.preemptionMode);
}

GEN9TEST_F(Gen9PreemptionEnqueueKernelTest, givenValidKernelForPreemptionWhenEnqueueKernelCalledAndBlockedThenPassDevicePreemptionModeThreadGroup) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    WhitelistedRegisters regs = {};
    regs.csChicken1_0x2580 = true;
    pDevice->setForceWhitelistedRegs(true, &regs);
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo());
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

GEN9TEST_F(Gen9MidThreadPreemptionEnqueueKernelTest, givenSecondEnqueueWithTheSamePreemptionRequestThenDontReprogramMidThread) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename FamilyType::GPGPU_CSR_BASE_ADDRESS GPGPU_CSR_BASE_ADDRESS;

    WhitelistedRegisters regs = {};
    regs.csChicken1_0x2580 = true;
    pDevice->setForceWhitelistedRegs(true, &regs);

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

GEN9TEST_F(Gen9PreemptionEnqueueKernelTest, givenDisabledPreemptionWhenEnqueueKernelCalledThenPassDisabledPreemptionMode) {
    pDevice->setPreemptionMode(PreemptionMode::Disabled);
    WhitelistedRegisters regs = {};
    pDevice->setForceWhitelistedRegs(true, &regs);
    auto mockCsr = new MockCsrHw2<FamilyType>(pDevice->getHardwareInfo());
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    EXPECT_EQ(PreemptionMode::Disabled, PreemptionHelper::taskPreemptionMode(*pDevice, mockKernel.mockKernel));

    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    EXPECT_EQ(1, mockCsr->flushCalledCount);
    EXPECT_EQ(PreemptionMode::Disabled, mockCsr->passedDispatchFlags.preemptionMode);
}
