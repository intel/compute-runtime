/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/fixtures/command_container_fixture.h"
#include "shared/test/unit_test/mocks/mock_dispatch_kernel_encoder_interface.h"

using namespace NEO;

using CommandEncodeXeHpCoreTest = ::testing::Test;

XE_HP_CORE_TEST_F(CommandEncodeXeHpCoreTest, whenProgrammingStateComputeModeThenProperFieldsAreSet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    uint8_t buffer[64]{};

    StateComputeModeProperties properties;
    auto pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties, *defaultHwInfo, nullptr);
    auto pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    auto expectedMask = FamilyType::stateComputeModeForceNonCoherentMask |
                        FamilyType::stateComputeModeLargeGrfModeMask;
    EXPECT_EQ(expectedMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT, pScm->getForceNonCoherent());
    EXPECT_FALSE(pScm->getLargeGrfMode());

    properties.isCoherencyRequired.value = 1;
    properties.largeGrfMode.value = 1;
    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties, *defaultHwInfo, nullptr);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    EXPECT_EQ(expectedMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED, pScm->getForceNonCoherent());
    EXPECT_TRUE(pScm->getLargeGrfMode());
}

XE_HP_CORE_TEST_F(CommandEncodeXeHpCoreTest, givenForceDisableMultiAtomicsWhenDebugFlagIsZeroThenExpectForceDisableMultiAtomicsSetToFalse) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceMultiGpuAtomics.set(0);

    uint8_t buffer[64]{};

    StateComputeModeProperties properties;
    LinearStream cmdStream(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(cmdStream, properties, *defaultHwInfo, nullptr);
    auto scmCommand = reinterpret_cast<STATE_COMPUTE_MODE *>(cmdStream.getCpuBase());

    uint32_t expectedMaskBits = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask |
                                FamilyType::stateComputeModeForceDisableSupportMultiGpuAtomics;
    EXPECT_EQ(expectedMaskBits, scmCommand->getMaskBits());
    EXPECT_FALSE(scmCommand->getForceDisableSupportForMultiGpuAtomics());
}

XE_HP_CORE_TEST_F(CommandEncodeXeHpCoreTest, givenForceDisableMultiAtomicsWhenDebugFlagIsOneThenExpectForceDisableMultiAtomicsSetToTrue) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceMultiGpuAtomics.set(1);

    uint8_t buffer[64]{};

    StateComputeModeProperties properties;
    LinearStream cmdStream(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(cmdStream, properties, *defaultHwInfo, nullptr);
    auto scmCommand = reinterpret_cast<STATE_COMPUTE_MODE *>(cmdStream.getCpuBase());

    uint32_t expectedMaskBits = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask |
                                FamilyType::stateComputeModeForceDisableSupportMultiGpuAtomics;
    EXPECT_EQ(expectedMaskBits, scmCommand->getMaskBits());
    EXPECT_TRUE(scmCommand->getForceDisableSupportForMultiGpuAtomics());
}

XE_HP_CORE_TEST_F(CommandEncodeXeHpCoreTest, givenForceDisableMultiPartialWritesWhenDebugFlagIsZeroThenExpectForceDisableMultiPartialWritesSetToFalse) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceMultiGpuPartialWrites.set(0);

    uint8_t buffer[64]{};

    StateComputeModeProperties properties;
    LinearStream cmdStream(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(cmdStream, properties, *defaultHwInfo, nullptr);
    auto scmCommand = reinterpret_cast<STATE_COMPUTE_MODE *>(cmdStream.getCpuBase());

    uint32_t expectedMaskBits = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask |
                                FamilyType::stateComputeModeForceDisableSupportMultiGpuPartialWrites;
    EXPECT_EQ(expectedMaskBits, scmCommand->getMaskBits());
    EXPECT_FALSE(scmCommand->getForceDisableSupportForMultiGpuAtomics());
}

