/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/command_stream/preemption.h"
#include "core/helpers/hw_helper.h"
#include "core/unit_tests/fixtures/preemption_fixture.h"
#include "runtime/built_ins/built_ins.h"
#include "unit_tests/command_queue/enqueue_fixture.h"
#include "unit_tests/helpers/hw_parse.h"
#include "unit_tests/mocks/mock_buffer.h"
#include "unit_tests/mocks/mock_command_queue.h"
#include "unit_tests/mocks/mock_csr.h"
#include "unit_tests/mocks/mock_submissions_aggregator.h"

namespace NEO {

template <>
void HardwareParse::findCsrBaseAddress<SKLFamily>() {
    typedef typename GEN9::GPGPU_CSR_BASE_ADDRESS GPGPU_CSR_BASE_ADDRESS;
    itorGpgpuCsrBaseAddress = find<GPGPU_CSR_BASE_ADDRESS *>(cmdList.begin(), itorWalker);
    if (itorGpgpuCsrBaseAddress != itorWalker) {
        cmdGpgpuCsrBaseAddress = *itorGpgpuCsrBaseAddress;
    }
}
} // namespace NEO

using namespace NEO;

using Gen9PreemptionTests = DevicePreemptionTests;
using Gen9PreemptionEnqueueKernelTest = PreemptionEnqueueKernelTest;
using Gen9MidThreadPreemptionEnqueueKernelTest = MidThreadPreemptionEnqueueKernelTest;
using Gen9ThreadGroupPreemptionEnqueueKernelTest = ThreadGroupPreemptionEnqueueKernelTest;

template <>
PreemptionTestHwDetails GetPreemptionTestHwDetails<SKLFamily>() {
    PreemptionTestHwDetails ret;
    ret.modeToRegValueMap[PreemptionMode::ThreadGroup] = DwordBuilder::build(1, true) | DwordBuilder::build(2, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidBatch] = DwordBuilder::build(2, true) | DwordBuilder::build(1, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidThread] = DwordBuilder::build(2, true, false) | DwordBuilder::build(1, true, false);
    ret.defaultRegValue = ret.modeToRegValueMap[PreemptionMode::MidBatch];
    ret.regAddress = 0x2580u;
    return ret;
}

GEN9TEST_F(Gen9PreemptionTests, whenMidThreadPreemptionIsNotAvailableThenDoesNotProgramPreamble) {
    device->setPreemptionMode(PreemptionMode::ThreadGroup);

    size_t requiredSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*device);
    EXPECT_EQ(0U, requiredSize);

    LinearStream cmdStream{nullptr, 0};
    PreemptionHelper::programStateSip<FamilyType>(cmdStream, *device);
    EXPECT_EQ(0U, cmdStream.getUsed());
}

GEN9TEST_F(Gen9PreemptionTests, whenMidThreadPreemptionIsAvailableThenStateSipIsProgrammed) {
    using STATE_SIP = typename FamilyType::STATE_SIP;

    device->setPreemptionMode(PreemptionMode::MidThread);
    executionEnvironment->DisableMidThreadPreemption = 0;

    size_t minCsrSize = device->getHardwareInfo().gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte;
    uint64_t minCsrAlignment = 2 * 256 * MemoryConstants::kiloByte;
    MockGraphicsAllocation csrSurface((void *)minCsrAlignment, minCsrSize);

    size_t requiredCmdStreamSize = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*device);
    size_t expectedPreambleSize = sizeof(STATE_SIP);
    EXPECT_EQ(expectedPreambleSize, requiredCmdStreamSize);

    StackVec<char, 8192> streamStorage(requiredCmdStreamSize);
    ASSERT_LE(requiredCmdStreamSize, streamStorage.size());

    LinearStream cmdStream{streamStorage.begin(), streamStorage.size()};
    PreemptionHelper::programStateSip<FamilyType>(cmdStream, *device);

    HardwareParse hwParsePreamble;
    hwParsePreamble.parseCommands<FamilyType>(cmdStream);

    auto stateSipCmd = hwParsePreamble.getCommand<STATE_SIP>();
    ASSERT_NE(nullptr, stateSipCmd);
    EXPECT_EQ(device->getExecutionEnvironment()->getBuiltIns()->getSipKernel(SipKernelType::Csr, *device).getSipAllocation()->getGpuAddressToPatch(), stateSipCmd->getSystemInstructionPointer());
}

