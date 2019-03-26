/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "test.h"
#include "unit_tests/helpers/cmd_buffer_validator.h"

#include "hw_parse.h"

using HwParseTest = ::testing::Test;

using namespace NEO;

HWTEST_F(HwParseTest, WhenEmptyBufferThenDontExpectCommands) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    bool cmdBuffOk = false;

    GenCmdList::iterator beg, end;
    end = beg;
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
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AnyNumber),
                                          });
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(beg, end,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AtLeastOne),
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
                                              new MatchAnyCmd(AtLeastOne),
                                          });
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(AnyNumber),
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
                                              new MatchAnyCmd(AtLeastOne)});
    EXPECT_FALSE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(1),
                                              new MatchAnyCmd(1),
                                              new MatchAnyCmd(1),
                                              new MatchAnyCmd(AnyNumber)});
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(AtLeastOne),
                                              new MatchAnyCmd(1)});
    EXPECT_FALSE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(AnyNumber),
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
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(AnyNumber),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(1)});
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AnyNumber),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(AnyNumber),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AnyNumber)});
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AtLeastOne),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(AtLeastOne),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AtLeastOne)});
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AtLeastOne),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AtLeastOne),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AtLeastOne),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(AtLeastOne),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AtLeastOne),
                                          });
    EXPECT_FALSE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AnyNumber),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(0),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(AnyNumber),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AnyNumber),
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
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AnyNumber),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(AnyNumber, Expects{EXPECT_MEMBER(STATE_BASE_ADDRESS, getGeneralStateBaseAddressModifyEnable, true)}),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AnyNumber)});
    EXPECT_TRUE(cmdBuffOk);

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AnyNumber),
                                              new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(AnyNumber, Expects{EXPECT_MEMBER(STATE_BASE_ADDRESS, getGeneralStateBaseAddressModifyEnable, false)}),
                                              new MatchHwCmd<FamilyType, PIPE_CONTROL>(AnyNumber)});
    EXPECT_FALSE(cmdBuffOk);
}
