/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/mocks/mock_command_stream_receiver.h"

#include "opencl/source/helpers/cl_preemption_helper.h"
#include "opencl/test/unit_test/command_queue/enqueue_fixture.h"
#include "opencl/test/unit_test/fixtures/cl_preemption_fixture.h"
#include "opencl/test/unit_test/mocks/mock_command_queue.h"

namespace NEO {

template <>
void HardwareParse::findCsrBaseAddress<Gen9Family>() {
    typedef typename Gen9::GPGPU_CSR_BASE_ADDRESS GPGPU_CSR_BASE_ADDRESS;
    itorGpgpuCsrBaseAddress = find<GPGPU_CSR_BASE_ADDRESS *>(cmdList.begin(), itorWalker);
    if (itorGpgpuCsrBaseAddress != itorWalker) {
        cmdGpgpuCsrBaseAddress = *itorGpgpuCsrBaseAddress;
    }
}
} // namespace NEO

using namespace NEO;

using Gen9PreemptionEnqueueKernelTest = PreemptionEnqueueKernelTest;
using Gen9MidThreadPreemptionEnqueueKernelTest = MidThreadPreemptionEnqueueKernelTest;
using Gen9ThreadGroupPreemptionEnqueueKernelTest = ThreadGroupPreemptionEnqueueKernelTest;

GEN9TEST_F(Gen9ThreadGroupPreemptionEnqueueKernelTest, givenSecondEnqueueWithTheSamePreemptionRequestThenDontReprogramThreadGroupNoWa) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);

    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = false;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.setMediaVFEStateDirty(false);
    auto csrSurface = csr.getPreemptionAllocation();
    EXPECT_EQ(nullptr, csrSurface);
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pClDevice);

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

    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = true;
    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.setMediaVFEStateDirty(false);
    auto csrSurface = csr.getPreemptionAllocation();
    EXPECT_EQ(nullptr, csrSurface);
    HardwareParse hwCsrParser;
    HardwareParse hwCmdQParser;
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pClDevice);

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

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pClDevice);
    MultiDispatchInfo multiDispatch(mockKernel.mockKernel);
    EXPECT_EQ(PreemptionMode::ThreadGroup, ClPreemptionHelper::taskPreemptionMode(*pDevice, multiDispatch));

    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    EXPECT_EQ(1, mockCsr->flushCalledCount);
    EXPECT_EQ(PreemptionMode::ThreadGroup, mockCsr->passedDispatchFlags.preemptionMode);
}

GEN9TEST_F(Gen9PreemptionEnqueueKernelTest, givenValidKernelForPreemptionWhenEnqueueKernelCalledAndBlockedThenPassDevicePreemptionModeThreadGroup) {
    pDevice->setPreemptionMode(PreemptionMode::ThreadGroup);

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pClDevice);
    MultiDispatchInfo multiDispatch(mockKernel.mockKernel);
    EXPECT_EQ(PreemptionMode::ThreadGroup, ClPreemptionHelper::taskPreemptionMode(*pDevice, multiDispatch));

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

    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = false;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.setMediaVFEStateDirty(false);
    auto csrSurface = csr.getPreemptionAllocation();
    ASSERT_NE(nullptr, csrSurface);
    HardwareParse hwCsrParser;
    HardwareParse hwCmdQParser;
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pClDevice);

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
    hwCmdQParser.parseCommands<FamilyType>(pCmdQ->getCS(1024), offsetCmdQ);
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

    pDevice->getRootDeviceEnvironment().getMutableHardwareInfo()->workaroundTable.flags.waModifyVFEStateAfterGPGPUPreemption = true;

    auto &csr = pDevice->getUltCommandStreamReceiver<FamilyType>();
    csr.getMemoryManager()->setForce32BitAllocations(false);
    csr.setMediaVFEStateDirty(false);
    auto csrSurface = csr.getPreemptionAllocation();
    ASSERT_NE(nullptr, csrSurface);
    HardwareParse hwCsrParser;
    HardwareParse hwCmdQParser;
    size_t off[3] = {0, 0, 0};
    size_t gws[3] = {1, 1, 1};

    MockKernelWithInternals mockKernel(*pClDevice);

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

    auto mockCsr = new MockCsrHw2<FamilyType>(*pDevice->executionEnvironment, pDevice->getRootDeviceIndex(), pDevice->getDeviceBitfield());
    pDevice->resetCommandStreamReceiver(mockCsr);

    MockKernelWithInternals mockKernel(*pClDevice);
    MultiDispatchInfo multiDispatch(mockKernel.mockKernel);
    EXPECT_EQ(PreemptionMode::Disabled, ClPreemptionHelper::taskPreemptionMode(*pDevice, multiDispatch));

    size_t gws[3] = {1, 0, 0};
    pCmdQ->enqueueKernel(mockKernel.mockKernel, 1, nullptr, gws, nullptr, 0, nullptr, nullptr);
    pCmdQ->flush();

    EXPECT_EQ(1, mockCsr->flushCalledCount);
    EXPECT_EQ(PreemptionMode::Disabled, mockCsr->passedDispatchFlags.preemptionMode);
}