GEN9TEST_F(Gen9ThreadGroupPreemptionEnqueueKernelTest, givenSecondEnqueueWithTheSamePreemptionRequestThenDontReprogramThreadGroupNoWa) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);

    pDevice->getExecutionEnvironment()->getMutableHardwareInfo()->workaroundTable.waModifyVFEStateAfterGPGPUPreemption = false;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.setMediaVFEStateDirty(false);
    auto csrSurface = csr.getPreemptionAllocation();
    EXPECT_EQ(nullptr, csrSurface);
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pDevice);

    HardwareParse hwParserCsr;
    HardwareParse hwParserCmdQ;
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwParserCsr.parseCommands<FamilyType>(csr.commandStream);
    hwParserCmdQ.parseCommands<FamilyType>(pCmdQ->getCS(1024));
    auto offsetCsr = csr.commandStream.getUsed();
    auto offsetCmdQ = pCmdQ->getCS(1024).getUsed();
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();
    hwParserCsr.parseCommands<FamilyType>(csr.commandStream, offsetCsr);
    hwParserCmdQ.parseCommands<FamilyType>(pCmdQ->getCS(1024), offsetCmdQ);

    EXPECT_EQ(1U, countMmio<FamilyType>(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end(), 0x2580u));
    EXPECT_EQ(0U, countMmio<FamilyType>(hwParserCsr.cmdList.begin(), hwParserCsr.cmdList.end(), 0x2600u));
    EXPECT_EQ(0U, countMmio<FamilyType>(hwParserCmdQ.cmdList.begin(), hwParserCmdQ.cmdList.end(), 0x2580u));
    EXPECT_EQ(0U, countMmio<FamilyType>(hwParserCmdQ.cmdList.begin(), hwParserCmdQ.cmdList.end(), 0x2600u));
}

GEN9TEST_F(Gen9ThreadGroupPreemptionEnqueueKernelTest, givenSecondEnqueueWithTheSamePreemptionRequestThenDontReprogramThreadGroupWa) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);

    pDevice->getExecutionEnvironment()->getMutableHardwareInfo()->workaroundTable.waModifyVFEStateAfterGPGPUPreemption = true;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.setMediaVFEStateDirty(false);
    auto csrSurface = csr.getPreemptionAllocation();
    EXPECT_EQ(nullptr, csrSurface);
    HardwareParse hwCsrParser;
    HardwareParse hwCmdQParser;
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwCsrParser.parseCommands<FamilyType>(csr.commandStream);
    hwCsrParser.findHardwareCommands<FamilyType>();
    hwCmdQParser.parseCommands<FamilyType>(pCmdQ->getCS(1024));
    hwCmdQParser.findHardwareCommands<FamilyType>();
    auto offsetCsr = csr.commandStream.getUsed();
    auto offsetCmdQ = pCmdQ->getCS(1024).getUsed();

    bool foundOne = false;
    for (auto it : hwCsrParser.lriList) {
        auto cmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_IMM *>(it);
        if (cmd->getRegisterOffset() == 0x2580u) {
            EXPECT_FALSE(foundOne);
            foundOne = true;
        }
    }
    EXPECT_TRUE(foundOne);
    hwCsrParser.cmdList.clear();
    hwCsrParser.lriList.clear();

    int foundWaLri = 0;
    int foundWaLriBegin = 0;
    int foundWaLriEnd = 0;
    for (auto it : hwCmdQParser.lriList) {
        auto cmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_IMM *>(it);
        if (cmd->getRegisterOffset() == 0x2600u) {
            foundWaLri++;
            if (cmd->getDataDword() == 0xFFFFFFFF) {
                foundWaLriBegin++;
            }
            if (cmd->getDataDword() == 0x0) {
                foundWaLriEnd++;
            }
        }
    }
    EXPECT_EQ(2, foundWaLri);
    EXPECT_EQ(1, foundWaLriBegin);
    EXPECT_EQ(1, foundWaLriEnd);
    hwCmdQParser.cmdList.clear();
    hwCmdQParser.lriList.clear();

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwCsrParser.parseCommands<FamilyType>(csr.commandStream, offsetCsr);
    hwCsrParser.findHardwareCommands<FamilyType>();

    hwCmdQParser.parseCommands<FamilyType>(pCmdQ->getCS(1024), offsetCmdQ);
    hwCmdQParser.findHardwareCommands<FamilyType>();

    for (auto it : hwCsrParser.lriList) {
        auto cmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_IMM *>(it);
        EXPECT_FALSE(cmd->getRegisterOffset() == 0x2580u);
    }

    foundWaLri = 0;
    foundWaLriBegin = 0;
    foundWaLriEnd = 0;
    for (auto it : hwCmdQParser.lriList) {
        auto cmd = genCmdCast<typename FamilyType::MI_LOAD_REGISTER_IMM *>(it);
        if (cmd->getRegisterOffset() == 0x2600u) {
            foundWaLri++;
            if (cmd->getDataDword() == 0xFFFFFFFF) {
                foundWaLriBegin++;
            }
            if (cmd->getDataDword() == 0x0) {
                foundWaLriEnd++;
            }
        }
    }
    EXPECT_EQ(2, foundWaLri);
    EXPECT_EQ(1, foundWaLriBegin);
    EXPECT_EQ(1, foundWaLriEnd);
}

