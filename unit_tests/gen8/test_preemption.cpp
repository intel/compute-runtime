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
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"
#include "unit_tests/preemption/preemption_tests.h"

using namespace OCLRT;

typedef DevicePreemptionTests Gen8PreemptionTests;
typedef PreemptionEnqueueKernelTest Gen8PreemptionEnqueueKernelTest;

GEN8TEST_F(Gen8PreemptionTests, programThreadGroupPreemptionLri) {
    preemptionMode = PreemptionMode::ThreadGroup;
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t requiredSize = PreemptionHelper::getRequiredCsrSize<FamilyType>(preemptionMode);
    size_t expectedSize = sizeof(MI_LOAD_REGISTER_IMM);
    EXPECT_EQ(expectedSize, requiredSize);
    auto &cmdStream = cmdQ->getCS(requiredSize);

    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(kernel, waTable));

    PreemptionHelper::programPreemptionMode<FamilyType>(&cmdStream, preemptionMode, nullptr, nullptr);
    EXPECT_EQ(sizeof(MI_LOAD_REGISTER_IMM), cmdStream.getUsed());

    auto lri = (MI_LOAD_REGISTER_IMM *)cmdStream.getBase();
    EXPECT_EQ(0x2248u, lri->getRegisterOffset());
    uint32_t expectedData = 0;
    EXPECT_EQ(expectedData, lri->getDataDword());
}

GEN8TEST_F(Gen8PreemptionTests, programMidBatchPreemptionLri) {
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
    EXPECT_EQ(0x2248u, lri->getRegisterOffset());
    uint32_t expectedData = DwordBuilder::build(2, false);
    EXPECT_EQ(expectedData, lri->getDataDword());
}

GEN8TEST_F(Gen8PreemptionTests, programPreemptionLri) {
    preemptionMode = PreemptionMode::ThreadGroup;
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t requiredSize = PreemptionHelper::getRequiredCsrSize<FamilyType>(preemptionMode);
    size_t expectedSize = sizeof(MI_LOAD_REGISTER_IMM);
    EXPECT_EQ(expectedSize, requiredSize);
    auto &cmdStream = cmdQ->getCS(requiredSize);

    device->setPreemptionMode(preemptionMode);
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(kernel, waTable));

    PreemptionHelper::programPreemptionMode<FamilyType>(&cmdStream, preemptionMode, nullptr, nullptr);
    EXPECT_EQ(requiredSize, cmdStream.getUsed());

    auto lri = (MI_LOAD_REGISTER_IMM *)cmdStream.getBase();
    EXPECT_EQ(0x2248u, lri->getRegisterOffset());
    uint32_t expectedData = 0;
    EXPECT_EQ(expectedData, lri->getDataDword());
}

GEN8TEST_F(Gen8PreemptionEnqueueKernelTest, givenSecondEnqueueWithTheSamePreemptionRequestThenDontReprogram) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.overrideMediaVFEStateDirty(false);
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
        if (cmd->getRegisterOffset() == 0x2248u) {
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
        EXPECT_FALSE(cmd->getRegisterOffset() == 0x2248u);
    }
}

GEN8TEST_F(Gen8PreemptionEnqueueKernelTest, givenValidKernelForPreemptionWhenEnqueueKernelCalledThenPassDevicePreemptionMode) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
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

GEN8TEST_F(Gen8PreemptionEnqueueKernelTest, givenValidKernelForPreemptionWhenEnqueueKernelCalledAndBlockedThenPassDevicePreemptionMode) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
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

GEN8TEST_F(Gen8PreemptionEnqueueKernelTest, givenDisabledPreemptionWhenEnqueueKernelCalledThenPassDisabledPreemptionMode) {
    pDevice->setPreemptionMode(PreemptionMode::Disabled);
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
