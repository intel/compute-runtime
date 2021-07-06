/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/fixtures/command_container_fixture.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/mocks/mock_dispatch_kernel_encoder_interface.h"

#include "test.h"

#include "hw_cmds.h"

using namespace NEO;

using CommandEncodeXeHpCoreTest = ::testing::Test;

XE_HP_CORE_TEST_F(CommandEncodeXeHpCoreTest, whenProgrammingStateComputeModeThenProperFieldsAreSet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    uint8_t buffer[64]{};

    StateComputeModeProperties properties;
    auto pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::adjustComputeMode(*pLinearStream, nullptr, properties, *defaultHwInfo);
    auto pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    EXPECT_EQ(0u, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED, pScm->getForceNonCoherent());
    EXPECT_FALSE(pScm->getLargeGrfMode());

    properties.isCoherencyRequired.value = 0;
    properties.largeGrfMode.value = 1;
    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::adjustComputeMode(*pLinearStream, nullptr, properties, *defaultHwInfo);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    EXPECT_EQ(0u, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED, pScm->getForceNonCoherent());
    EXPECT_FALSE(pScm->getLargeGrfMode());

    properties.isCoherencyRequired.isDirty = true;
    properties.largeGrfMode.isDirty = true;
    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::adjustComputeMode(*pLinearStream, nullptr, properties, *defaultHwInfo);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    auto expectedMask = FamilyType::stateComputeModeForceNonCoherentMask |
                        FamilyType::stateComputeModeLargeGrfModeMask;
    EXPECT_EQ(expectedMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT, pScm->getForceNonCoherent());
    EXPECT_TRUE(pScm->getLargeGrfMode());
}

struct EncodeKernelGlobalAtomicsFixture : public CommandEncodeStatesFixture, public ::testing::Test {
    void SetUp() override {
        DebugManager.flags.CreateMultipleSubDevices.set(2);
        osLocalMemoryBackup = std::make_unique<VariableBackup<bool>>(&OSInterface::osEnableLocalMemory, true);
        mockDeviceBackup = std::make_unique<VariableBackup<bool>>(&MockDevice::createSingleDevice, false);

        CommandEncodeStatesFixture::SetUp();
    }

    void TearDown() override {
        CommandEncodeStatesFixture::TearDown();
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
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(cmdContainer->slmSize));
    cmdContainer->setDirtyStateForAllHeaps(false);

    bool requiresUncachedMocs = false;
    bool useGlobalAtomics = true;
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, useGlobalAtomics, partitionCount, false);

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
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(cmdContainer->slmSize));
    cmdContainer->setDirtyStateForAllHeaps(false);

    bool requiresUncachedMocs = false;
    bool useGlobalAtomics = true;
    cmdContainer->lastSentUseGlobalAtomics = true;
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, useGlobalAtomics, partitionCount, false);

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
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(cmdContainer->slmSize));
    cmdContainer->setDirtyStateForAllHeaps(false);

    bool requiresUncachedMocs = false;
    bool useGlobalAtomics = false;
    cmdContainer->lastSentUseGlobalAtomics = true;
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, useGlobalAtomics, partitionCount, false);

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
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(cmdContainer->slmSize));
    cmdContainer->setDirtyStateForAllHeaps(false);

    bool requiresUncachedMocs = true;
    bool useGlobalAtomics = false;
    cmdContainer->lastSentUseGlobalAtomics = true;
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, useGlobalAtomics, partitionCount, false);

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
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(cmdContainer->slmSize));
    cmdContainer->setDirtyStateForAllHeaps(false);

    bool requiresUncachedMocs = false;
    bool useGlobalAtomics = false;
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, useGlobalAtomics, partitionCount, false);

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
    EXPECT_CALL(*dispatchInterface.get(), getSlmTotalSize()).WillRepeatedly(::testing::Return(cmdContainer->slmSize));
    cmdContainer->setDirtyStateForAllHeaps(false);

    bool requiresUncachedMocs = false;
    bool useGlobalAtomics = true;
    uint32_t partitionCount = 0;
    EncodeDispatchKernel<FamilyType>::encode(*cmdContainer.get(), dims, false, false, dispatchInterface.get(), 0, false, false,
                                             pDevice, NEO::PreemptionMode::Disabled, requiresUncachedMocs, useGlobalAtomics, partitionCount, false);

    EXPECT_FALSE(cmdContainer->lastSentUseGlobalAtomics);
    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, ptrOffset(cmdContainer->getCommandStream()->getCpuBase(), 0), cmdContainer->getCommandStream()->getUsed());

    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    auto itor = find<STATE_BASE_ADDRESS *>(commands.begin(), commands.end());
    EXPECT_EQ(commands.end(), itor);
}