XE_HP_CORE_TEST_F(CommandEncodeXeHpCoreTest, givenForceDisableMultiPartialWritesWhenDebugFlagIsOneThenExpectForceDisableMultiPartialWritesSetToTrue) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;

    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ForceMultiGpuPartialWrites.set(1);

    uint8_t buffer[64]{};

    StateComputeModeProperties properties;
    LinearStream cmdStream(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(cmdStream, properties, *defaultHwInfo, nullptr);
    auto scmCommand = reinterpret_cast<STATE_COMPUTE_MODE *>(cmdStream.getCpuBase());

    uint32_t expectedMaskBits = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask |
                                FamilyType::stateComputeModeForceDisableSupportMultiGpuPartialWrites;
    EXPECT_EQ(expectedMaskBits, scmCommand->getMaskBits());
    EXPECT_TRUE(scmCommand->getForceDisableSupportForMultiGpuPartialWrites());
}

struct EncodeKernelGlobalAtomicsFixture : public CommandEncodeStatesFixture, public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.CreateMultipleSubDevices.set(2);
        osLocalMemoryBackup = std::make_unique<VariableBackup<bool>>(&OSInterface::osEnableLocalMemory, true);
        mockDeviceBackup = std::make_unique<VariableBackup<bool>>(&MockDevice::createSingleDevice, false);

        CommandEncodeStatesFixture::setUp();
    }

    void TearDown() override {
        CommandEncodeStatesFixture::tearDown();
    }

    DebugManagerStateRestore restorer;
    std::unique_ptr<VariableBackup<bool>> osLocalMemoryBackup;
    std::unique_ptr<VariableBackup<bool>> mockDeviceBackup;
};

struct EncodeKernelGlobalAtomicsTestWithImplicitScalingTests : public EncodeKernelGlobalAtomicsFixture {
    void SetUp() override {
        DebugManager.flags.EnableWalkerPartition.set(1);
        EncodeKernelGlobalAtomicsFixture::SetUp();
    }

    void TearDown() override {
        EncodeKernelGlobalAtomicsFixture::TearDown();
    }

    DebugManagerStateRestore restorer;
};

XE_HP_CORE_TEST_F(EncodeKernelGlobalAtomicsTestWithImplicitScalingTests, givenCleanHeapsAndSlmNotChangedAndNoUncachedMocsAndGlobalAtomicsUsedThenDisableSupportForMultiGpuAtomicsForStatelessAccessesIsFalse) {
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSize = 1;
    cmdContainer->setDirtyStateForAllHeaps(false);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSize;

    bool requiresUncachedMocs = false;
    bool useGlobalAtomics = true;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.useGlobalAtomics = useGlobalAtomics;
    dispatchArgs.partitionCount = 2;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs, nullptr);

    EXPECT_TRUE(cmdContainer->lastSentUseGlobalAtomics);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_NE(commands.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    EXPECT_FALSE(cmdSba->getDisableSupportForMultiGpuAtomicsForStatelessAccesses());
}

XE_HP_CORE_TEST_F(EncodeKernelGlobalAtomicsTestWithImplicitScalingTests, givenCleanHeapsAndSlmNotChangedAndNoUncachedMocsAndGlobalAtomicsUsedAndLastSentGlobalAtomicsTrueThenSbaIsNotProgrammed) {
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSize = 1;
    cmdContainer->setDirtyStateForAllHeaps(false);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSize;

    bool requiresUncachedMocs = false;
    bool useGlobalAtomics = true;
    cmdContainer->lastSentUseGlobalAtomics = true;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.useGlobalAtomics = useGlobalAtomics;
    dispatchArgs.partitionCount = 2;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs, nullptr);

    EXPECT_TRUE(cmdContainer->lastSentUseGlobalAtomics);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_EQ(commands.end(), itor);
}

