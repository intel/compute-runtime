/*
 * Copyright (C) 2021-2023 Intel Corporation
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
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

using PreambleTest = ::testing::Test;

using namespace NEO;

HWTEST2_F(PreambleTest, givenDisableEUFusionWhenProgramVFEStateThenFusedEUDispatchIsSetCorrectly, IsXeHpgCore) {
    typedef typename FamilyType::CFE_STATE CFE_STATE;

    auto bufferSize = PreambleHelper<FamilyType>::getVFECommandsSize();
    auto buffer = std::unique_ptr<char[]>(new char[bufferSize]);
    LinearStream stream(buffer.get(), bufferSize);

    auto pVfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&stream, *defaultHwInfo.get(), EngineGroupType::renderCompute);
    StreamProperties props;
    props.frontEndState.disableEUFusion.set(true);
    MockExecutionEnvironment executionEnvironment{};
    PreambleHelper<FamilyType>::programVfeState(pVfeCmd, *executionEnvironment.rootDeviceEnvironments[0], 0, 0, 0, props);

    auto cfeCmd = reinterpret_cast<CFE_STATE *>(pVfeCmd);
    EXPECT_EQ(1u, cfeCmd->getFusedEuDispatch());
}
