/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"

#include "opencl/test/unit_test/command_stream/compute_mode_tests.h"
#include "test.h"

#include "test_traits_common.h"

using namespace NEO;

HWCMDTEST_F(IGFX_XE_HP_CORE, ComputeModeRequirements, givenCoherencyWithoutSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    SetUpImpl<FamilyType>();

    getCsrHw<FamilyType>()->requiredThreadArbitrationPolicy = getCsrHw<FamilyType>()->lastSentThreadArbitrationPolicy;
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);

    overrideComputeModeRequest<FamilyType>(false, false, false);
    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(0u, retSize);

    overrideComputeModeRequest<FamilyType>(false, true, false);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(0u, retSize);

    overrideComputeModeRequest<FamilyType>(true, true, false);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);

    overrideComputeModeRequest<FamilyType>(true, false, false);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ComputeModeRequirements, givenCoherencyWithSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdsSize = 0u;

    overrideComputeModeRequest<FamilyType>(false, false, true);
    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);

    overrideComputeModeRequest<FamilyType>(false, true, true);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);

    cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);

    overrideComputeModeRequest<FamilyType>(true, true, true);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);

    overrideComputeModeRequest<FamilyType>(true, false, true);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);
}

struct ForceNonCoherentSupportedMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::forceNonCoherentSupported;
        }
        return false;
    }
};

HWTEST2_F(ComputeModeRequirements, givenCoherencyWithoutSharedHandlesWhenComputeModeIsProgrammedThenCorrectCommandsAreAdded, ForceNonCoherentSupportedMatcher) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    auto expectedBitsMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;

    overrideComputeModeRequest<FamilyType>(true, false, false, false);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    auto startOffset = stream.getUsed();

    overrideComputeModeRequest<FamilyType>(true, true, false, false);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize * 2, stream.getUsed());

    expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED);
    scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), startOffset));
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(ComputeModeRequirements, givenCoherencyWithSharedHandlesWhenComputeModeIsProgrammedThenCorrectCommandsAreAdded, ForceNonCoherentSupportedMatcher) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    auto expectedBitsMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;

    auto expectedPcCmd = FamilyType::cmdInitPipeControl;

    overrideComputeModeRequest<FamilyType>(true, false, true, false);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    auto pcCmd = reinterpret_cast<PIPE_CONTROL *>(ptrOffset(stream.getCpuBase(), sizeof(STATE_COMPUTE_MODE)));
    EXPECT_TRUE(memcmp(&expectedPcCmd, pcCmd, sizeof(PIPE_CONTROL)) == 0);

    auto startOffset = stream.getUsed();

    overrideComputeModeRequest<FamilyType>(true, true, true, false);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize * 2, stream.getUsed());

    expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED);
    scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), startOffset));
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    pcCmd = reinterpret_cast<PIPE_CONTROL *>(ptrOffset(stream.getCpuBase(), startOffset + sizeof(STATE_COMPUTE_MODE)));
    EXPECT_TRUE(memcmp(&expectedPcCmd, pcCmd, sizeof(PIPE_CONTROL)) == 0);
}

HWTEST2_F(ComputeModeRequirements, givenCoherencyRequirementWithoutSharedHandlesWhenFlushTaskCalledThenProgramCmdOnlyIfChanged, ForceNonCoherentSupportedMatcher) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto startOffset = getCsrHw<FamilyType>()->commandStream.getUsed();

    auto graphicAlloc = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap stream(graphicAlloc);

    auto flushTask = [&](bool coherencyRequired) {
        getCsrHw<FamilyType>()->lastSentThreadArbitrationPolicy = getCsrHw<FamilyType>()->requiredThreadArbitrationPolicy;
        flags.requiresCoherency = coherencyRequired;
        startOffset = getCsrHw<FamilyType>()->commandStream.getUsed();
        csr->flushTask(stream, 0, stream, stream, stream, 0, flags, *device);
    };

    auto findCmd = [&](bool expectToBeProgrammed, bool expectCoherent) {
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(getCsrHw<FamilyType>()->commandStream, startOffset);
        bool foundOne = false;

        typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT expectedCoherentValue = expectCoherent ? STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED : STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT;
        uint32_t expectedCoherentMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;

        for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
            auto cmd = genCmdCast<STATE_COMPUTE_MODE *>(*it);
            if (cmd) {
                EXPECT_EQ(expectedCoherentValue, cmd->getForceNonCoherent());
                EXPECT_TRUE(isValueSet(cmd->getMaskBits(), expectedCoherentMask));
                EXPECT_FALSE(foundOne);
                foundOne = true;
                auto pc = genCmdCast<PIPE_CONTROL *>(*(++it));
                EXPECT_EQ(nullptr, pc);
            }
        }
        EXPECT_EQ(expectToBeProgrammed, foundOne);
    };

    flushTask(false);
    findCmd(true, false); // first time

    flushTask(false);
    findCmd(false, false); // not changed

    flushTask(true);
    findCmd(true, true); // changed

    flushTask(true);
    findCmd(false, true); // not changed

    flushTask(false);
    findCmd(true, false); // changed

    flushTask(false);
    findCmd(false, false); // not changed
    csr->getMemoryManager()->freeGraphicsMemory(graphicAlloc);
}

