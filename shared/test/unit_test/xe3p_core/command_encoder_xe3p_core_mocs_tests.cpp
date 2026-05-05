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
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

struct CommandEncoderXe3pMocsSetTest : ::testing::Test {
    DebugManagerStateRestore restorer;
    static constexpr uint32_t mocsIndex = 5u;
    void SetUp() override {
        debugManager.flags.OverrideCommandLevelMocsIndex.set(static_cast<int32_t>(mocsIndex));
    }
};

using CommandEncoderXe3pMocsNotSetTest = ::testing::Test;

XE3P_CORETEST_F(CommandEncoderXe3pMocsSetTest, givenCommandLevelMocsIndexSetWhenProgrammingPipeControlThenMocsIndexIsSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    PIPE_CONTROL pipeControl = FamilyType::cmdInitPipeControl;
    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::setPipeControlExtraProperties(pipeControl, args);

    EXPECT_EQ(mocsIndex, pipeControl.getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSetTest, givenCommandLevelMocsIndexNotSetWhenProgrammingPipeControlThenMocsIndexIsNotOverridden) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    PIPE_CONTROL pipeControl = FamilyType::cmdInitPipeControl;
    PipeControlArgs args;
    MemorySynchronizationCommands<FamilyType>::setPipeControlExtraProperties(pipeControl, args);

    EXPECT_EQ(0u, pipeControl.getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSetTest, givenCommandLevelMocsIndexSetWhenProgrammingResourceBarrierThenMocsIndexIsSet) {
    using RESOURCE_BARRIER = typename FamilyType::RESOURCE_BARRIER;

    RESOURCE_BARRIER resourceBarrier{};
    PipeControlArgs args;
    args.csStallOnly = true;
    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(reinterpret_cast<void *>(&resourceBarrier), args);

    EXPECT_EQ(mocsIndex, resourceBarrier.getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSetTest, givenCommandLevelMocsIndexNotSetWhenProgrammingResourceBarrierThenMocsIndexIsNotOverridden) {
    using RESOURCE_BARRIER = typename FamilyType::RESOURCE_BARRIER;

    RESOURCE_BARRIER resourceBarrier{};
    PipeControlArgs args;
    args.csStallOnly = true;
    MemorySynchronizationCommands<FamilyType>::setSingleBarrier(reinterpret_cast<void *>(&resourceBarrier), args);

    EXPECT_EQ(0u, resourceBarrier.getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSetTest, givenCommandLevelMocsIndexSetWhenProgrammingMiSemaphoreWait64ThenMocsIndexIsSet) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    MI_SEMAPHORE_WAIT cmd = FamilyType::cmdInitMiSemaphoreWait;
    const bool native64bCmd = true;
    EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&cmd, 0x1000u, 1u,
                                                        COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                        false, true, false, false, false, native64bCmd);

    EXPECT_EQ(mocsIndex, cmd.getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSetTest, givenCommandLevelMocsIndexNotSetWhenProgrammingMiSemaphoreWait64ThenMocsIndexIsNotOverridden) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    MI_SEMAPHORE_WAIT cmd = FamilyType::cmdInitMiSemaphoreWait;
    const bool native64bCmd = true;
    EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&cmd, 0x1000u, 1u,
                                                        COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                        false, true, false, false, false, native64bCmd);

    EXPECT_EQ(0u, cmd.getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSetTest, givenCommandLevelMocsIndexSetWhenProgrammingMiSemaphoreWaitLegacyThenMocsIndexIsSet) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_SEMAPHORE_WAIT_LEGACY = typename FamilyType::MI_SEMAPHORE_WAIT_LEGACY;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    MI_SEMAPHORE_WAIT cmd = FamilyType::cmdInitMiSemaphoreWait;
    const bool native64bCmd = false;
    EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&cmd, 0x1000u, 1u,
                                                        COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                        false, true, false, false, false, native64bCmd);

    auto legacyCmd = reinterpret_cast<MI_SEMAPHORE_WAIT_LEGACY *>(&cmd);
    EXPECT_EQ(mocsIndex, legacyCmd->getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSetTest, givenCommandLevelMocsIndexNotSetWhenProgrammingMiSemaphoreWaitLegacyThenMocsIndexIsNotOverridden) {
    using MI_SEMAPHORE_WAIT = typename FamilyType::MI_SEMAPHORE_WAIT;
    using MI_SEMAPHORE_WAIT_LEGACY = typename FamilyType::MI_SEMAPHORE_WAIT_LEGACY;
    using COMPARE_OPERATION = typename MI_SEMAPHORE_WAIT::COMPARE_OPERATION;

    MI_SEMAPHORE_WAIT cmd = FamilyType::cmdInitMiSemaphoreWait;
    const bool native64bCmd = false;
    EncodeSemaphore<FamilyType>::programMiSemaphoreWait(&cmd, 0x1000u, 1u,
                                                        COMPARE_OPERATION::COMPARE_OPERATION_SAD_EQUAL_SDD,
                                                        false, true, false, false, false, native64bCmd);

    auto legacyCmd = reinterpret_cast<MI_SEMAPHORE_WAIT_LEGACY *>(&cmd);
    EXPECT_EQ(0u, legacyCmd->getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSetTest, givenCommandLevelMocsIndexSetWhenProgrammingMiAtomicThenMocsIndexIsSet) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using ATOMIC_OPCODES = typename MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename MI_ATOMIC::DATA_SIZE;

    MI_ATOMIC cmd = FamilyType::cmdInitAtomic;
    EncodeAtomic<FamilyType>::programMiAtomic(&cmd, 0x1000u,
                                              ATOMIC_OPCODES::ATOMIC_4B_MOVE,
                                              DATA_SIZE::DATA_SIZE_DWORD,
                                              0u, 0u, 0u, 0u);

    EXPECT_EQ(mocsIndex, cmd.getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSetTest, givenCommandLevelMocsIndexNotSetWhenProgrammingMiAtomicThenMocsIndexIsNotOverridden) {
    using MI_ATOMIC = typename FamilyType::MI_ATOMIC;
    using ATOMIC_OPCODES = typename MI_ATOMIC::ATOMIC_OPCODES;
    using DATA_SIZE = typename MI_ATOMIC::DATA_SIZE;

    MI_ATOMIC cmd = FamilyType::cmdInitAtomic;
    EncodeAtomic<FamilyType>::programMiAtomic(&cmd, 0x1000u,
                                              ATOMIC_OPCODES::ATOMIC_4B_MOVE,
                                              DATA_SIZE::DATA_SIZE_DWORD,
                                              0u, 0u, 0u, 0u);

    EXPECT_EQ(0u, cmd.getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSetTest, givenCommandLevelMocsIndexSetWhenProgrammingBatchBufferStartThenMocsIndexIsSet) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    MI_BATCH_BUFFER_START cmd{};
    EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferStart(&cmd, 0x1000u, false, false, false);

    EXPECT_EQ(mocsIndex, cmd.getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSetTest, givenCommandLevelMocsIndexNotSetWhenProgrammingBatchBufferStartThenMocsIndexIsNotOverridden) {
    using MI_BATCH_BUFFER_START = typename FamilyType::MI_BATCH_BUFFER_START;

    MI_BATCH_BUFFER_START cmd{};
    EncodeBatchBufferStartOrEnd<FamilyType>::programBatchBufferStart(&cmd, 0x1000u, false, false, false);

    EXPECT_EQ(0u, cmd.getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSetTest, givenCommandLevelMocsIndexSetWhenProgrammingMiFlushDwThenMocsIndexIsSet) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    MockExecutionEnvironment mockExecutionEnvironment{};
    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};

    EncodeMiFlushDW<FamilyType>::programWithWa(linearStream, 0u, 0u, args);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);

    EXPECT_EQ(mocsIndex, miFlushDwCmd->getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSetTest, givenCommandLevelMocsIndexNotSetWhenProgrammingMiFlushDwThenMocsIndexIsNotOverridden) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;

    uint8_t buffer[2 * sizeof(MI_FLUSH_DW)] = {};
    LinearStream linearStream(buffer, sizeof(buffer));
    MockExecutionEnvironment mockExecutionEnvironment{};
    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};

    EncodeMiFlushDW<FamilyType>::programWithWa(linearStream, 0u, 0u, args);
    auto miFlushDwCmd = reinterpret_cast<MI_FLUSH_DW *>(buffer);

    EXPECT_EQ(0u, miFlushDwCmd->getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSetTest, givenCommandLevelMocsIndexSetWhenProgrammingStoreMMIOThenMocsIndexIsSet) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    MI_STORE_REGISTER_MEM cmd{};
    EncodeStoreMMIO<FamilyType>::encode(&cmd, 0x200u, 0x1000u, false, false);

    EXPECT_EQ(mocsIndex, cmd.getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSetTest, givenCommandLevelMocsIndexNotSetWhenProgrammingStoreMMIOThenMocsIndexIsNotOverridden) {
    using MI_STORE_REGISTER_MEM = typename FamilyType::MI_STORE_REGISTER_MEM;

    MI_STORE_REGISTER_MEM cmd{};
    EncodeStoreMMIO<FamilyType>::encode(&cmd, 0x200u, 0x1000u, false, false);

    EXPECT_EQ(0u, cmd.getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSetTest, givenCommandLevelMocsIndexSetWhenProgrammingLoadRegisterMemThenMocsIndexIsSet) {
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    uint8_t buffer[sizeof(MI_LOAD_REGISTER_MEM)] = {};
    LinearStream cmdStream(buffer, sizeof(buffer));
    EncodeSetMMIO<FamilyType>::encodeMEM(cmdStream, 0x200u, 0x1000u, false);

    auto cmd = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(buffer);
    EXPECT_EQ(mocsIndex, cmd->getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSetTest, givenCommandLevelMocsIndexNotSetWhenProgrammingLoadRegisterMemThenMocsIndexIsNotOverridden) {
    using MI_LOAD_REGISTER_MEM = typename FamilyType::MI_LOAD_REGISTER_MEM;

    uint8_t buffer[sizeof(MI_LOAD_REGISTER_MEM)] = {};
    LinearStream cmdStream(buffer, sizeof(buffer));
    EncodeSetMMIO<FamilyType>::encodeMEM(cmdStream, 0x200u, 0x1000u, false);

    auto cmd = reinterpret_cast<MI_LOAD_REGISTER_MEM *>(buffer);
    EXPECT_EQ(0u, cmd->getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsSetTest, givenCommandLevelMocsIndexSetWhenProgrammingStoreDataImmThenMocsIndexIsSet) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MI_STORE_DATA_IMM cmd = FamilyType::cmdInitStoreDataImm;
    EncodeStoreMemory<FamilyType>::programStoreDataImm(&cmd, 0x1000u, 0xdeadu, 0u, false, false);

    EXPECT_EQ(mocsIndex, cmd.getMocsIndex());
}

XE3P_CORETEST_F(CommandEncoderXe3pMocsNotSetTest, givenCommandLevelMocsIndexNotSetWhenProgrammingStoreDataImmThenMocsIndexIsNotOverridden) {
    using MI_STORE_DATA_IMM = typename FamilyType::MI_STORE_DATA_IMM;

    MI_STORE_DATA_IMM cmd = FamilyType::cmdInitStoreDataImm;
    EncodeStoreMemory<FamilyType>::programStoreDataImm(&cmd, 0x1000u, 0xdeadu, 0u, false, false);

    EXPECT_EQ(0u, cmd.getMocsIndex());
}
