/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/unit_test/cmd_parse/hw_parse.h"

#include "opencl/source/gen12lp/helpers_gen12lp.h"
#include "opencl/source/helpers/hardware_commands_helper.h"
#include "opencl/test/unit_test/mocks/mock_device.h"
#include "test.h"

using namespace NEO;

struct Gen12LpCoherencyRequirements : public ::testing::Test {
    using STATE_COMPUTE_MODE = typename TGLLPFamily::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename TGLLPFamily::PIPE_CONTROL;
    using PIPELINE_SELECT = typename TGLLPFamily::PIPELINE_SELECT;

    struct myCsr : public CommandStreamReceiverHw<TGLLPFamily> {
        using CommandStreamReceiver::commandStream;
        myCsr(ExecutionEnvironment &executionEnvironment) : CommandStreamReceiverHw<TGLLPFamily>(executionEnvironment, 0){};
        CsrSizeRequestFlags *getCsrRequestFlags() { return &csrSizeRequestFlags; }
    };

    void makeResidentSharedAlloc() {
        csr->getResidencyAllocations().push_back(alloc);
    }

    void overrideCoherencyRequest(bool reqestChanged, bool requireCoherency, bool hasSharedHandles) {
        csr->getCsrRequestFlags()->coherencyRequestChanged = reqestChanged;
        csr->getCsrRequestFlags()->hasSharedHandles = hasSharedHandles;
        flags.requiresCoherency = requireCoherency;
        if (hasSharedHandles) {
            makeResidentSharedAlloc();
        }
    }

    void SetUp() override {
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        csr = new myCsr(*device->executionEnvironment);
        device->resetCommandStreamReceiver(csr);
        AllocationProperties properties(device->getRootDeviceIndex(), false, MemoryConstants::pageSize, GraphicsAllocation::AllocationType::SHARED_BUFFER, false);

        alloc = device->getMemoryManager()->createGraphicsAllocationFromSharedHandle((osHandle)123, properties, false);
    }

    void TearDown() override {
        device->getMemoryManager()->freeGraphicsMemory(alloc);
    }

    myCsr *csr = nullptr;
    std::unique_ptr<MockDevice> device;
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    GraphicsAllocation *alloc = nullptr;
};

GEN12LPTEST_F(Gen12LpCoherencyRequirements, coherencyCmdSizeWithoutSharedHandles) {
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    auto &hwHelper = HwHelper::get(device->getHardwareInfo().platform.eRenderCoreFamily);
    if (hwHelper.is3DPipelineSelectWARequired(device->getHardwareInfo())) {
        cmdsSize += 2 * sizeof(PIPELINE_SELECT);
        if (Gen12LPHelpers::pipeControlWaRequired(device->getHardwareInfo().platform.eProductFamily)) {
            cmdsSize += 2 * sizeof(PIPE_CONTROL);
        }
    }

    overrideCoherencyRequest(false, false, false);
    auto retSize = csr->getCmdSizeForComputeMode();
    EXPECT_EQ(0u, retSize);

    overrideCoherencyRequest(false, true, false);
    retSize = csr->getCmdSizeForComputeMode();
    EXPECT_EQ(0u, retSize);

    overrideCoherencyRequest(true, true, false);
    retSize = csr->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);

    overrideCoherencyRequest(true, false, false);
    retSize = csr->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);
}

GEN12LPTEST_F(Gen12LpCoherencyRequirements, coherencyCmdSizeWithSharedHandles) {
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);
    auto &hwHelper = HwHelper::get(device->getHardwareInfo().platform.eRenderCoreFamily);
    if (hwHelper.is3DPipelineSelectWARequired(device->getHardwareInfo())) {
        cmdsSize += 2 * sizeof(PIPELINE_SELECT);
        if (Gen12LPHelpers::pipeControlWaRequired(device->getHardwareInfo().platform.eProductFamily)) {
            cmdsSize += 2 * sizeof(PIPE_CONTROL);
        }
    }

    overrideCoherencyRequest(false, false, true);
    auto retSize = csr->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);

    overrideCoherencyRequest(false, true, true);
    retSize = csr->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);

    overrideCoherencyRequest(true, true, true);
    retSize = csr->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);

    overrideCoherencyRequest(true, false, true);
    retSize = csr->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);
}

