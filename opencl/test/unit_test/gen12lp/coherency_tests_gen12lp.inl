/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/command_stream_receiver_hw.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/libult/gen12lp/special_ult_helper_gen12lp.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct Gen12LpCoherencyRequirements : public ::testing::Test {
    using STATE_COMPUTE_MODE = typename TGLLPFamily::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename TGLLPFamily::PIPE_CONTROL;
    using PIPELINE_SELECT = typename TGLLPFamily::PIPELINE_SELECT;

    struct myCsr : public CommandStreamReceiverHw<TGLLPFamily> {
        using CommandStreamReceiver::commandStream;
        using CommandStreamReceiver::streamProperties;
        myCsr(ExecutionEnvironment &executionEnvironment) : CommandStreamReceiverHw<TGLLPFamily>(executionEnvironment, 0, 1){};
        CsrSizeRequestFlags *getCsrRequestFlags() { return &csrSizeRequestFlags; }
    };

    void makeResidentSharedAlloc() {
        csr->getResidencyAllocations().push_back(alloc);
    }

    void overrideCoherencyRequest(bool reqestChanged, bool requireCoherency, bool hasSharedHandles) {
        csr->getCsrRequestFlags()->hasSharedHandles = hasSharedHandles;
        flags.requiresCoherency = requireCoherency;
        csr->streamProperties.stateComputeMode.isCoherencyRequired.value = requireCoherency;
        csr->streamProperties.stateComputeMode.isCoherencyRequired.isDirty = reqestChanged;
        if (hasSharedHandles) {
            makeResidentSharedAlloc();
        }
    }

    void SetUp() override {
        device.reset(MockDevice::createWithNewExecutionEnvironment<MockDevice>(defaultHwInfo.get()));
        csr = new myCsr(*device->executionEnvironment);
        device->resetCommandStreamReceiver(csr);
        AllocationProperties properties(device->getRootDeviceIndex(), false, MemoryConstants::pageSize, AllocationType::SHARED_BUFFER, false, {});

        alloc = device->getMemoryManager()->createGraphicsAllocationFromSharedHandle(static_cast<osHandle>(123), properties, false, false);
    }

    void TearDown() override {
        device->getMemoryManager()->freeGraphicsMemory(alloc);
    }

    myCsr *csr = nullptr;
    std::unique_ptr<MockDevice> device;
    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    GraphicsAllocation *alloc = nullptr;
};

GEN12LPTEST_F(Gen12LpCoherencyRequirements, GivenNoSharedHandlesWhenGettingCmdSizeThenSizeIsCorrect) {
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    const auto &hwInfoConfig = *HwInfoConfig::get(device->getHardwareInfo().platform.eProductFamily);
    if (hwInfoConfig.is3DPipelineSelectWARequired()) {
        cmdsSize += 2 * sizeof(PIPELINE_SELECT);
        if (SpecialUltHelperGen12lp::isPipeControlWArequired(device->getHardwareInfo().platform.eProductFamily)) {
            cmdsSize += 2 * sizeof(PIPE_CONTROL);
        }
    }

    overrideCoherencyRequest(false, false, false);
    EXPECT_FALSE(csr->streamProperties.stateComputeMode.isDirty());

    overrideCoherencyRequest(false, true, false);
    EXPECT_FALSE(csr->streamProperties.stateComputeMode.isDirty());

    overrideCoherencyRequest(true, true, false);
    auto retSize = csr->getCmdSizeForComputeMode();
    EXPECT_TRUE(csr->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);

    overrideCoherencyRequest(true, false, false);
    retSize = csr->getCmdSizeForComputeMode();
    EXPECT_TRUE(csr->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
}

GEN12LPTEST_F(Gen12LpCoherencyRequirements, GivenSharedHandlesWhenGettingCmdSizeThenSizeIsCorrect) {
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);
    const auto &hwInfoConfig = *HwInfoConfig::get(device->getHardwareInfo().platform.eProductFamily);
    if (hwInfoConfig.is3DPipelineSelectWARequired()) {
        cmdsSize += 2 * sizeof(PIPELINE_SELECT);
        if (SpecialUltHelperGen12lp::isPipeControlWArequired(device->getHardwareInfo().platform.eProductFamily)) {
            cmdsSize += 2 * sizeof(PIPE_CONTROL);
        }
    }

    overrideCoherencyRequest(false, false, true);
    EXPECT_FALSE(csr->streamProperties.stateComputeMode.isDirty());

    overrideCoherencyRequest(false, true, true);
    EXPECT_FALSE(csr->streamProperties.stateComputeMode.isDirty());

    overrideCoherencyRequest(true, true, true);
    auto retSize = csr->getCmdSizeForComputeMode();
    EXPECT_TRUE(csr->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);

    overrideCoherencyRequest(true, false, true);
    retSize = csr->getCmdSizeForComputeMode();
    EXPECT_TRUE(csr->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
}

GEN12LPTEST_F(Gen12LpCoherencyRequirements, GivenNoSharedHandlesThenCoherencyCmdValuesAreCorrect) {
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    auto cmdsSizeWABeginOffset = 0;
    const auto &hwInfoConfig = *HwInfoConfig::get(device->getHardwareInfo().platform.eProductFamily);
    if (hwInfoConfig.is3DPipelineSelectWARequired()) {
        cmdsSizeWABeginOffset += sizeof(PIPELINE_SELECT);
        cmdsSize += sizeof(PIPELINE_SELECT);
        if (SpecialUltHelperGen12lp::isPipeControlWArequired(device->getHardwareInfo().platform.eProductFamily)) {
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
    csr->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<char *>(stream.getCpuBase());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd + cmdsSizeWABeginOffset, sizeof(STATE_COMPUTE_MODE)) == 0);

    auto startOffset = stream.getUsed();

    overrideCoherencyRequest(true, true, false);
    csr->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize * 2, stream.getUsed());

    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask);
    scmCmd = reinterpret_cast<char *>(ptrOffset(stream.getCpuBase(), startOffset));
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd + cmdsSizeWABeginOffset, sizeof(STATE_COMPUTE_MODE)) == 0);
}