XE_HP_CORE_TEST_F(EncodeKernelGlobalAtomicsTestWithImplicitScalingTests, givenCleanHeapsAndSlmNotChangedAndNoUncachedMocsAndNoGlobalAtomicsUsedAndLastSentGlobalAtomicsTrueThenDisableSupportForMultiGpuAtomicsForStatelessAccessesIsTrue) {
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSize = 1;
    cmdContainer->setDirtyStateForAllHeaps(false);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSize;

    bool requiresUncachedMocs = false;
    bool useGlobalAtomics = false;
    cmdContainer->lastSentUseGlobalAtomics = true;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.useGlobalAtomics = useGlobalAtomics;
    dispatchArgs.partitionCount = 2;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs, nullptr);

    EXPECT_FALSE(cmdContainer->lastSentUseGlobalAtomics);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_NE(commands.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    EXPECT_TRUE(cmdSba->getDisableSupportForMultiGpuAtomicsForStatelessAccesses());
}

XE_HP_CORE_TEST_F(EncodeKernelGlobalAtomicsTestWithImplicitScalingTests, givenCleanHeapsAndSlmNotChangedAndUncachedMocsAndNoGlobalAtomicsUsedAndLastSentGlobalAtomicsTrueThenDisableSupportForMultiGpuAtomicsForStatelessAccessesIsTrue) {
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSize = 1;
    cmdContainer->setDirtyStateForAllHeaps(false);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSize;

    bool requiresUncachedMocs = true;
    bool useGlobalAtomics = false;
    cmdContainer->lastSentUseGlobalAtomics = true;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.useGlobalAtomics = useGlobalAtomics;
    dispatchArgs.partitionCount = 2;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs, nullptr);

    EXPECT_FALSE(cmdContainer->lastSentUseGlobalAtomics);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_NE(commands.end(), itor);

    auto cmdSba = genCmdCast<STATE_BASE_ADDRESS *>(*itor);
    EXPECT_TRUE(cmdSba->getDisableSupportForMultiGpuAtomicsForStatelessAccesses());
}

XE_HP_CORE_TEST_F(EncodeKernelGlobalAtomicsTestWithImplicitScalingTests, givenCleanHeapsAndSlmNotChangedAndNoUncachedMocsAndNoGlobalAtomicsUsedThenSbaIsNotProgrammed) {
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSize = 1;
    cmdContainer->setDirtyStateForAllHeaps(false);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSize;

    bool requiresUncachedMocs = false;
    bool useGlobalAtomics = false;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.useGlobalAtomics = useGlobalAtomics;
    dispatchArgs.partitionCount = 2;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs, nullptr);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    ASSERT_EQ(commands.end(), itor);
}

struct EncodeKernelGlobalAtomicsTestWithNoImplicitScalingTests : public EncodeKernelGlobalAtomicsFixture {
    void SetUp() override {
        DebugManager.flags.EnableWalkerPartition.set(0);
        EncodeKernelGlobalAtomicsFixture::SetUp();
    }

    void TearDown() override {
        EncodeKernelGlobalAtomicsFixture::TearDown();
    }

    DebugManagerStateRestore restorer;
};

XE_HP_CORE_TEST_F(EncodeKernelGlobalAtomicsTestWithNoImplicitScalingTests, givenCleanHeapsAndSlmNotChangedAndNoUncachedMocsAndGlobalAtomicsUsedWithNoImplicitScalingThenSBAIsNotProgrammed) {
    uint32_t dims[] = {2, 1, 1};
    std::unique_ptr<MockDispatchKernelEncoder> dispatchInterface(new MockDispatchKernelEncoder());
    cmdContainer->slmSize = 1;
    cmdContainer->setDirtyStateForAllHeaps(false);
    dispatchInterface->getSlmTotalSizeResult = cmdContainer->slmSize;

    bool requiresUncachedMocs = false;
    bool useGlobalAtomics = true;
    EncodeDispatchKernelArgs dispatchArgs = createDefaultDispatchKernelArgs(pDevice, dispatchInterface.get(), dims, requiresUncachedMocs);
    dispatchArgs.useGlobalAtomics = useGlobalAtomics;

    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dispatchArgs, nullptr);

    EXPECT_FALSE(cmdContainer->lastSentUseGlobalAtomics);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    EXPECT_EQ(commands.end(), itor);
}