GEN12LPTEST_F(Gen12LpCoherencyRequirements, coherencyCmdValuesWithoutSharedHandles) {
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    auto cmdsSizeWABeginOffset = 0;
    auto &hwHelper = HwHelper::get(device->getHardwareInfo().platform.eRenderCoreFamily);
    if (hwHelper.is3DPipelineSelectWARequired(device->getHardwareInfo())) {
        cmdsSizeWABeginOffset += sizeof(PIPELINE_SELECT);
        cmdsSize += sizeof(PIPELINE_SELECT);
        if (Gen12LPHelpers::pipeControlWaRequired(device->getHardwareInfo().platform.eProductFamily)) {
            cmdsSizeWABeginOffset += sizeof(PIPE_CONTROL);
            cmdsSize += sizeof(PIPE_CONTROL);
        }
    }
    cmdsSize += cmdsSizeWABeginOffset;

    char buff[1024];
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask);

    overrideCoherencyRequest(true, false, false);
    csr->programComputeMode(stream, flags);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<char *>(stream.getCpuBase());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd + cmdsSizeWABeginOffset, sizeof(STATE_COMPUTE_MODE)) == 0);

    auto startOffset = stream.getUsed();

    overrideCoherencyRequest(true, true, false);
    csr->programComputeMode(stream, flags);
    EXPECT_EQ(cmdsSize * 2, stream.getUsed());

    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask);
    scmCmd = reinterpret_cast<char *>(ptrOffset(stream.getCpuBase(), startOffset));
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd + cmdsSizeWABeginOffset, sizeof(STATE_COMPUTE_MODE)) == 0);
}

GEN12LPTEST_F(Gen12LpCoherencyRequirements, coherencyCmdValuesWithSharedHandles) {
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);
    auto cmdsSizeWABeginOffset = 0;
    auto &hwHelper = HwHelper::get(device->getHardwareInfo().platform.eRenderCoreFamily);
    if (hwHelper.is3DPipelineSelectWARequired(device->getHardwareInfo())) {
        cmdsSizeWABeginOffset += sizeof(PIPELINE_SELECT);
        cmdsSize += sizeof(PIPELINE_SELECT);
        if (Gen12LPHelpers::pipeControlWaRequired(device->getHardwareInfo().platform.eProductFamily)) {
            cmdsSizeWABeginOffset += sizeof(PIPE_CONTROL);
            cmdsSize += sizeof(PIPE_CONTROL);
        }
    }
    cmdsSize += cmdsSizeWABeginOffset;

    char buff[1024];
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask);

    auto expectedPcCmd = FamilyType::cmdInitPipeControl;

    overrideCoherencyRequest(true, false, true);
    csr->programComputeMode(stream, flags);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<char *>(stream.getCpuBase());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd + cmdsSizeWABeginOffset, sizeof(STATE_COMPUTE_MODE)) == 0);

    auto pcCmd = reinterpret_cast<char *>(ptrOffset(stream.getCpuBase(), sizeof(STATE_COMPUTE_MODE)));
    EXPECT_TRUE(memcmp(&expectedPcCmd, pcCmd + cmdsSizeWABeginOffset, sizeof(PIPE_CONTROL)) == 0);

    auto startOffset = stream.getUsed();

    overrideCoherencyRequest(true, true, true);
    csr->programComputeMode(stream, flags);
    EXPECT_EQ(cmdsSize * 2, stream.getUsed());

    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask);
    scmCmd = reinterpret_cast<char *>(ptrOffset(stream.getCpuBase(), startOffset));
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd + cmdsSizeWABeginOffset, sizeof(STATE_COMPUTE_MODE)) == 0);

    pcCmd = reinterpret_cast<char *>(ptrOffset(stream.getCpuBase(), startOffset + sizeof(STATE_COMPUTE_MODE)));
    EXPECT_TRUE(memcmp(&expectedPcCmd, pcCmd + cmdsSizeWABeginOffset, sizeof(PIPE_CONTROL)) == 0);
}

GEN12LPTEST_F(Gen12LpCoherencyRequirements, givenCoherencyRequirementWithoutSharedHandlesWhenFlushTaskCalledThenProgramCmdOnlyIfChanged) {
    auto startOffset = csr->commandStream.getUsed();

    auto graphicAlloc = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap stream(graphicAlloc);

    auto flushTask = [&](bool coherencyRequired) {
        flags.requiresCoherency = coherencyRequired;
        startOffset = csr->commandStream.getUsed();
        csr->flushTask(stream, 0, stream, stream, stream, 0, flags, *device);
    };

    auto findCmd = [&](bool expectToBeProgrammed, bool expectCoherent, bool expectPipeControl) {
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, startOffset);
        bool foundOne = false;

        STATE_COMPUTE_MODE::FORCE_NON_COHERENT expectedCoherentValue = expectCoherent ? STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED : STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT;
        uint32_t expectedCoherentMask = FamilyType::stateComputeModeForceNonCoherentMask;

        for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
            auto cmd = genCmdCast<STATE_COMPUTE_MODE *>(*it);
            if (cmd) {
                EXPECT_EQ(expectedCoherentValue, cmd->getForceNonCoherent());
                EXPECT_EQ(expectedCoherentMask, cmd->getMaskBits());
                EXPECT_FALSE(foundOne);
                foundOne = true;
                auto pc = genCmdCast<PIPE_CONTROL *>(*(++it));
                if (!expectPipeControl && !Gen12LPHelpers::pipeControlWaRequired(device->getHardwareInfo().platform.eProductFamily)) {
                    EXPECT_EQ(nullptr, pc);
                } else {
                    EXPECT_NE(nullptr, pc);
                }
            }
        }
        EXPECT_EQ(expectToBeProgrammed, foundOne);
    };

    auto hwInfo = device->getHardwareInfo();

    flushTask(false);
    if (HardwareCommandsHelper<FamilyType>::isPipeControlPriorToPipelineSelectWArequired(hwInfo)) {
        findCmd(true, false, true); // first time
    } else {
        findCmd(true, false, false); // first time
    }

    flushTask(false);
    findCmd(false, false, false); // not changed

    flushTask(true);
    findCmd(true, true, false); // changed

    flushTask(true);
    findCmd(false, true, false); // not changed

    flushTask(false);
    findCmd(true, false, false); // changed

    flushTask(false);
    findCmd(false, false, false); // not changed
    csr->getMemoryManager()->freeGraphicsMemory(graphicAlloc);
}

