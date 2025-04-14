/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/preemption.h"
#include "shared/source/gen12lp/hw_cmds_base.h"
#include "shared/source/gen12lp/hw_info_gen12lp.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/fixtures/preemption_fixture.h"

using namespace NEO;

template <>
PreemptionTestHwDetails getPreemptionTestHwDetails<Gen12LpFamily>() {
    PreemptionTestHwDetails ret;
    ret.modeToRegValueMap[PreemptionMode::ThreadGroup] = DwordBuilder::build(1, true) | DwordBuilder::build(2, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidBatch] = DwordBuilder::build(2, true) | DwordBuilder::build(1, true, false);
    ret.modeToRegValueMap[PreemptionMode::MidThread] = DwordBuilder::build(2, true, false) | DwordBuilder::build(1, true, false);
    ret.defaultRegValue = ret.modeToRegValueMap[PreemptionMode::MidBatch];
    ret.regAddress = 0x2580u;
    return ret;
}

using Gen12LpPreemptionTests = DevicePreemptionTests;

GEN12LPTEST_F(Gen12LpPreemptionTests, WhenProgrammingPreemptionThenExpectLoarRegisterCommandRemapFlagEnabled) {
    using MI_LOAD_REGISTER_IMM = typename FamilyType::MI_LOAD_REGISTER_IMM;

    const size_t bufferSize = 128;
    uint8_t buffer[bufferSize];
    LinearStream cmdStream(buffer, bufferSize);

    PreemptionHelper::programCmdStream<FamilyType>(cmdStream, PreemptionMode::ThreadGroup, PreemptionMode::Initial, nullptr);
    auto lriCommand = genCmdCast<MI_LOAD_REGISTER_IMM *>(cmdStream.getCpuBase());
    ASSERT_NE(nullptr, lriCommand);
    EXPECT_TRUE(lriCommand->getMmioRemapEnable());
}
