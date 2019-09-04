/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/hw_helper.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/fixtures/hello_world_fixture.h"
#include "unit_tests/fixtures/preemption_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"

using namespace NEO;

using Gen8PreemptionTests = DevicePreemptionTests;
using Gen8PreemptionEnqueueKernelTest = PreemptionEnqueueKernelTest;

template <>
PreemptionTestHwDetails GetPreemptionTestHwDetails<BDWFamily>() {
    PreemptionTestHwDetails ret;
    ret.modeToRegValueMap[PreemptionMode::ThreadGroup] = 0;
    ret.modeToRegValueMap[PreemptionMode::MidBatch] = (1 << 2);
    ret.defaultRegValue = ret.modeToRegValueMap[PreemptionMode::MidBatch];
    ret.regAddress = 0x2248u;
    return ret;
}

GEN8TEST_F(Gen8PreemptionTests, allowThreadGroupPreemptionReturnsTrue) {
    EXPECT_TRUE(PreemptionHelper::allowThreadGroupPreemption(kernel.get(), waTable));
}

GEN8TEST_F(Gen8PreemptionTests, whenProgramStateSipIsCalledThenNoCmdsAreProgrammed) {
    size_t requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*device);
    EXPECT_EQ(0U, requiredSize);

    LinearStream cmdStream{nullptr, 0};
    PreemptionHelper::programStateSip<FamilyType>(cmdStream, *device);
    EXPECT_EQ(0U, cmdStream.getUsed());
}

GEN8TEST_F(Gen8PreemptionEnqueueKernelTest, givenSecondEnqueueWithTheSamePreemptionRequestThenDontReprogram) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.setMediaVFEStateDirty(false);
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pDevice);

    HardwareParse hwParser;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwParser.parseCommands<FamilyType>(csr.commandStream);
    auto offset = csr.commandStream.getUsed();
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();
    hwParser.parseCommands<FamilyType>(csr.commandStream, offset);

    size_t numMmiosFound = countMmio<FamilyType>(hwParser.cmdList.begin(), hwParser.cmdList.end(), 0x2248u);
    EXPECT_EQ(1U, numMmiosFound);
}

GEN8TEST_F(Gen8PreemptionEnqueueKernelTest, givenValidKernelForPreemptionWhenEnqueueKernelCalledThenPassDevicePreemptionMode) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment);
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
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment);
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
    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment);
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    EXPECT_EQ(PreemptionMode::Disabled, PreemptionHelper::taskPreemptionMode(*pDevice, mockKernel.mockKernel));

    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    EXPECT_EQ(1, mockCsr->flushCalledCount);
    EXPECT_EQ(PreemptionMode::Disabled, mockCsr->passedDispatchFlags.preemptionMode);
}

GEN8TEST_F(Gen8PreemptionTests, getPreemptionWaCsSizeMidBatch) {
    size_t expectedSize = 0;
    device->setPreemptionMode(PreemptionMode::MidBatch);
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN8TEST_F(Gen8PreemptionTests, getPreemptionWaCsSizeThreadGroupNoWa) {
    size_t expectedSize = 0;
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    device->getExecutionEnvironment()->getMutableHardwareInfo()->workaroundTable.waModifyVFEStateAfterGPGPUPreemption = false;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN8TEST_F(Gen8PreemptionTests, getPreemptionWaCsSizeThreadGroupWa) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t expectedSize = 2 * sizeof(MI_LOAD_REGISTER_IMM);
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    device->getExecutionEnvironment()->getMutableHardwareInfo()->workaroundTable.waModifyVFEStateAfterGPGPUPreemption = true;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN8TEST_F(Gen8PreemptionTests, getPreemptionWaCsSizeMidThreadNoWa) {
    size_t expectedSize = 0;
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->getExecutionEnvironment()->getMutableHardwareInfo()->workaroundTable.waModifyVFEStateAfterGPGPUPreemption = false;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN8TEST_F(Gen8PreemptionTests, getPreemptionWaCsSizeMidThreadWa) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t expectedSize = 2 * sizeof(MI_LOAD_REGISTER_IMM);
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->getExecutionEnvironment()->getMutableHardwareInfo()->workaroundTable.waModifyVFEStateAfterGPGPUPreemption = true;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN8TEST_F(Gen8PreemptionTests, givenInterfaceDescriptorDataWhenAnyPreemptionModeThenNoChange) {
    using INTERFACE_DESCRIPTOR_DATA = typename FamilyType::INTERFACE_DESCRIPTOR_DATA;

    INTERFACE_DESCRIPTOR_DATA idd;
    INTERFACE_DESCRIPTOR_DATA iddArg;
    int ret;

    idd = FamilyType::cmdInitInterfaceDescriptorData;
    iddArg = FamilyType::cmdInitInterfaceDescriptorData;

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::Disabled);
    ret = memcmp(&idd, &iddArg, sizeof(INTERFACE_DESCRIPTOR_DATA));
    EXPECT_EQ(0, ret);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidBatch);
    ret = memcmp(&idd, &iddArg, sizeof(INTERFACE_DESCRIPTOR_DATA));
    EXPECT_EQ(0, ret);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::ThreadGroup);
    ret = memcmp(&idd, &iddArg, sizeof(INTERFACE_DESCRIPTOR_DATA));
    EXPECT_EQ(0, ret);

    PreemptionHelper::programInterfaceDescriptorDataPreemption<FamilyType>(&iddArg, PreemptionMode::MidThread);
    ret = memcmp(&idd, &iddArg, sizeof(INTERFACE_DESCRIPTOR_DATA));
    EXPECT_EQ(0, ret);
}
