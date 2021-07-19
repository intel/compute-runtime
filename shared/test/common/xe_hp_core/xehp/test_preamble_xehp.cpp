/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"

#include "opencl/source/helpers/hardware_commands_helper.h"
#include "test.h"

using namespace NEO;

struct XeHPSlm : HardwareParse, ::testing::Test {
    void SetUp() override {
        HardwareParse::SetUp();
    }

    void TearDown() override {
        HardwareParse::TearDown();
    }

    uint32_t cmdBuffer[1024];
    MockGraphicsAllocation gfxAllocation{cmdBuffer, sizeof(cmdBuffer)};
    LinearStream linearStream{&gfxAllocation};
};

XEHPTEST_F(XeHPSlm, givenTglWhenPreambleIsBeingProgrammedThenThreadArbitrationPolicyIsIgnored) {
    typedef XeHpFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<XeHpFamily>::getL3Config(*defaultHwInfo, true);
    MockDevice mockDevice;
    PreambleHelper<XeHpFamily>::programPreamble(&linearStream, mockDevice, l3Config,
                                                ThreadArbitrationPolicy::RoundRobin,
                                                nullptr);

    parseCommands<XeHpFamily>(cs);

    // parse through commands and ensure that 0xE404 is not being programmed
    EXPECT_EQ(0U, countMmio<FamilyType>(cmdList.begin(), cmdList.end(), 0xE404));
}

XEHPTEST_F(XeHPSlm, WhenProgrammingL3ThenLoadRegisterImmNotUsed) {
    typedef XeHpFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<FamilyType>::getL3Config(*defaultHwInfo, true);
    PreambleHelper<FamilyType>::programL3(&cs, l3Config);

    parseCommands<XeHpFamily>(cs);

    auto itorLRI = find<MI_LOAD_REGISTER_IMM *>(cmdList.begin(), cmdList.end());
    ASSERT_EQ(cmdList.end(), itorLRI);
}

using XeHPUrbEntryAllocationSize = ::testing::Test;
XEHPTEST_F(XeHPUrbEntryAllocationSize, WhenGettingUrbEntryAllocationSizeTheZeroIsReturned) {
    uint32_t actualVal = PreambleHelper<FamilyType>::getUrbEntryAllocationSize();
    EXPECT_EQ(0u, actualVal);
}

using XeHPPreambleVfeState = XeHPSlm;
XEHPTEST_F(XeHPPreambleVfeState, givenDefaultPipeControlWhenItIsProgrammedThenCsStallBitIsSet) {
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    PIPE_CONTROL *pipeControl = static_cast<PIPE_CONTROL *>(linearStream.getSpace(sizeof(PIPE_CONTROL)));
    *pipeControl = FamilyType::cmdInitPipeControl;

    EXPECT_EQ(1u, pipeControl->getCommandStreamerStallEnable());
}

XEHPTEST_F(XeHPPreambleVfeState, whenProgrammingComputeWalkerThenUavFieldsSetToZero) {
    using COMPUTE_WALKER = typename FamilyType::COMPUTE_WALKER;

    COMPUTE_WALKER *computeWalker = static_cast<COMPUTE_WALKER *>(linearStream.getSpace(sizeof(COMPUTE_WALKER)));
    *computeWalker = FamilyType::cmdInitGpgpuWalker;

    EXPECT_FALSE(computeWalker->getUavWaitToProduce());
    EXPECT_FALSE(computeWalker->getUavProducer());
    EXPECT_FALSE(computeWalker->getUavConsumer());
}

XEHPTEST_F(XeHPPreambleVfeState, whenProgrammingVfeStateThenDoNotAddPipeControlWaCmd) {
    LinearStream &cs = linearStream;

    size_t sizeBefore = cs.getUsed();
    PreambleHelper<FamilyType>::addPipeControlBeforeVfeCmd(&cs, defaultHwInfo.get(), EngineGroupType::RenderCompute);
    size_t sizeAfter = cs.getUsed();

    EXPECT_EQ(sizeBefore, sizeAfter);
}
