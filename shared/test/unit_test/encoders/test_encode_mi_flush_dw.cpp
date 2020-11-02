/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/helpers/blit_commands_helper.h"
#include "shared/test/unit_test/cmd_parse/gen_cmd_parse.h"
#include "shared/test/unit_test/mocks/mock_graphics_allocation.h"

#include "test.h"

using namespace NEO;

using EncodeMiFlushDWTest = testing::Test;

HWTEST_F(EncodeMiFlushDWTest, GivenLinearStreamWhenCllaedEncodeWithNoPostSyncThenPostSyncNotWriteIsSet) {
    using MI_FLUSH_DW = typename FamilyType::MI_FLUSH_DW;
    uint32_t pCmdBuffer[1024];
    MockGraphicsAllocation gfxAllocation(static_cast<void *>(pCmdBuffer), sizeof(pCmdBuffer));
    LinearStream stream(&gfxAllocation);

    EncodeMiFlushDW<FamilyType>::programMiFlushDw(stream, 0, 0, false, false);

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

    EncodeMiFlushDW<FamilyType>::programMiFlushDw(stream, address, data, false, true);

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

    EncodeMiFlushDW<FamilyType>::programMiFlushDw(stream, address, data, false, true);

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

    EncodeMiFlushDW<FamilyType>::programMiFlushDw(stream, address, data, true, true);

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