GEN9TEST_F(Gen9PreemptionEnqueueKernelTest, givenValidKernelForPreemptionWhenEnqueueKernelCalledThenPassDevicePreemptionModeThreadGroup) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex());
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    MultiDispatchInfo multiDispatch(mockKernel.mockKernel);
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*pDevice, multiDispatch));

    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    EXPECT_EQ(1, mockCsr->flushCalledCount);
    EXPECT_EQ(PreemptionMode::ThreadGroup, mockCsr->passedDispatchFlags.preemptionMode);
}

GEN9TEST_F(Gen9PreemptionEnqueueKernelTest, givenValidKernelForPreemptionWhenEnqueueKernelCalledAndBlockedThenPassDevicePreemptionModeThreadGroup) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex());
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    MultiDispatchInfo multiDispatch(mockKernel.mockKernel);
    EXPECT_EQ(PreemptionMode::ThreadGroup, PreemptionHelper::taskPreemptionMode(*pDevice, multiDispatch));

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

GEN9TEST_F(Gen9MidThreadPreemptionEnqueueKernelTest, givenSecondEnqueueWithTheSamePreemptionRequestThenDontReprogramMidThreadNoWa) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename FamilyType::GPGPU_CSR_BASE_ADDRESS GPGPU_CSR_BASE_ADDRESS;

    pDevice->getExecutionEnvironment()->getMutableHardwareInfo()->workaroundTable.waModifyVFEStateAfterGPGPUPreemption = false;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.setMediaVFEStateDirty(false);
    auto csrSurface = csr.getPreemptionAllocation();
    ASSERT_NE(nullptr, csrSurface);
    HardwareParse hwCsrParser;
    HardwareParse hwCmdQParser;
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwCsrParser.parseCommands<FamilyType>(csr.commandStream);
    hwCsrParser.findHardwareCommands<FamilyType>();
    hwCmdQParser.parseCommands<FamilyType>(pCmdQ->getCS(1024));
    hwCmdQParser.findHardwareCommands<FamilyType>();
    auto offsetCsr = csr.commandStream.getUsed();
    auto offsetCmdQ = pCmdQ->getCS(1024).getUsed();

    bool foundOneLri = false;
    for (auto it : hwCsrParser.lriList) {
        auto cmdLri = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (cmdLri->getRegisterOffset() == 0x2580u) {
            EXPECT_FALSE(foundOneLri);
            foundOneLri = true;
        }
    }
    EXPECT_TRUE(foundOneLri);

    bool foundWaLri = false;
    for (auto it : hwCmdQParser.lriList) {
        auto cmdLri = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (cmdLri->getRegisterOffset() == 0x2600u) {
            foundWaLri = true;
        }
    }
    EXPECT_FALSE(foundWaLri);

    hwCsrParser.findCsrBaseAddress<FamilyType>();
    ASSERT_NE(nullptr, hwCsrParser.cmdGpgpuCsrBaseAddress);
    auto cmdCsr = genCmdCast<GPGPU_CSR_BASE_ADDRESS *>(hwCsrParser.cmdGpgpuCsrBaseAddress);
    ASSERT_NE(nullptr, cmdCsr);
    EXPECT_EQ(csrSurface->getGpuAddressToPatch(), cmdCsr->getGpgpuCsrBaseAddress());

    hwCsrParser.cmdList.clear();
    hwCsrParser.lriList.clear();
    hwCsrParser.cmdGpgpuCsrBaseAddress = nullptr;
    hwCmdQParser.cmdList.clear();
    hwCmdQParser.lriList.clear();

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwCsrParser.parseCommands<FamilyType>(csr.commandStream, offsetCsr);
    hwCsrParser.findHardwareCommands<FamilyType>();
    hwCmdQParser.parseCommands<FamilyType>(csr.commandStream, offsetCmdQ);
    hwCmdQParser.findHardwareCommands<FamilyType>();

    for (auto it : hwCsrParser.lriList) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        EXPECT_FALSE(cmd->getRegisterOffset() == 0x2580u);
    }

    hwCsrParser.findCsrBaseAddress<FamilyType>();
    EXPECT_EQ(nullptr, hwCsrParser.cmdGpgpuCsrBaseAddress);

    for (auto it : hwCmdQParser.lriList) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        EXPECT_FALSE(cmd->getRegisterOffset() == 0x2600u);
    }
}

