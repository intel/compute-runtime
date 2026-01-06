/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/grf_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/command_stream/compute_mode_tests.h"

using namespace NEO;

using ThreadArbitrationXeHpc = ::testing::Test;
HWTEST2_F(ThreadArbitrationXeHpc, givenXeHpcWhenCallgetDefaultThreadArbitrationPolicyThenAgeBasedisReturned, IsXeHpcCore) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobinAfterDependency, gfxCoreHelper.getDefaultThreadArbitrationPolicy());
}

using XeHpcComputeModeRequirements = ComputeModeRequirements;

HWTEST2_F(XeHpcComputeModeRequirements, givenNewRequiredThreadArbitrationPolicyWhenComputeModeIsProgrammedThenStateComputeIsProgrammedAgain, IsXeHpcCore) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using EU_THREAD_SCHEDULING_MODE = typename STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto newEuThreadSchedulingMode = gfxCoreHelper.getDefaultThreadArbitrationPolicy();
    auto expectedEuThreadSchedulingMode = static_cast<EU_THREAD_SCHEDULING_MODE>(UnitTestHelper<FamilyType>::getAppropriateThreadArbitrationPolicy(newEuThreadSchedulingMode));

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask | FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask);
    expectedScmCmd.setEuThreadSchedulingMode(expectedEuThreadSchedulingMode);

    overrideComputeModeRequest<FamilyType>(true, false, false, true, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    EXPECT_EQ(expectedEuThreadSchedulingMode, static_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase())->getEuThreadSchedulingMode());
}

HWTEST2_F(XeHpcComputeModeRequirements, givenRequiredThreadArbitrationPolicyAlreadySetWhenComputeModeIsProgrammedThenStateComputeIsNotProgrammedAgain, IsXeHpcCore) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = 0u;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask);

    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    EXPECT_FALSE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    EXPECT_NE(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_ROUND_ROBIN, static_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase())->getEuThreadSchedulingMode());
}

HWTEST2_F(XeHpcComputeModeRequirements, givenCoherencyWithoutSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned, IsXeHpcCore) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    setUpImpl<FamilyType>();

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);

    overrideComputeModeRequest<FamilyType>(false, false, false, false);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    overrideComputeModeRequest<FamilyType>(false, false, false, true);
    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
}

HWTEST2_F(XeHpcComputeModeRequirements, givenNumGrfRequiredChangedWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned, IsXeHpcCore) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    setUpImpl<FamilyType>();

    auto numGrfRequired = 128u;
    auto numGrfRequiredChanged = false;
    overrideComputeModeRequest<FamilyType>(false, false, false, numGrfRequiredChanged, numGrfRequired);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    numGrfRequiredChanged = true;
    overrideComputeModeRequest<FamilyType>(false, false, false, numGrfRequiredChanged, numGrfRequired);
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(sizeof(STATE_COMPUTE_MODE), getCsrHw<FamilyType>()->getCmdSizeForComputeMode());
}

HWTEST2_F(XeHpcComputeModeRequirements, givenComputeModeProgrammingWhenLargeGrfModeDoesntChangeButRequiredThreadArbitrationPolicyIsNewThenSCMIsReloaded, IsXeHpcCore) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    char buff[1024];
    LinearStream stream(buff, 1024);
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);

    overrideComputeModeRequest<FamilyType>(false, false, false, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());
}

HWTEST2_F(XeHpcComputeModeRequirements, givenComputeModeProgrammingWhenLargeGrfModeChangedThenSCMIsReloadedAndLargeGrfModeProgrammed, IsXeHpcCore) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    char buff[1024];
    LinearStream stream(buff, 1024);
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    uint32_t numGrfRequired = GrfConfig::largeGrfNumber;

    overrideComputeModeRequest<FamilyType>(false, false, false, true, numGrfRequired);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    EXPECT_TRUE(static_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase())->getLargeGrfMode());
}