GEN12LPTEST_F(Gen12LpCoherencyRequirements, givenCoherencyRequirementWithSharedHandlesWhenFlushTaskCalledThenAlwaysProgramCmds) {
    auto startOffset = csr->commandStream.getUsed();
    auto graphicsAlloc = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap stream(graphicsAlloc);

    auto flushTask = [&](bool coherencyRequired) {
        flags.requiresCoherency = coherencyRequired;
        makeResidentSharedAlloc();

        startOffset = csr->commandStream.getUsed();
        csr->flushTask(stream, 0, stream, stream, stream, 0, flags, *device);
    };

    auto flushTaskAndFindCmds = [&](bool expectCoherent) {
        flushTask(expectCoherent);
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, startOffset);
        bool foundOne = false;

        STATE_COMPUTE_MODE::FORCE_NON_COHERENT expectedCoherentValue = expectCoherent ? STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED : STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT;
        uint32_t expectedCoherentMask = FamilyType::stateComputeModeForceNonCoherentMask;

        for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
            auto cmd = genCmdCast<STATE_COMPUTE_MODE *>(*it);
            if (cmd) {
                EXPECT_EQ(expectedCoherentValue, cmd->getForceNonCoherent());
                EXPECT_EQ(expectedCoherentMask, cmd->getMaskBits());
                EXPECT_FALSE(foundOne);
                foundOne = true;
                auto pc = genCmdCast<PIPE_CONTROL *>(*(++it));
                EXPECT_NE(nullptr, pc);
            }
        }
        EXPECT_TRUE(foundOne);
    };

    flushTaskAndFindCmds(false); // first time
    flushTaskAndFindCmds(false); // not changed
    flushTaskAndFindCmds(true);  // changed
    flushTaskAndFindCmds(true);  // not changed
    flushTaskAndFindCmds(false); // changed
    flushTaskAndFindCmds(false); // not changed

    csr->getMemoryManager()->freeGraphicsMemory(graphicsAlloc);
}

GEN12LPTEST_F(Gen12LpCoherencyRequirements, givenFlushWithoutSharedHandlesWhenPreviouslyUsedThenProgramPcAndSCM) {
    auto graphicAlloc = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap stream(graphicAlloc);

    makeResidentSharedAlloc();
    csr->flushTask(stream, 0, stream, stream, stream, 0, flags, *device);
    EXPECT_TRUE(csr->getCsrRequestFlags()->hasSharedHandles);
    auto startOffset = csr->commandStream.getUsed();

    csr->flushTask(stream, 0, stream, stream, stream, 0, flags, *device);
    EXPECT_TRUE(csr->getCsrRequestFlags()->hasSharedHandles);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(csr->commandStream, startOffset);

    STATE_COMPUTE_MODE::FORCE_NON_COHERENT expectedCoherentValue = STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT;
    uint32_t expectedCoherentMask = FamilyType::stateComputeModeForceNonCoherentMask;

    bool foundOne = false;
    for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
        auto cmd = genCmdCast<STATE_COMPUTE_MODE *>(*it);
        if (cmd) {
            EXPECT_EQ(expectedCoherentValue, cmd->getForceNonCoherent());
            EXPECT_EQ(expectedCoherentMask, cmd->getMaskBits());
            EXPECT_FALSE(foundOne);
            foundOne = true;
            auto pc = genCmdCast<PIPE_CONTROL *>(*(++it));
            EXPECT_NE(nullptr, pc);
        }
    }
    EXPECT_TRUE(foundOne);

    csr->getMemoryManager()->freeGraphicsMemory(graphicAlloc);
}
