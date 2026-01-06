/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/kernel/grf_config.h"
#include "shared/source/xe2_hpg_core/hw_cmds_base.h"
#include "shared/source/xe2_hpg_core/hw_info_xe2_hpg_core.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/unit_test/command_stream/compute_mode_tests.h"

#include "per_product_test_definitions.h"

using namespace NEO;

using ThreadArbitrationXe2HpgCore = ::testing::Test;
XE2_HPG_CORETEST_F(ThreadArbitrationXe2HpgCore, givenBmgWhenCallingGetDefaultThreadArbitrationPolicyThenRoundRobinIsReturned) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobinAfterDependency, gfxCoreHelper.getDefaultThreadArbitrationPolicy());
}

using ComputeModeRequirementsXe2HpgCore = ComputeModeRequirements;

XE2_HPG_CORETEST_F(ComputeModeRequirementsXe2HpgCore, givenNewRequiredThreadArbitrationPolicyWhenComputeModeIsProgrammedThenStateComputeIsProgrammedAgain) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);
    auto &gfxCoreHelper = device->getGfxCoreHelper();
    auto newEuThreadSchedulingMode = gfxCoreHelper.getDefaultThreadArbitrationPolicy();
    typename STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE expectedEuThreadSchedulingMode = static_cast<typename STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE>(UnitTestHelper<FamilyType>::getAppropriateThreadArbitrationPolicy(newEuThreadSchedulingMode));

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setMask1(FamilyType::stateComputeModeLargeGrfModeMask | FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask);
    expectedScmCmd.setEuThreadSchedulingMode(expectedEuThreadSchedulingMode);

    overrideComputeModeRequest<FamilyType>(false, false, false, true, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    EXPECT_EQ(expectedEuThreadSchedulingMode, static_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase())->getEuThreadSchedulingMode());
}

XE2_HPG_CORETEST_F(ComputeModeRequirementsXe2HpgCore, givenRequiredThreadArbitrationPolicyAlreadySetWhenComputeModeIsProgrammedThenStateComputeIsNotProgrammedAgain) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = 0u;
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setMask1(FamilyType::stateComputeModeLargeGrfModeMask);

    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    EXPECT_FALSE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    EXPECT_NE(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE::EU_THREAD_SCHEDULING_MODE_ROUND_ROBIN, static_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase())->getEuThreadSchedulingMode());
}

XE2_HPG_CORETEST_F(ComputeModeRequirementsXe2HpgCore, givenCoherencyWithoutSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
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

XE2_HPG_CORETEST_F(ComputeModeRequirementsXe2HpgCore, givenNumGrfRequiredChangedWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
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

XE2_HPG_CORETEST_F(ComputeModeRequirementsXe2HpgCore, givenComputeModeProgrammingWhenLargeGrfModeDoesntChangeButRequiredThreadArbitrationPolicyIsNewThenSCMIsReloaded) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    char buff[1024];
    LinearStream stream(buff, 1024);
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);

    overrideComputeModeRequest<FamilyType>(false, false, false, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());
}

