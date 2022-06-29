/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gen_common/reg_configs_common.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/utilities/stackvec.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/test_macros/hw_test.h"

using PreambleTest = ::testing::Test;

using namespace NEO;

HWTEST2_F(PreambleTest, whenKernelDebuggingCommandsAreProgrammedThenCorrectCommandsArePlacedIntoStream, IsXeHpgCore) {
    typedef typename FamilyType::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;

    auto bufferSize = PreambleHelper<FamilyType>::getKernelDebuggingCommandsSize(true);
    auto buffer = std::unique_ptr<char[]>(new char[bufferSize]);

    LinearStream stream(buffer.get(), bufferSize);
    PreambleHelper<FamilyType>::programKernelDebugging(&stream);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream);
    auto cmdList = hwParser.getCommandsList<MI_LOAD_REGISTER_IMM>();

    ASSERT_EQ(2u, cmdList.size());

    auto it = cmdList.begin();

    MI_LOAD_REGISTER_IMM *pCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*it);
    EXPECT_EQ(0x20d8u, pCmd->getRegisterOffset());
    EXPECT_EQ((1u << 5) | (1u << 21), pCmd->getDataDword());
    it++;

    pCmd = reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*it);
    EXPECT_EQ(0xe400u, pCmd->getRegisterOffset());
    EXPECT_EQ((1u << 7) | (1u << 4) | (1u << 2) | (1u << 0), pCmd->getDataDword());
}

HWTEST2_F(PreambleTest, givenDisableEUFusionWhenProgramVFEStateThenFusedEUDispatchIsSetCorrectly, IsXeHpgCore) {
    typedef typename FamilyType::CFE_STATE CFE_STATE;

    auto bufferSize = PreambleHelper<FamilyType>::getVFECommandsSize();
    auto buffer = std::unique_ptr<char[]>(new char[bufferSize]);
    LinearStream stream(buffer.get(), bufferSize);

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&stream, *defaultHwInfo.get(), EngineGroupType::RenderCompute);
    StreamProperties props;
    props.frontEndState.disableEUFusion.set(true);
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *defaultHwInfo.get(), 0, 0, 0, props, nullptr);

    auto cfeCmd = reinterpret_cast<CFE_STATE *>(pVfeCmd);
    EXPECT_EQ(1u, cfeCmd->getFusedEuDispatch());
}