GEN9TEST_F(Gen9MidThreadPreemptionEnqueueKernelTest, givenSecondEnqueueWithTheSamePreemptionRequestThenDontReprogramMidThreadWa) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    typedef typename FamilyType::GPGPU_CSR_BASE_ADDRESS GPGPU_CSR_BASE_ADDRESS;

    pDevice->getExecutionEnvironment()->getMutableHardwareInfo()->workaroundTable.waModifyVFEStateAfterGPGPUPreemption = true;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.setMediaVFEStateDirty(false);
    auto csrSurface = csr.getPreemptionAllocation();
    ASSERT_NE(nullptr, csrSurface);
    HardwareParse hwCsrParser;
    HardwareParse hwCmdQParser;
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pDevice);

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwCsrParser.parseCommands<FamilyType>(csr.commandStream);
    hwCsrParser.findHardwareCommands<FamilyType>();
    hwCmdQParser.parseCommands<FamilyType>(pCmdQ->getCS(1024));
    hwCmdQParser.findHardwareCommands<FamilyType>();
    auto offsetCsr = csr.commandStream.getUsed();
    auto offsetCmdQ = pCmdQ->getCS(1024).getUsed();

    bool foundOneLri = false;
    for (auto it : hwCsrParser.lriList) {
        auto cmdLri = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (cmdLri->getRegisterOffset() == 0x2580u) {
            EXPECT_FALSE(foundOneLri);
            foundOneLri = true;
        }
    }
    EXPECT_TRUE(foundOneLri);

    int foundWaLri = 0;
    int foundWaLriBegin = 0;
    int foundWaLriEnd = 0;
    for (auto it : hwCmdQParser.lriList) {
        auto cmdLri = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (cmdLri->getRegisterOffset() == 0x2600u) {
            foundWaLri++;
            if (cmdLri->getDataDword() == 0xFFFFFFFF) {
                foundWaLriBegin++;
            }
            if (cmdLri->getDataDword() == 0x0) {
                foundWaLriEnd++;
            }
        }
    }
    EXPECT_EQ(2, foundWaLri);
    EXPECT_EQ(1, foundWaLriBegin);
    EXPECT_EQ(1, foundWaLriEnd);

    hwCsrParser.findCsrBaseAddress<FamilyType>();
    ASSERT_NE(nullptr, hwCsrParser.cmdGpgpuCsrBaseAddress);
    auto cmdCsr = genCmdCast<GPGPU_CSR_BASE_ADDRESS *>(hwCsrParser.cmdGpgpuCsrBaseAddress);
    ASSERT_NE(nullptr, cmdCsr);
    EXPECT_EQ(csrSurface->getGpuAddressToPatch(), cmdCsr->getGpgpuCsrBaseAddress());

    hwCsrParser.cmdList.clear();
    hwCsrParser.lriList.clear();
    hwCsrParser.cmdGpgpuCsrBaseAddress = nullptr;
    hwCmdQParser.cmdList.clear();
    hwCmdQParser.lriList.clear();

    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, off, gws, nullptr, 0, nullptr, nullptr);
    hwCsrParser.parseCommands<FamilyType>(csr.commandStream, offsetCsr);
    hwCsrParser.findHardwareCommands<FamilyType>();
    hwCmdQParser.parseCommands<FamilyType>(pCmdQ->getCS(1024), offsetCmdQ);
    hwCmdQParser.findHardwareCommands<FamilyType>();

    for (auto it : hwCsrParser.lriList) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        EXPECT_FALSE(cmd->getRegisterOffset() == 0x2580u);
    }

    hwCsrParser.findCsrBaseAddress<FamilyType>();
    EXPECT_EQ(nullptr, hwCsrParser.cmdGpgpuCsrBaseAddress);

    foundWaLri = 0;
    foundWaLriBegin = 0;
    foundWaLriEnd = 0;
    for (auto it : hwCmdQParser.lriList) {
        auto cmd = genCmdCast<MI_LOAD_REGISTER_IMM *>(it);
        if (cmd->getRegisterOffset() == 0x2600u) {
            foundWaLri++;
            if (cmd->getDataDword() == 0xFFFFFFFF) {
                foundWaLriBegin++;
            }
            if (cmd->getDataDword() == 0x0) {
                foundWaLriEnd++;
            }
        }
    }
    EXPECT_EQ(2, foundWaLri);
    EXPECT_EQ(1, foundWaLriBegin);
    EXPECT_EQ(1, foundWaLriEnd);
}