HWTEST2_F(XeHpcComputeModeRequirements, givenComputeModeProgrammingWhenLargeGrfRequiredChangedButValueIsDefaultThenSCMIsReloadedButLargeGrfModeNotProgrammed, IsXeHpcCore) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    char buff[1024];
    LinearStream stream(buff, 1024);
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    uint32_t numGrfRequired = GrfConfig::defaultGrfNumber;

    overrideComputeModeRequest<FamilyType>(false, false, false, true, numGrfRequired);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());
    EXPECT_FALSE(static_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase())->getLargeGrfMode());
}

HWTEST2_F(XeHpcComputeModeRequirements, giventhreadArbitrationPolicyWithoutSharedHandlesWhenFlushTaskCalledThenProgramCmdOnlyIfChanged, IsXeHpcCore) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &productHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<ProductHelper>();

    auto startOffset = getCsrHw<FamilyType>()->commandStream.getUsed();

    auto graphicAlloc = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap stream(graphicAlloc);

    auto flushTask = [&](bool threadArbitrationPolicyChanged) {
        if (threadArbitrationPolicyChanged) {
            getCsrHw<FamilyType>()->streamProperties.stateComputeMode.threadArbitrationPolicy.value = -1;
        }
        startOffset = getCsrHw<FamilyType>()->commandStream.getUsed();
        csr->flushTask(stream, 0, &stream, &stream, &stream, 0, flags, *device);
    };

    auto findCmd = [&](bool expectToBeProgrammed) {
        HardwareParse hwParser;
        hwParser.parseCommands<FamilyType>(getCsrHw<FamilyType>()->commandStream, startOffset);
        bool foundOne = false;

        uint32_t expectedMask = FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask;

        for (auto it = hwParser.cmdList.begin(); it != hwParser.cmdList.end(); it++) {
            auto cmd = genCmdCast<STATE_COMPUTE_MODE *>(*it);
            if (cmd) {
                EXPECT_EQ(expectedMask, cmd->getMaskBits());
                EXPECT_FALSE(foundOne);
                foundOne = true;
                auto pc = genCmdCast<PIPE_CONTROL *>(*(++it));
                EXPECT_EQ(nullptr, pc);
            }
        }
        EXPECT_EQ(expectToBeProgrammed, foundOne);
    };

    getCsrHw<FamilyType>()->streamProperties.stateComputeMode.setPropertiesAll(false, flags.numGrfRequired,
                                                                               flags.threadArbitrationPolicy, PreemptionMode::Disabled, false);

    flushTask(true);
    findCmd(productHelper.isThreadArbitrationPolicyReportedWithScm()); // first time

    flushTask(false);
    findCmd(false); // not changed

    flushTask(true);
    findCmd(productHelper.isThreadArbitrationPolicyReportedWithScm()); // changed

    csr->getMemoryManager()->freeGraphicsMemory(graphicAlloc);
}

HWTEST2_F(XeHpcComputeModeRequirements, givenCoherencyWithoutSharedHandlesWhenComputeModeIsProgrammedThenCorrectCommandsAreAdded, IsXeHpcCore) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask |
                               FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask);

    overrideComputeModeRequest<FamilyType>(true, false, false, true, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    auto startOffset = stream.getUsed();

    overrideComputeModeRequest<FamilyType>(true, true, false, true, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize * 2, stream.getUsed());

    expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED);
    expectedScmCmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask |
                               FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask);
    scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), startOffset));
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(XeHpcComputeModeRequirements, givenCoherencyWithSharedHandlesWhenComputeModeIsProgrammedThenCorrectCommandsAreAdded, IsXeHpcCore) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask |
                               FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask);

    auto expectedPcCmd = FamilyType::cmdInitPipeControl;

    overrideComputeModeRequest<FamilyType>(true, false, true, true, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    auto pcCmd = reinterpret_cast<PIPE_CONTROL *>(ptrOffset(stream.getCpuBase(), sizeof(STATE_COMPUTE_MODE)));
    EXPECT_TRUE(memcmp(&expectedPcCmd, pcCmd, sizeof(PIPE_CONTROL)) == 0);

    auto startOffset = stream.getUsed();

    overrideComputeModeRequest<FamilyType>(true, true, true, true, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize * 2, stream.getUsed());

    expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED);
    expectedScmCmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask |
                               FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask);
    scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), startOffset));
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    pcCmd = reinterpret_cast<PIPE_CONTROL *>(ptrOffset(stream.getCpuBase(), startOffset + sizeof(STATE_COMPUTE_MODE)));
    EXPECT_TRUE(memcmp(&expectedPcCmd, pcCmd, sizeof(PIPE_CONTROL)) == 0);
}