GEN12LPTEST_F(Gen12LpCoherencyRequirements, GivenSharedHandlesThenCoherencyCmdValuesAreCorrect) {
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);
    auto cmdsSizeWABeginOffset = 0;
    const auto &hwInfoConfig = *HwInfoConfig::get(device->getHardwareInfo().platform.eProductFamily);
    if (hwInfoConfig.is3DPipelineSelectWARequired()) {
        cmdsSizeWABeginOffset += sizeof(PIPELINE_SELECT);
        cmdsSize += sizeof(PIPELINE_SELECT);
        if (SpecialUltHelperGen12lp::isPipeControlWArequired(device->getHardwareInfo().platform.eProductFamily)) {
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
    csr->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<char *>(stream.getCpuBase());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd + cmdsSizeWABeginOffset, sizeof(STATE_COMPUTE_MODE)) == 0);

    auto pcCmd = reinterpret_cast<char *>(ptrOffset(stream.getCpuBase(), sizeof(STATE_COMPUTE_MODE)));
    EXPECT_TRUE(memcmp(&expectedPcCmd, pcCmd + cmdsSizeWABeginOffset, sizeof(PIPE_CONTROL)) == 0);

    auto startOffset = stream.getUsed();

    overrideCoherencyRequest(true, true, true);
    csr->programComputeMode(stream, flags, *defaultHwInfo);
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
        csr->flushTask(stream, 0, &stream, &stream, &stream, 0, flags, *device);
    };

    auto findCmd = [&](bool expectToBeProgrammed, bool expectCoherent, bool expectPipeControl) {
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, startOffset);
        bool foundOne = false;

        STATE_COMPUTE_MODE::FORCE_NON_COHERENT expectedCoherentValue = STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT;
        uint32_t expectedCoherentMask = FamilyType::stateComputeModeForceNonCoherentMask;

        for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
            auto cmd = genCmdCast<STATE_COMPUTE_MODE *>(*it);
            if (cmd) {
                EXPECT_EQ(expectedCoherentValue, cmd->getForceNonCoherent());
                EXPECT_EQ(expectedCoherentMask, cmd->getMaskBits());
                EXPECT_FALSE(foundOne);
                foundOne = true;
                auto pc = genCmdCast<PIPE_CONTROL *>(*(++it));
                if (!expectPipeControl && !SpecialUltHelperGen12lp::isPipeControlWArequired(device->getHardwareInfo().platform.eProductFamily)) {
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
    if (MemorySynchronizationCommands<FamilyType>::isPipeControlPriorToPipelineSelectWArequired(hwInfo)) {
        findCmd(true, false, true); // first time
    } else {
        findCmd(true, false, false); // first time
    }

    flushTask(false);
    findCmd(false, false, false); // not changed

    csr->getMemoryManager()->freeGraphicsMemory(graphicAlloc);
}

GEN12LPTEST_F(Gen12LpCoherencyRequirements, givenSharedHandlesWhenFlushTaskCalledThenProgramPipeControlWhenNeeded) {
    auto startOffset = csr->commandStream.getUsed();
    auto graphicsAlloc = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap stream(graphicsAlloc);

    auto flushTask = [&](bool coherencyRequired) {
        flags.requiresCoherency = coherencyRequired;
        makeResidentSharedAlloc();

        startOffset = csr->commandStream.getUsed();
        csr->flushTask(stream, 0, &stream, &stream, &stream, 0, flags, *device);
    };

    auto flushTaskAndFindCmds = [&](bool expectCoherent, bool valueChanged) {
        flushTask(expectCoherent);
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(csr->commandStream, startOffset);
        bool foundOne = false;
        STATE_COMPUTE_MODE::FORCE_NON_COHERENT expectedCoherentValue = STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT;
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
        EXPECT_EQ(valueChanged, foundOne);
    };

    flushTaskAndFindCmds(false, true);  // first time
    flushTaskAndFindCmds(false, false); // not changed

    csr->getMemoryManager()->freeGraphicsMemory(graphicsAlloc);
}

GEN12LPTEST_F(Gen12LpCoherencyRequirements, givenFlushWithoutSharedHandlesWhenPreviouslyUsedThenProgramPcAndSCM) {
    auto graphicAlloc = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap stream(graphicAlloc);

    makeResidentSharedAlloc();
    csr->flushTask(stream, 0, &stream, &stream, &stream, 0, flags, *device);
    EXPECT_TRUE(csr->getCsrRequestFlags()->hasSharedHandles);
    auto startOffset = csr->commandStream.getUsed();

    csr->streamProperties.stateComputeMode.isCoherencyRequired.set(true);
    csr->flushTask(stream, 0, &stream, &stream, &stream, 0, flags, *device);
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
