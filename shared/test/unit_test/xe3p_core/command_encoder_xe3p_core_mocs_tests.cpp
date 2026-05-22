/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/pipe_control_args.h"
#include "shared/source/xe3p_core/hw_cmds_base.h"
#include "shared/source/xe3p_core/hw_info_xe3p_core.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/variable_backup.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

#include "gtest/gtest.h"

using namespace NEO;

namespace {

template <typename FamilyType>
uint32_t getPipeControlMocsIndex() {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    PIPE_CONTROL pipeControl = FamilyType::cmdInitPipeControl;
    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::setPipeControlExtraProperties(pipeControl, args);

    return pipeControl.getMocsIndex();
}

template <typename FamilyType>
uint32_t getResourceBarrierMocsIndex() {
    using RESOURCE_BARRIER = typename FamilyType::RESOURCE_BARRIER;

    RESOURCE_BARRIER resourceBarrier{};
    PipeControlArgs args;
    args.csStallOnly = true;
    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(reinterpret_cast<void *>(&resourceBarrier), args);

    return resourceBarrier.getMocsIndex();
}

template <typename FamilyType>
uint32_t getMiSemaphoreWait64MocsIndex() {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    MI_SEMAPHORE_WAIT cmd = FamilyType::cmdInitMiSemaphoreWait;
    constexpr bool native64bCmd = true;
    EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&cmd, 0x1000u, 1u,
                                                        COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                        false, true, false, false, false, native64bCmd);

    return cmd.getMocsIndex();
}

template <typename FamilyType>
uint32_t getMiSemaphoreWaitLegacyMocsIndex() {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_SEMAPHORE_WAIT_LEGACY = typename FamilyType::MI_SEMAPHORE_WAIT_LEGACY;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    MI_SEMAPHORE_WAIT cmd = FamilyType::cmdInitMiSemaphoreWait;
    constexpr bool native64bCmd = false;
    EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&cmd, 0x1000u, 1u,
                                                        COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                        false, true, false, false, false, native64bCmd);

    auto legacyCmd = reinterpret_cast<MI_SEMAPHORE_WAIT_LEGACY *>(&cmd);
    return legacyCmd->getMocsIndex();
}

template <typename FamilyType>
uint32_t getMiAtomicMocsIndex() {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using ATOMIC_OPCODES = typename MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename MI_ATOMIC::DATA_SIZE;

    MI_ATOMIC cmd = FamilyType::cmdInitAtomic;
    EncodeAtomic<FamilyType>::programMiAtomic(&cmd, 0x1000u,
                                              ATOMIC_OPCODES::ATOMIC_4B_MOVE,
                                              DATA_SIZE::DATA_SIZE_DWORD,
                                              0u, 0u, 0u, 0u);

    return cmd.getMocsIndex();
}

template <typename FamilyType>
uint32_t getBatchBufferStartMocsIndex() {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    MI_BATCH_BUFFER_START cmd{};
    EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferStart(&cmd, 0x1000u, false, false, false);

    return cmd.getMocsIndex();
}

template <typename FamilyType>
uint32_t getMiFlushDwMocsIndex() {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    MockExecutionEnvironment mockExecutionEnvironment{};
    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};

    EncodeMiFlushDW<FamilyType>::programWithWa(linearStream, 0u, 0u, args);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);

    return miFlushDwCmd->getMocsIndex();
}

template <typename FamilyType>
uint32_t getStoreMmioMocsIndex() {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    MI_STORE_REGISTER_MEM cmd{};
    EncodeStoreMMIO<FamilyType>::encode(&cmd, 0x200u, 0x1000u, false, false);

    return cmd.getMocsIndex();
}

template <typename FamilyType>
uint32_t getLoadRegisterMemMocsIndex() {
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    uint8_t buffer[sizeof(MI_LOAD_REGISTER_MEM)] = {};
    LinearStream cmdStream(buffer, sizeof(buffer));
    EncodeSetMMIO<FamilyType>::encodeMEM(cmdStream, 0x200u, 0x1000u, false);

    auto cmd = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(buffer);
    return cmd->getMocsIndex();
}