HWTEST2_F(ComputeModeRequirements, givenCoherencyRequirementWithSharedHandlesWhenFlushTaskCalledThenProgramCmdsWhenNeeded, ForceNonCoherentSupportedMatcher) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto startOffset = getCsrHw<FamilyType>()->commandStream.getUsed();
    auto graphicsAlloc = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap stream(graphicsAlloc);

    auto flushTask = [&](bool coherencyRequired) {
        getCsrHw<FamilyType>()->lastSentThreadArbitrationPolicy = getCsrHw<FamilyType>()->requiredThreadArbitrationPolicy;
        flags.requiresCoherency = coherencyRequired;
        makeResidentSharedAlloc();

        startOffset = getCsrHw<FamilyType>()->commandStream.getUsed();
        csr->flushTask(stream, 0, stream, stream, stream, 0, flags, *device);
    };

    auto flushTaskAndFindCmds = [&](bool expectCoherent, bool areCommandsProgrammed) {
        flushTask(expectCoherent);
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(getCsrHw<FamilyType>()->commandStream, startOffset);
        bool foundOne = false;

        typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT expectedCoherentValue = expectCoherent ? STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED : STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT;
        uint32_t expectedCoherentMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;

        for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
            auto cmd = genCmdCast<STATE_COMPUTE_MODE *>(*it);
            if (cmd) {
                EXPECT_EQ(expectedCoherentValue, cmd->getForceNonCoherent());
                EXPECT_TRUE(isValueSet(cmd->getMaskBits(), expectedCoherentMask));
                EXPECT_FALSE(foundOne);
                foundOne = true;
                auto pc = genCmdCast<PIPE_CONTROL *>(*(++it));
                EXPECT_NE(nullptr, pc);
            }
        }
        EXPECT_EQ(foundOne, areCommandsProgrammed);
    };

    flushTaskAndFindCmds(false, true);  // first time
    flushTaskAndFindCmds(false, false); // not changed
    flushTaskAndFindCmds(true, true);   // changed
    flushTaskAndFindCmds(true, false);  // not changed
    flushTaskAndFindCmds(false, true);  // changed
    flushTaskAndFindCmds(false, false); // not changed

    csr->getMemoryManager()->freeGraphicsMemory(graphicsAlloc);
}

HWTEST2_F(ComputeModeRequirements, givenFlushWithoutSharedHandlesWhenPreviouslyUsedThenPcAndSCMAreNotProgrammed, ForceNonCoherentSupportedMatcher) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto graphicAlloc = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap stream(graphicAlloc);

    makeResidentSharedAlloc();
    csr->flushTask(stream, 0, stream, stream, stream, 0, flags, *device);
    EXPECT_TRUE(getCsrHw<FamilyType>()->getCsrRequestFlags()->hasSharedHandles);
    auto startOffset = getCsrHw<FamilyType>()->commandStream.getUsed();

    csr->flushTask(stream, 0, stream, stream, stream, 0, flags, *device);
    EXPECT_TRUE(getCsrHw<FamilyType>()->getCsrRequestFlags()->hasSharedHandles);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(getCsrHw<FamilyType>()->commandStream, startOffset);

    EXPECT_EQ(0u, hwParser.cmdList.size());

    csr->getMemoryManager()->freeGraphicsMemory(graphicAlloc);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ComputeModeRequirements, givenComputeModeCmdSizeWhenLargeGrfModeChangeIsRequiredThenSCMCommandSizeIsCalculated) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdSize = 0u;

    overrideComputeModeRequest<FamilyType>(false, false, false, false, 128u);
    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdSize, retSize);

    cmdSize = sizeof(STATE_COMPUTE_MODE);

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 256u);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdSize, retSize);

    overrideComputeModeRequest<FamilyType>(true, false, false, true, 256u);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdSize, retSize);
}

HWTEST2_F(ComputeModeRequirements, givenComputeModeProgrammingWhenLargeGrfModeChangeIsRequiredThenCorrectCommandsAreAdded, ForceNonCoherentSupportedMatcher) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024];
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    auto expectedBitsMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;

    expectedScmCmd.setLargeGrfMode(true);

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 256u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    auto startOffset = stream.getUsed();

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 128u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize * 2, stream.getUsed());

    expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(false);
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), startOffset));
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ComputeModeRequirements, givenComputeModeProgrammingWhenLargeGrfModeDoesntChangeThenSCMIsNotAdded) {
    SetUpImpl<FamilyType>();

    char buff[1024];
    LinearStream stream(buff, 1024);

    overrideComputeModeRequest<FamilyType>(false, false, false, false, 256u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(0u, stream.getUsed());
}

HWTEST2_F(ComputeModeRequirements, givenComputeModeProgrammingWhenRequiredGRFNumberIsLowerThan128ThenSmallGRFModeIsProgrammed, ForceNonCoherentSupportedMatcher) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024];
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(false);
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    auto expectedBitsMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 127u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(ComputeModeRequirements, givenComputeModeProgrammingWhenRequiredGRFNumberIsGreaterThan128ThenLargeGRFModeIsProgrammed, ForceNonCoherentSupportedMatcher) {
    SetUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024];
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setLargeGrfMode(true);
    auto expectedBitsMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;

    getCsrHw<FamilyType>()->requiredThreadArbitrationPolicy = ThreadArbitrationPolicy::NotPresent;

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 256u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}
