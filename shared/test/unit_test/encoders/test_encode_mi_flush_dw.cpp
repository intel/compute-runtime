/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/test/common/cmd_parse/gen_cmd_parse.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using EncodeMiFlushDWTest = testing::Test;

HWTEST_F(EncodeMiFlushDWTest, GivenLinearStreamWhenCllaedEncodeWithNoPostSyncThenPostSyncNotWriteIsSet) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint32_t pCmdBuffer[1024];
    MockGraphicsAllocation gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    LinearStream stream(&gfxAllocation);

    MiFlushArgs args;
    EncodeMiFlushDW<FamilyType>::programMiFlushDw(stream, 0, 0, args, *defaultHwInfo);

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

    MiFlushArgs args;
    args.commandWithPostSync = true;
    EncodeMiFlushDW<FamilyType>::programMiFlushDw(stream, address, data, args, *defaultHwInfo);

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

    MiFlushArgs args;
    args.commandWithPostSync = true;
    EncodeMiFlushDW<FamilyType>::programMiFlushDw(stream, address, data, args, *defaultHwInfo);

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

    MiFlushArgs args;
    args.timeStampOperation = true;
    args.commandWithPostSync = true;
    EncodeMiFlushDW<FamilyType>::programMiFlushDw(stream, address, data, args, *defaultHwInfo);

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