template <typename FamilyType>
uint32_t getStoreDataImmMocsIndex() {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MI_STORE_DATA_IMM cmd = FamilyType::cmdInitStoreDataImm;
    EncodeStoreMemory<FamilyType>::programStoreDataImm(&cmd, 0x1000u, 0xdeadu, 0u, false, false);

    return cmd.getMocsIndex();
}

} // namespace

struct CommandEncoderXe3pMocsSupportedAndDefaultMocsModifiedTest : ::testing::Test {
    static constexpr uint32_t expectedMocsIndex = 5u;
    void SetUp() override {
        EncodeCommandLevelMocs<Xe3pCoreFamily>::defaultMocs = expectedMocsIndex;
        EncodeCommandLevelMocs<Xe3pCoreFamily>::cmdMocsSupported = true;
    }

    VariableBackup<uint32_t> defaultMocsBackup{&EncodeCommandLevelMocs<Xe3pCoreFamily>::defaultMocs};
    VariableBackup<bool> cmdMocsSupportedBackup{&EncodeCommandLevelMocs<Xe3pCoreFamily>::cmdMocsSupported};
};

struct CommandEncoderXe3pMocsNotSupportedTest : ::testing::Test {
    static constexpr uint32_t expectedMocsIndex = 0u;
    static constexpr uint32_t notExpectedMocsIndex = 10u;

    void SetUp() override {
        EncodeCommandLevelMocs<Xe3pCoreFamily>::defaultMocs = notExpectedMocsIndex;
        EncodeCommandLevelMocs<Xe3pCoreFamily>::cmdMocsSupported = false;
    }

    VariableBackup<uint32_t> defaultMocsBackup{&EncodeCommandLevelMocs<Xe3pCoreFamily>::defaultMocs};
    VariableBackup<bool> cmdMocsSupportedBackup{&EncodeCommandLevelMocs<Xe3pCoreFamily>::cmdMocsSupported};
};

struct CommandEncoderXe3pMocsOverrideTest : ::testing::Test {
    DebugManagerStateRestore restorer;
    static constexpr uint32_t expectedMocsIndex = 5u;
    void SetUp() override {
        debugManager.flags.OverrideCommandLevelMocsIndex.set(static_cast<int32_t>(expectedMocsIndex));
    }
};

