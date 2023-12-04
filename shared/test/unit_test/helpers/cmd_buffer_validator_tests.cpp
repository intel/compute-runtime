/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/cmd_buffer_validator.h"
#include "shared/test/common/test_macros/hw_test.h"

using HwParseTest = ::testing::Test;

using namespace NEO;

HWTEST_F(HwParseTest, WhenEmptyBufferThenDontExpectCommands) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    bool cmdBuffOk = false;

    GenCmdList cmdList;

    auto beg = cmdList.begin();
    auto end = cmdList.end();

    cmdBuffOk = expectCmdBuff<FamilyType>(beg, end,
                                          std::vector<MatchCmd *>{});
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(beg, end,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(0),
                                          });
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(beg, end,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(anyNumber),
                                          });
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(beg, end,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(atLeastOne),
                                          });
    EXPECT_FALSE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(beg, end,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1),
                                          });
    EXPECT_FALSE(cmdBuffOk);
}

HWTEST_F(HwParseTest, WhenExpectingAnyCommandThenAllCommandsAreValidAsLongAsTheCountMatches) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    bool cmdBuffOk = false;
    char buffer[8192];
    LinearStream stream{alignUp(buffer, 4096), 4096};
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;
    *stream.getSpaceForCmd<STATE_BASE_ADDRESS>() = FamilyType::cmdInitStateBaseAddress;
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(1),
                                              new MatchAnyCmd(1),
                                              new MatchAnyCmd(1)});
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(atLeastOne),
                                          });
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(anyNumber),
                                          });
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(1),
                                              new MatchAnyCmd(1),
                                              new MatchAnyCmd(1),
                                              new MatchAnyCmd(1)});
    EXPECT_FALSE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(1),
                                              new MatchAnyCmd(1),
                                              new MatchAnyCmd(1),
                                              new MatchAnyCmd(atLeastOne)});
    EXPECT_FALSE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(1),
                                              new MatchAnyCmd(1),
                                              new MatchAnyCmd(1),
                                              new MatchAnyCmd(anyNumber)});
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(atLeastOne),
                                              new MatchAnyCmd(1)});
    EXPECT_FALSE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(anyNumber),
                                              new MatchAnyCmd(1)});
    EXPECT_FALSE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(1),
                                              new MatchAnyCmd(1),
                                          });
    EXPECT_FALSE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(1)});
    EXPECT_FALSE(cmdBuffOk);
}

HWTEST_F(HwParseTest, WhenExpectingSpecificSetOfCommandsThenNoOtherCommandBufferIsValid) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    bool cmdBuffOk = false;
    char buffer[8192];
    LinearStream stream{alignUp(buffer, 4096), 4096};
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;
    *stream.getSpaceForCmd<STATE_BASE_ADDRESS>() = FamilyType::cmdInitStateBaseAddress;
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1)});
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1)});
    EXPECT_FALSE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1),
                                          });
    EXPECT_FALSE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1)});
    EXPECT_FALSE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1)});
    EXPECT_FALSE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1)});
    EXPECT_FALSE(cmdBuffOk);
}

HWTEST_F(HwParseTest, WhenExpectingAnyNumberOfCommandsThenOnlyTypeOfCommandMatters) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    bool cmdBuffOk = false;
    char buffer[8192];
    LinearStream stream{alignUp(buffer, 4096), 4096};
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;
    *stream.getSpaceForCmd<STATE_BASE_ADDRESS>() = FamilyType::cmdInitStateBaseAddress;
    *stream.getSpaceForCmd<STATE_BASE_ADDRESS>() = FamilyType::cmdInitStateBaseAddress;
    *stream.getSpaceForCmd<STATE_BASE_ADDRESS>() = FamilyType::cmdInitStateBaseAddress;
    *stream.getSpaceForCmd<STATE_BASE_ADDRESS>() = FamilyType::cmdInitStateBaseAddress;
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1)});
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1)});
    EXPECT_FALSE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(anyNumber),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1)});
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(anyNumber),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(anyNumber),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(anyNumber)});
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(atLeastOne),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(atLeastOne),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(atLeastOne)});
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(atLeastOne),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(atLeastOne),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(atLeastOne),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(atLeastOne),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(atLeastOne),
                                          });
    EXPECT_FALSE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(anyNumber),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(0),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(anyNumber),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(anyNumber),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(0)});
    EXPECT_TRUE(cmdBuffOk);
}

HWTEST_F(HwParseTest, WhenCommandMemberValidatorFailsThenCommandBufferValidationFails) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
    bool cmdBuffOk = false;
    char buffer[8192];
    LinearStream stream{alignUp(buffer, 4096), 4096};
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;
    auto sba = stream.getSpaceForCmd<STATE_BASE_ADDRESS>();
    *sba = FamilyType::cmdInitStateBaseAddress;
    sba->setGeneralStateBaseAddressModifyEnable(true);
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(anyNumber),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(anyNumber, Expects{EXPECT_MEMBER(STATE_BASE_ADDRESS, getGeneralStateBaseAddressModifyEnable, true)}),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(anyNumber)});
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(anyNumber),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(anyNumber, Expects{EXPECT_MEMBER(STATE_BASE_ADDRESS, getGeneralStateBaseAddressModifyEnable, false)}),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(anyNumber)});
    EXPECT_FALSE(cmdBuffOk);
}
