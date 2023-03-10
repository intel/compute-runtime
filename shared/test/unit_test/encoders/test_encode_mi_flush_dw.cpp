/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/source/helpers/definitions/command_encoder_args.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/hw_test.h"

using namespace NEO;

using EncodeMiFlushDWTest = testing::Test;

HWTEST_F(EncodeMiFlushDWTest, GivenLinearStreamWhenCllaedEncodeWithNoPostSyncThenPostSyncNotWriteIsSet) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint32_t pCmdBuffer[1024];
    MockGraphicsAllocation gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    LinearStream stream(&gfxAllocation);
    MockExecutionEnvironment mockExecutionEnvironment{};
    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};

    EncodeMiFlushDW<FamilyType>::programWithWa(stream, 0, 0, args);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, stream.getCpuBase(), stream.getUsed());

    auto itor = commands.begin();
    itor = find<MI_FLUSH_DW *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());
    auto cmd = genCmdCast<MI_FLUSH_DW *>(*itor);
    EXPECT_EQ(cmd->getPostSyncOperation(), MI_FLUSH_DW::POST_SYNC_OPERATION_NO_WRITE);
}

HWTEST_F(EncodeMiFlushDWTest, GivenLinearStreamWhenCllaedEncodeWithPostSyncDataThenPostSyncDataIsSet) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint32_t pCmdBuffer[1024];
    MockGraphicsAllocation gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    LinearStream stream(&gfxAllocation);

    uint64_t address = 0x1000;
    uint64_t data = 0x4321;
    MockExecutionEnvironment mockExecutionEnvironment{};
    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;
    EncodeMiFlushDW<FamilyType>::programWithWa(stream, address, data, args);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, stream.getCpuBase(), stream.getUsed());

    auto itor = commands.begin();
    itor = find<MI_FLUSH_DW *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());
    bool miFlushWithPostSyncFound = false;
    for (; itor != commands.end(); itor++) {
        auto cmd = genCmdCast<MI_FLUSH_DW *>(*itor);
        if (cmd->getPostSyncOperation() != MI_FLUSH_DW::POST_SYNC_OPERATION_NO_WRITE) {
            EXPECT_EQ(cmd->getDestinationAddress(), address);
            EXPECT_EQ(cmd->getImmediateData(), data);
            miFlushWithPostSyncFound = true;
            break;
        }
    }
    EXPECT_TRUE(miFlushWithPostSyncFound);
}

HWTEST_F(EncodeMiFlushDWTest, GivenLinearStreamWhenCllaedEncodeWithTimestampFaslseThenPostSyncDataTypeIsSet) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint32_t pCmdBuffer[1024];
    MockGraphicsAllocation gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    LinearStream stream(&gfxAllocation);

    uint64_t address = 0x1000;
    uint64_t data = 0x4321;
    MockExecutionEnvironment mockExecutionEnvironment{};
    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};
    args.commandWithPostSync = true;

    EncodeMiFlushDW<FamilyType>::programWithWa(stream, address, data, args);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, stream.getCpuBase(), stream.getUsed());

    auto itor = commands.begin();
    itor = find<MI_FLUSH_DW *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());
    bool miFlushWithPostSyncFound = false;
    for (; itor != commands.end(); itor++) {
        auto cmd = genCmdCast<MI_FLUSH_DW *>(*itor);
        if (cmd->getPostSyncOperation() != MI_FLUSH_DW::POST_SYNC_OPERATION_NO_WRITE) {
            EXPECT_EQ(cmd->getPostSyncOperation(), MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_IMMEDIATE_DATA_QWORD);
            miFlushWithPostSyncFound = true;
            break;
        }
    }
    EXPECT_TRUE(miFlushWithPostSyncFound);
}

HWTEST_F(EncodeMiFlushDWTest, GivenLinearStreamWhenCllaedEncodeWithTimestampTrueThenPostSyncDataTypeIsSet) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint32_t pCmdBuffer[1024];
    MockGraphicsAllocation gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    LinearStream stream(&gfxAllocation);

    uint64_t address = 0x1000;
    uint64_t data = 0x4321;
    MockExecutionEnvironment mockExecutionEnvironment{};
    NEO::EncodeDummyBlitWaArgs waArgs{false, mockExecutionEnvironment.rootDeviceEnvironments[0].get()};
    MiFlushArgs args{waArgs};
    args.timeStampOperation = true;
    args.commandWithPostSync = true;

    EncodeMiFlushDW<FamilyType>::programWithWa(stream, address, data, args);

    GenCmdList commands;
    CmdParse<FamilyType>::parseCommandBuffer(commands, stream.getCpuBase(), stream.getUsed());

    auto itor = commands.begin();
    itor = find<MI_FLUSH_DW *>(itor, commands.end());
    ASSERT_NE(itor, commands.end());
    bool miFlushWithPostSyncFound = false;
    for (; itor != commands.end(); itor++) {
        auto cmd = genCmdCast<MI_FLUSH_DW *>(*itor);
        if (cmd->getPostSyncOperation() != MI_FLUSH_DW::POST_SYNC_OPERATION_NO_WRITE) {
            EXPECT_EQ(cmd->getPostSyncOperation(), MI_FLUSH_DW::POST_SYNC_OPERATION_WRITE_TIMESTAMP_REGISTER);
            miFlushWithPostSyncFound = true;
            break;
        }
    }
    EXPECT_TRUE(miFlushWithPostSyncFound);
}