XE3P_CORETEST_F(CommandEncoderXe3pMocsSupportedAndDefaultMocsModifiedTest, givenCommandLevelMocsDefaultWhenProgrammingPipeControlThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getPipeControlMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSupportedTest, givenCommandLevelMocsIndexNotSetWhenProgrammingPipeControlThenMocsIndexIsNotOverridden) {
    EXPECT_EQ(expectedMocsIndex, getPipeControlMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsOverrideTest, givenCommandLevelMocsIndexSetWhenProgrammingPipeControlThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getPipeControlMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSupportedAndDefaultMocsModifiedTest, givenCommandLevelMocsDefaultWhenProgrammingResourceBarrierThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getResourceBarrierMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSupportedTest, givenCommandLevelMocsIndexNotSetWhenProgrammingResourceBarrierThenMocsIndexIsNotOverridden) {
    EXPECT_EQ(expectedMocsIndex, getResourceBarrierMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsOverrideTest, givenCommandLevelMocsIndexSetWhenProgrammingResourceBarrierThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getResourceBarrierMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSupportedAndDefaultMocsModifiedTest, givenCommandLevelMocsDefaultWhenProgrammingMiSemaphoreWait64ThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getMiSemaphoreWait64MocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSupportedTest, givenCommandLevelMocsIndexNotSetWhenProgrammingMiSemaphoreWait64ThenMocsIndexIsNotOverridden) {
    EXPECT_EQ(expectedMocsIndex, getMiSemaphoreWait64MocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsOverrideTest, givenCommandLevelMocsIndexSetWhenProgrammingMiSemaphoreWait64ThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getMiSemaphoreWait64MocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSupportedAndDefaultMocsModifiedTest, givenCommandLevelMocsDefaultWhenProgrammingMiSemaphoreWaitLegacyThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getMiSemaphoreWaitLegacyMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSupportedTest, givenCommandLevelMocsIndexNotSetWhenProgrammingMiSemaphoreWaitLegacyThenMocsIndexIsNotOverridden) {
    EXPECT_EQ(expectedMocsIndex, getMiSemaphoreWaitLegacyMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsOverrideTest, givenCommandLevelMocsIndexSetWhenProgrammingMiSemaphoreWaitLegacyThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getMiSemaphoreWaitLegacyMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSupportedAndDefaultMocsModifiedTest, givenCommandLevelMocsDefaultWhenProgrammingMiAtomicThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getMiAtomicMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSupportedTest, givenCommandLevelMocsIndexNotSetWhenProgrammingMiAtomicThenMocsIndexIsNotOverridden) {
    EXPECT_EQ(expectedMocsIndex, getMiAtomicMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsOverrideTest, givenCommandLevelMocsIndexSetWhenProgrammingMiAtomicThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getMiAtomicMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSupportedAndDefaultMocsModifiedTest, givenCommandLevelMocsDefaultWhenProgrammingBatchBufferStartThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getBatchBufferStartMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSupportedTest, givenCommandLevelMocsIndexNotSetWhenProgrammingBatchBufferStartThenMocsIndexIsNotOverridden) {
    EXPECT_EQ(expectedMocsIndex, getBatchBufferStartMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsOverrideTest, givenCommandLevelMocsIndexSetWhenProgrammingBatchBufferStartThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getBatchBufferStartMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSupportedAndDefaultMocsModifiedTest, givenCommandLevelMocsDefaultWhenProgrammingMiFlushDwThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getMiFlushDwMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSupportedTest, givenCommandLevelMocsIndexNotSetWhenProgrammingMiFlushDwThenMocsIndexIsNotOverridden) {
    EXPECT_EQ(expectedMocsIndex, getMiFlushDwMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsOverrideTest, givenCommandLevelMocsIndexSetWhenProgrammingMiFlushDwThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getMiFlushDwMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSupportedAndDefaultMocsModifiedTest, givenCommandLevelMocsDefaultWhenProgrammingStoreMMIOThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getStoreMmioMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSupportedTest, givenCommandLevelMocsIndexNotSetWhenProgrammingStoreMMIOThenMocsIndexIsNotOverridden) {
    EXPECT_EQ(expectedMocsIndex, getStoreMmioMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsOverrideTest, givenCommandLevelMocsIndexSetWhenProgrammingStoreMMIOThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getStoreMmioMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSupportedAndDefaultMocsModifiedTest, givenCommandLevelMocsDefaultWhenProgrammingLoadRegisterMemThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getLoadRegisterMemMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSupportedTest, givenCommandLevelMocsIndexNotSetWhenProgrammingLoadRegisterMemThenMocsIndexIsNotOverridden) {
    EXPECT_EQ(expectedMocsIndex, getLoadRegisterMemMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsOverrideTest, givenCommandLevelMocsIndexSetWhenProgrammingLoadRegisterMemThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getLoadRegisterMemMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSupportedAndDefaultMocsModifiedTest, givenCommandLevelMocsDefaultWhenProgrammingStoreDataImmThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getStoreDataImmMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSupportedTest, givenCommandLevelMocsIndexNotSetWhenProgrammingStoreDataImmThenMocsIndexIsNotOverridden) {
    EXPECT_EQ(expectedMocsIndex, getStoreDataImmMocsIndex<FamilyType>());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsOverrideTest, givenCommandLevelMocsIndexSetWhenProgrammingStoreDataImmThenMocsIndexIsSet) {
    EXPECT_EQ(expectedMocsIndex, getStoreDataImmMocsIndex<FamilyType>());
}
