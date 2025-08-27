/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/kernel/grf_config.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/command_stream/compute_mode_tests.h"

#include "hw_cmds_xe3_core.h"

using namespace NEO;

using ThreadArbitrationXe3Core = ::testing::Test;
XE3_CORETEST_F(ThreadArbitrationXe3Core, givenXe3DeviceWhenCallingGetDefaultThreadArbitrationPolicyThenRoundRobinIsReturned) {
    MockExecutionEnvironment mockExecutionEnvironment{};
    auto &gfxCoreHelper = mockExecutionEnvironment.rootDeviceEnvironments[0]->getHelper<GfxCoreHelper>();
    EXPECT_EQ(ThreadArbitrationPolicy::RoundRobinAfterDependency, gfxCoreHelper.getDefaultThreadArbitrationPolicy());
}

struct ComputeModeRequirementsXe3Core : public ComputeModeRequirements {
    void SetUp() override {
        ComputeModeRequirements::SetUp();
    }

    DebugManagerStateRestore restore;
};

XE3_CORETEST_F(ComputeModeRequirementsXe3Core, givenNewRequiredThreadArbitrationPolicyWhenComputeModeIsProgrammedThenStateComputeIsProgrammedAgain) {
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

XE3_CORETEST_F(ComputeModeRequirementsXe3Core, givenCoherencyWithoutSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
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

XE3_CORETEST_F(ComputeModeRequirementsXe3Core, givenNumGrfRequiredChangedWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
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

XE3_CORETEST_F(ComputeModeRequirementsXe3Core, givenComputeModeProgrammingWhenLargeGrfModeDoesntChangeButRequiredThreadArbitrationPolicyIsNewThenSCMIsReloaded) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    char buff[1024];
    LinearStream stream(buff, 1024);
    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);

    overrideComputeModeRequest<FamilyType>(false, false, false, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());
}

XE3_CORETEST_F(ComputeModeRequirementsXe3Core, givenComputeModeProgrammingWhenLargeGrfModeChangedThenSCMIsReloadedAndLargeGrfModeProgrammed) {
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

XE3_CORETEST_F(ComputeModeRequirementsXe3Core, givenComputeModeProgrammingWhenLargeGrfRequiredChangedButValueIsDefaultThenSCMIsReloadedButLargeGrfModeNotProgrammed) {
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

XE3_CORETEST_F(ComputeModeRequirementsXe3Core, giventhreadArbitrationPolicyWithoutSharedHandlesWhenFlushTaskCalledThenProgramCmdOnlyIfChanged) {
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

        uint32_t expectedMask = FamilyType::stateComputeModePipelinedEuThreadArbitrationMask;

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

XE3_CORETEST_F(ComputeModeRequirementsXe3Core, givenComputeModeProgrammingWhenLargeGrfModeChangeIsRequiredThenCorrectCommandsAreAdded) {
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

XE3_CORETEST_F(ComputeModeRequirementsXe3Core, givenFlushWithoutSharedHandlesWhenPreviouslyUsedThenPcAndPreemptionModeAreNotProgrammed) {
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

XE3_CORETEST_F(ComputeModeRequirementsXe3Core, givenComputeModeCmdSizeWhenLargeGrfModeChangeIsRequiredThenSCMCommandSizeIsCalculated) {
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
