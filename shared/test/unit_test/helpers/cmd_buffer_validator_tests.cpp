/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/state_base_address_helper.h"
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
    bool cmdBuffOk = false;
    char buffer[8192];
    LinearStream stream{alignUp(buffer, 4096), 4096};
    std::vector<MatchCmd *> expectedCmdBuf;
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;
    expectedCmdBuf.push_back(new MatchAnyCmd(1));
    if constexpr (GfxFamilyWithSBA<FamilyType>) {
        using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
        *stream.getSpaceForCmd<STATE_BASE_ADDRESS>() = FamilyType::cmdInitStateBaseAddress;
        expectedCmdBuf.push_back(new MatchAnyCmd(1));
    }
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;
    expectedCmdBuf.push_back(new MatchAnyCmd(1));

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0, std::move(expectedCmdBuf));
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

    if constexpr (GfxFamilyWithSBA<FamilyType>) {
        cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                              std::vector<MatchCmd *>{
                                                  new MatchAnyCmd(1),
                                                  new MatchAnyCmd(1),
                                                  new MatchAnyCmd(1),
                                                  new MatchAnyCmd(anyNumber)});
        EXPECT_TRUE(cmdBuffOk);
    }

    if constexpr (GfxFamilyWithSBA<FamilyType>) {
        cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                              std::vector<MatchCmd *>{
                                                  new MatchAnyCmd(atLeastOne),
                                                  new MatchAnyCmd(1)});
        EXPECT_FALSE(cmdBuffOk);
    }

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(anyNumber),
                                              new MatchAnyCmd(1)});
    EXPECT_FALSE(cmdBuffOk);

    if constexpr (GfxFamilyWithSBA<FamilyType>) {
        cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                              std::vector<MatchCmd *>{
                                                  new MatchAnyCmd(1),
                                                  new MatchAnyCmd(1),
                                              });
        EXPECT_FALSE(cmdBuffOk);
    }

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                          std::vector<MatchCmd *>{
                                              new MatchAnyCmd(1)});
    EXPECT_FALSE(cmdBuffOk);
}

HWTEST_F(HwParseTest, WhenExpectingSpecificSetOfCommandsThenNoOtherCommandBufferIsValid) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    bool cmdBuffOk = false;
    char buffer[8192];
    LinearStream stream{alignUp(buffer, 4096), 4096};
    std::vector<MatchCmd *> expectedCmdBuf;
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;
    expectedCmdBuf.push_back(new MatchHwCmd<FamilyType, PIPE_CONTROL>(1));
    if constexpr (GfxFamilyWithSBA<FamilyType>) {
        using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
        *stream.getSpaceForCmd<STATE_BASE_ADDRESS>() = FamilyType::cmdInitStateBaseAddress;
        expectedCmdBuf.push_back(new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1));
    }
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;
    expectedCmdBuf.push_back(new MatchHwCmd<FamilyType, PIPE_CONTROL>(1));

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0, std::move(expectedCmdBuf));
    EXPECT_TRUE(cmdBuffOk);

    if constexpr (GfxFamilyWithSBA<FamilyType>) {
        using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
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
}

HWTEST_F(HwParseTest, WhenExpectingAnyNumberOfCommandsThenOnlyTypeOfCommandMatters) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    bool cmdBuffOk = false;
    char buffer[8192];
    LinearStream stream{alignUp(buffer, 4096), 4096};
    std::vector<MatchCmd *> expectedCmdBuf;
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;
    expectedCmdBuf.push_back(new MatchHwCmd<FamilyType, PIPE_CONTROL>(1));
    if constexpr (GfxFamilyWithSBA<FamilyType>) {
        using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
        *stream.getSpaceForCmd<STATE_BASE_ADDRESS>() = FamilyType::cmdInitStateBaseAddress;
        expectedCmdBuf.push_back(new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1));
        *stream.getSpaceForCmd<STATE_BASE_ADDRESS>() = FamilyType::cmdInitStateBaseAddress;
        expectedCmdBuf.push_back(new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1));
        *stream.getSpaceForCmd<STATE_BASE_ADDRESS>() = FamilyType::cmdInitStateBaseAddress;
        expectedCmdBuf.push_back(new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1));
        *stream.getSpaceForCmd<STATE_BASE_ADDRESS>() = FamilyType::cmdInitStateBaseAddress;
        expectedCmdBuf.push_back(new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1));
    }
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;
    expectedCmdBuf.push_back(new MatchHwCmd<FamilyType, PIPE_CONTROL>(1));

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0, std::move(expectedCmdBuf));
    EXPECT_TRUE(cmdBuffOk);

    if constexpr (GfxFamilyWithSBA<FamilyType>) {
        using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
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
}

HWTEST_F(HwParseTest, WhenCommandMemberValidatorFailsThenCommandBufferValidationFails) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    bool cmdBuffOk = false;
    char buffer[8192];
    LinearStream stream{alignUp(buffer, 4096), 4096};
    std::vector<MatchCmd *> expectedCmdBuf;
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;
    expectedCmdBuf.push_back(new MatchHwCmd<FamilyType, PIPE_CONTROL>(1));
    if constexpr (GfxFamilyWithSBA<FamilyType>) {
        using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
        auto sba = stream.getSpaceForCmd<STATE_BASE_ADDRESS>();
        *sba = FamilyType::cmdInitStateBaseAddress;
        sba->setGeneralStateBaseAddressModifyEnable(true);
        expectedCmdBuf.push_back(new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(1, Expects{EXPECT_MEMBER(STATE_BASE_ADDRESS, getGeneralStateBaseAddressModifyEnable, true)}));
    }
    *stream.getSpaceForCmd<PIPE_CONTROL>() = FamilyType::cmdInitPipeControl;
    expectedCmdBuf.push_back(new MatchHwCmd<FamilyType, PIPE_CONTROL>(1));

    cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0, std::move(expectedCmdBuf));
    EXPECT_TRUE(cmdBuffOk);

    if constexpr (GfxFamilyWithSBA<FamilyType>) {
        using STATE_BASE_ADDRESS = typename FamilyType::STATE_BASE_ADDRESS;
        cmdBuffOk = expectCmdBuff<FamilyType>(stream, 0,
                                              std::vector<MatchCmd *>{
                                                  new MatchHwCmd<FamilyType, PIPE_CONTROL>(anyNumber),
                                                  new MatchHwCmd<FamilyType, STATE_BASE_ADDRESS>(anyNumber, Expects{EXPECT_MEMBER(STATE_BASE_ADDRESS, getGeneralStateBaseAddressModifyEnable, false)}),
                                                  new MatchHwCmd<FamilyType, PIPE_CONTROL>(anyNumber)});
        EXPECT_FALSE(cmdBuffOk);
    }
}