GEN9TEST_F(Gen9PreemptionEnqueueKernelTest, givenDisabledPreemptionWhenEnqueueKernelCalledThenPassDisabledPreemptionMode) {
    pDevice->setPreemptionMode(PreemptionMode::Disabled);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex());
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pDevice);
    MultiDispatchInfo multiDispatch(mockKernel.mockKernel);
    EXPECT_EQ(PreemptionMode::Disabled, PreemptionHelper::taskPreemptionMode(*pDevice, multiDispatch));

    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    EXPECT_EQ(1, mockCsr->flushCalledCount);
    EXPECT_EQ(PreemptionMode::Disabled, mockCsr->passedDispatchFlags.preemptionMode);
}

GEN9TEST_F(Gen9PreemptionTests, getPreemptionWaCsSizeMidBatch) {
    size_t expectedSize = 0;
    device->setPreemptionMode(PreemptionMode::MidBatch);
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN9TEST_F(Gen9PreemptionTests, getPreemptionWaCsSizeThreadGroupNoWa) {
    size_t expectedSize = 0;
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    device->getExecutionEnvironment()->getMutableHardwareInfo()->workaroundTable.waModifyVFEStateAfterGPGPUPreemption = false;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN9TEST_F(Gen9PreemptionTests, getPreemptionWaCsSizeThreadGroupWa) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t expectedSize = 2 * sizeof(MI_LOAD_REGISTER_IMM);
    device->setPreemptionMode(PreemptionMode::ThreadGroup);
    device->getExecutionEnvironment()->getMutableHardwareInfo()->workaroundTable.waModifyVFEStateAfterGPGPUPreemption = true;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN9TEST_F(Gen9PreemptionTests, getPreemptionWaCsSizeMidThreadNoWa) {
    size_t expectedSize = 0;
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->getExecutionEnvironment()->getMutableHardwareInfo()->workaroundTable.waModifyVFEStateAfterGPGPUPreemption = false;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN9TEST_F(Gen9PreemptionTests, getPreemptionWaCsSizeMidThreadWa) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    size_t expectedSize = 2 * sizeof(MI_LOAD_REGISTER_IMM);
    device->setPreemptionMode(PreemptionMode::MidThread);
    device->getExecutionEnvironment()->getMutableHardwareInfo()->workaroundTable.waModifyVFEStateAfterGPGPUPreemption = true;
    size_t size = PreemptionHelper::getPreemptionWaCsSize<FamilyType>(*device);
    EXPECT_EQ(expectedSize, size);
}

GEN9TEST_F(Gen9PreemptionTests, givenInterfaceDescriptorDataWhenAnyPreemptionModeThenNoChange) {
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

GEN9TEST_F(Gen9PreemptionTests, givenMidThreadPreemptionModeWhenStateSipIsProgrammedThenSipEqualsSipAllocationGpuAddressToPatch) {
    using STATE_SIP = typename FamilyType::STATE_SIP;
    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<MockDevice>(nullptr));

    mockDevice->setPreemptionMode(PreemptionMode::MidThread);
    auto cmdSizePreemptionMidThread = PreemptionHelper::getRequiredStateSipCmdSize<FamilyType>(*mockDevice);

    StackVec<char, 4096> preemptionBuffer;
    preemptionBuffer.resize(cmdSizePreemptionMidThread);
    LinearStream preemptionStream(&*preemptionBuffer.begin(), preemptionBuffer.size());

    PreemptionHelper::programStateSip<FamilyType>(preemptionStream, *mockDevice);

    HardwareParse hwParserOnlyPreemption;
    hwParserOnlyPreemption.parseCommands<FamilyType>(preemptionStream, 0);
    auto cmd = hwParserOnlyPreemption.getCommand<STATE_SIP>();
    EXPECT_NE(nullptr, cmd);

    auto sipType = SipKernel::getSipKernelType(mockDevice->getHardwareInfo().platform.eRenderCoreFamily, mockDevice->isSourceLevelDebuggerActive());
    EXPECT_EQ(mockDevice->getExecutionEnvironment()->getBuiltIns()->getSipKernel(sipType, *mockDevice).getSipAllocation()->getGpuAddressToPatch(), cmd->getSystemInstructionPointer());
}