XE2_HPG_CORETEST_F(ComputeModeRequirementsXe2HpgCore, givenComputeModeProgrammingWhenLargeGrfModeChangedThenSCMIsReloadedAndLargeGrfModeProgrammed) {
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

XE2_HPG_CORETEST_F(ComputeModeRequirementsXe2HpgCore, givenComputeModeProgrammingWhenLargeGrfRequiredChangedButValueIsDefaultThenSCMIsReloadedButLargeGrfModeNotProgrammed) {
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

XE2_HPG_CORETEST_F(ComputeModeRequirementsXe2HpgCore, giventhreadArbitrationPolicyWithoutSharedHandlesWhenFlushTaskCalledThenProgramCmdOnlyIfChanged) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

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

                EXPECT_EQ(expectToBeProgrammed ? expectedMask : 0u, cmd->getMask1());
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
    findCmd(true); // first time

    flushTask(false);
    findCmd(false); // not changed

    flushTask(true);
    findCmd(true); // changed

    csr->getMemoryManager()->freeGraphicsMemory(graphicAlloc);
}

XE2_HPG_CORETEST_F(ComputeModeRequirementsXe2HpgCore, givenComputeModeProgrammingWhenLargeGrfModeChangeIsRequiredThenCorrectCommandsAreAdded) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024];
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(true);
    expectedScmCmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);
    expectedScmCmd.setMask1(FamilyType::stateComputeModeLargeGrfModeMask | FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask);

    overrideComputeModeRequest<FamilyType>(false, false, false, true, true, 256u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    auto startOffset = stream.getUsed();

    overrideComputeModeRequest<FamilyType>(false, false, false, true, true, 128u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize * 2, stream.getUsed());

    expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(false);
    expectedScmCmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);
    expectedScmCmd.setMask1(FamilyType::stateComputeModeLargeGrfModeMask | FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask);
    scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), startOffset));
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

XE2_HPG_CORETEST_F(ComputeModeRequirementsXe2HpgCore, givenComputeModeProgrammingWhenRequiredGRFNumberIsLowerThan128ThenSmallGRFModeIsProgrammed) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024];
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(false);
    expectedScmCmd.setEuThreadSchedulingMode(STATE_COMPUTE_MODE::EU_THREAD_SCHEDULING_MODE_STALL_BASED_ROUND_ROBIN);
    expectedScmCmd.setMask1(FamilyType::stateComputeModeLargeGrfModeMask | FamilyType::stateComputeModeEuThreadSchedulingModeOverrideMask);

    overrideComputeModeRequest<FamilyType>(false, false, false, true, true, 127u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

XE2_HPG_CORETEST_F(ComputeModeRequirementsXe2HpgCore, givenComputeModeProgrammingWhenRequiredGRFNumberIsGreaterThan128ThenLargeGRFModeIsProgrammed) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    char buff[1024];
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(true);
    auto expectedBitsMask = FamilyType::stateComputeModeLargeGrfModeMask;

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 256u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    EXPECT_TRUE(isValueSet(scmCmd->getMask1(), expectedBitsMask));
    expectedScmCmd.setMask1(scmCmd->getMask1());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

XE2_HPG_CORETEST_F(ComputeModeRequirementsXe2HpgCore, givenFlushWithoutSharedHandlesWhenPreviouslyUsedThenPcAndSCMAreNotProgrammed) {
    setUpImpl<FamilyType>();

    auto graphicAlloc = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap stream(graphicAlloc);

    makeResidentSharedAlloc();
    csr->flushTask(stream, 0, &stream, &stream, &stream, 0, flags, *device);
    EXPECT_TRUE(getCsrHw<FamilyType>()->getCsrRequestFlags()->hasSharedHandles);
    auto startOffset = getCsrHw<FamilyType>()->commandStream.getUsed();

    csr->flushTask(stream, 0, &stream, &stream, &stream, 0, flags, *device);
    EXPECT_TRUE(getCsrHw<FamilyType>()->getCsrRequestFlags()->hasSharedHandles);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(getCsrHw<FamilyType>()->commandStream, startOffset);

    EXPECT_EQ(0u, hwParser.cmdList.size());

    csr->getMemoryManager()->freeGraphicsMemory(graphicAlloc);
}

XE2_HPG_CORETEST_F(ComputeModeRequirementsXe2HpgCore, givenComputeModeCmdSizeWhenLargeGrfModeChangeIsRequiredThenSCMCommandSizeIsCalculated) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    auto cmdSize = sizeof(STATE_COMPUTE_MODE);

    overrideComputeModeRequest<FamilyType>(false, false, false, false, 128u);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    cmdSize = sizeof(STATE_COMPUTE_MODE);

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 256u);
    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdSize, retSize);

    overrideComputeModeRequest<FamilyType>(true, false, false, true, 256u);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdSize, retSize);
}