HWTEST2_F(XeHpcComputeModeRequirements, givenComputeModeProgrammingWhenLargeGrfModeChangeIsRequiredThenCorrectCommandsAreAdded, IsXeHpcCore) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024];
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(true);
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask |
                               FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask);

    overrideComputeModeRequest<FamilyType>(true, false, false, true, true, 256u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    EXPECT_TRUE(memcmp(&expectedScmCmd, stream.getCpuBase(), sizeof(STATE_COMPUTE_MODE)) == 0);

    auto startOffset = stream.getUsed();

    overrideComputeModeRequest<FamilyType>(true, false, false, true, true, 128u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize * 2, stream.getUsed());

    expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(false);
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask |
                               FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask);
    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), startOffset));
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(XeHpcComputeModeRequirements, givenComputeModeProgrammingWhenRequiredGRFNumberIsLowerThan128ThenSmallGRFModeIsProgrammed, IsXeHpcCore) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024];
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(false);
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask |
                               FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask);

    overrideComputeModeRequest<FamilyType>(true, false, false, true, true, 127u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    EXPECT_TRUE(memcmp(&expectedScmCmd, stream.getCpuBase(), sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(XeHpcComputeModeRequirements, givenComputeModeProgrammingThenCorrectCommandsAreAdded, IsXeHpcCore) {
    setUpImpl<FamilyType>();

    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask |
                               FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask);

    overrideComputeModeRequest<FamilyType>(true, false, false, true, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto startOffset = getCsrHw<FamilyType>()->commandStream.getUsed();

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, startOffset);

    auto stateComputeModeCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    expectedScmCmd.setMaskBits(stateComputeModeCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, stateComputeModeCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(XeHpcComputeModeRequirements, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledThenCorrectCommandsAreAdded, IsXeHpcCore) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(true);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    setUpImpl<FamilyType>(&hwInfo);

    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask |
                               FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask);

    overrideComputeModeRequest<FamilyType>(true, false, false, true, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, hwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto stateComputeModeCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    expectedScmCmd.setMaskBits(stateComputeModeCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, stateComputeModeCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(XeHpcComputeModeRequirements, givenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledWhenaddPipeControlBefore3dStateIsCalledThenCorrectCommandsAreAdded, IsXeHpcCore) {
    DebugManagerStateRestore dbgRestorer;
    debugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(true);

    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    setUpImpl<FamilyType>(&hwInfo);
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    flags.usePerDssBackedBuffer = true;
    getCsrHw<FamilyType>()->addPipeControlBefore3dState(stream, flags);

    auto startOffset = getCsrHw<FamilyType>()->commandStream.getUsed();

    HardwareParse hwParser;

    hwParser.parseCommands<FamilyType>(stream, startOffset);

    auto pipeControlIterator = find<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControlIterator);

    EXPECT_TRUE(pipeControlCmd->getHdcPipelineFlush());
    EXPECT_TRUE(pipeControlCmd->getAmfsFlushEnable());
    EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControlCmd->getInstructionCacheInvalidateEnable());
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getUnTypedDataPortCacheFlush());
    EXPECT_TRUE(pipeControlCmd->getConstantCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getStateCacheInvalidationEnable());
}

HWTEST2_F(XeHpcComputeModeRequirements, GivenSingleCCSEnabledSetupThenCorrectCommandsAreAdded, IsXeHpcCore) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    setUpImpl<FamilyType>(&hwInfo);

    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);
    expectedScmCmd.setMaskBits(FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask |
                               FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask);

    overrideComputeModeRequest<FamilyType>(true, false, false, true, true);

    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);

    getCsrHw<FamilyType>()->programComputeMode(stream, flags, hwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto stateComputeModeCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    expectedScmCmd.setMaskBits(stateComputeModeCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, stateComputeModeCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}
