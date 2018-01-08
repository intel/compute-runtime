/*
 * Copyright (c) 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "runtime/command_stream/thread_arbitration_policy.h"
#include "runtime/helpers/options.h"
#include "runtime/helpers/preamble.h"
#include "runtime/gen8/reg_configs.h"
#include "unit_tests/preamble/preamble_fixture.h"
#include "unit_tests/fixtures/platform_fixture.h"

using namespace OCLRT;

typedef PreambleFixture BdwSlm;

BDWTEST_F(BdwSlm, shouldBeEnabledOnGen8) {
    typedef BDWFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<BDWFamily>::getL3Config(**platformDevices, true);
    PreambleHelper<BDWFamily>::programL3(&cs, l3Config);

    parseCommands<BDWFamily>(cs);

    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorLRI);

    const auto &lri = *reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    auto RegisterOffset = L3CNTLRegisterOffset<BDWFamily>::registerOffset;
    EXPECT_EQ(RegisterOffset, lri.getRegisterOffset());
    EXPECT_EQ(1u, lri.getDataDword() & 1);
}

typedef PreambleFixture Gen8L3Config;

BDWTEST_F(Gen8L3Config, checkNoSLM) {
    bool slmUsed = false;
    uint32_t l3Config = 0;

    l3Config = getL3ConfigHelper<IGFX_BROADWELL>(slmUsed);
    EXPECT_EQ(0x80000140u, l3Config);
}

BDWTEST_F(Gen8L3Config, checkSLM) {
    bool slmUsed = true;
    uint32_t l3Config = 0;

    l3Config = getL3ConfigHelper<IGFX_BROADWELL>(slmUsed);
    EXPECT_EQ(0x60000121u, l3Config);
}

typedef PreambleFixture ThreadArbitrationGen8;
BDWTEST_F(ThreadArbitrationGen8, givenPreambleWhenItIsProgrammedThenThreadArbitrationIsNotPresent) {
    typedef BDWFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<BDWFamily>::getL3Config(**platformDevices, true);

    PreambleHelper<BDWFamily>::programPreamble(&linearStream, MockDevice(**platformDevices), l3Config,
                                               ThreadArbitrationPolicy::threadArbirtrationPolicyRoundRobin,
                                               nullptr);

    parseCommands<BDWFamily>(cs);

    auto itorLRI = reverse_find<MI_LOAD_REGISTER_IMM *>(cmdList.rbegin(), cmdList.rend());
    ASSERT_NE(cmdList.rend(), itorLRI);

    //we expect l3 programming here
    const auto &lri = *reinterpret_cast<MI_LOAD_REGISTER_IMM *>(*itorLRI);
    auto RegisterOffset = L3CNTLRegisterOffset<BDWFamily>::registerOffset;
    EXPECT_EQ(RegisterOffset, lri.getRegisterOffset());
    EXPECT_EQ(1u, lri.getDataDword() & 1);

    EXPECT_EQ(0u, PreambleHelper<BDWFamily>::getAdditionalCommandsSize(MockDevice(**platformDevices)));
}

typedef PreambleFixture Gen8UrbEntryAllocationSize;
BDWTEST_F(Gen8UrbEntryAllocationSize, getUrbEntryAllocationSize) {
    uint32_t actualVal = PreambleHelper<FamilyType>::getUrbEntryAllocationSize();
    EXPECT_EQ(0x782u, actualVal);
}

BDWTEST_F(PreambleVfeState, basic) {
    typedef BDWFamily::PIPE_CONTROL PIPE_CONTROL;

    LinearStream &cs = linearStream;
    PreambleHelper<BDWFamily>::programVFEState(&linearStream, **platformDevices, 0, 0);

    parseCommands<BDWFamily>(cs);

    auto itorPC = find<PIPE_CONTROL *>(cmdList.begin(), cmdList.end());
    ASSERT_NE(cmdList.end(), itorPC);

    const auto &pc = *reinterpret_cast<PIPE_CONTROL *>(*itorPC);
    EXPECT_TRUE(pc.getDcFlushEnable());
    EXPECT_EQ(1u, pc.getCommandStreamerStallEnable());
}
