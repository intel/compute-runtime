/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/preamble.h"
#include "shared/source/xe_hp_core/hw_cmds.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_graphics_allocation.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

struct XeHPSlm : HardwareParse, ::testing::Test {
    void SetUp() override {
        HardwareParse::setUp();
    }

    void TearDown() override {
        HardwareParse::tearDown();
    }

    uint32_t cmdBuffer[1024]{};
    MockGraphicsAllocation gfxAllocation{cmdBuffer, sizeof(cmdBuffer)};
    LinearStream linearStream{&gfxAllocation};
};

XEHPTEST_F(XeHPSlm, givenTglWhenPreambleIsBeingProgrammedThenThreadArbitrationPolicyIsIgnored) {
    typedef XeHpFamily::MI_LOAD_REGISTER_IMM MI_LOAD_REGISTER_IMM;
    LinearStream &cs = linearStream;
    uint32_t l3Config = PreambleHelper<XeHpFamily>::getL3Config(*defaultHwInfo, true);
    MockDevice mockDevice;
    PreambleHelper<XeHpFamily>::programPreamble(&linearStream, mockDevice, l3Config,
                                                nullptr, nullptr);

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

XEHPTEST_F(XeHPPreambleVfeState, WhenProgramVFEStateIsCalledThenCorrectCfeStateAddressIsReturned) {
    using CFE_STATE = typename FamilyType::CFE_STATE;

    char buffer[64];
    MockGraphicsAllocation graphicsAllocation(buffer, sizeof(buffer));
    LinearStream preambleStream(&graphicsAllocation, graphicsAllocation.getUnderlyingBuffer(), graphicsAllocation.getUnderlyingBufferSize());
    uint64_t addressToPatch = 0xC0DEC0DE;
    uint64_t expectedAddress = 0xDEC0C0;

    auto pCfeCmd = PreambleHelper<FamilyType>::getSpaceForVfeState(&preambleStream, *defaultHwInfo, EngineGroupType::RenderCompute);
    StreamProperties emptyProperties{};
    PreambleHelper<FamilyType>::programVfeState(pCfeCmd, *defaultHwInfo, 1024u, addressToPatch, 10u, emptyProperties, nullptr);
    EXPECT_GE(reinterpret_cast<uintptr_t>(pCfeCmd), reinterpret_cast<uintptr_t>(preambleStream.getCpuBase()));
    EXPECT_LT(reinterpret_cast<uintptr_t>(pCfeCmd), reinterpret_cast<uintptr_t>(preambleStream.getCpuBase()) + preambleStream.getUsed());

    auto &cfeCmd = *reinterpret_cast<CFE_STATE *>(pCfeCmd);
    EXPECT_EQ(10u, cfeCmd.getMaximumNumberOfThreads());
    EXPECT_EQ(1u, cfeCmd.getNumberOfWalkers());
    EXPECT_EQ(expectedAddress, cfeCmd.getScratchSpaceBuffer());
}

using XeHPPipelineSelect = ::testing::Test;

XEHPTEST_F(XeHPPipelineSelect, WhenAppendProgramPipelineSelectThenCorrectValuesSet) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    PIPELINE_SELECT cmd = FamilyType::cmdInitPipelineSelect;
    PreambleHelper<FamilyType>::appendProgramPipelineSelect(&cmd, true, *defaultHwInfo);
    EXPECT_TRUE(cmd.getSystolicModeEnable());
    PreambleHelper<FamilyType>::appendProgramPipelineSelect(&cmd, false, *defaultHwInfo);
    EXPECT_FALSE(cmd.getSystolicModeEnable());
    EXPECT_EQ(pipelineSelectSystolicModeEnableMaskBits, cmd.getMaskBits());
}

XEHPTEST_F(XeHPPipelineSelect, WhenProgramPipelineSelectThenProgramMediaSamplerDopClockGateEnable) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    PIPELINE_SELECT cmd = FamilyType::cmdInitPipelineSelect;
    LinearStream pipelineSelectStream(&cmd, sizeof(cmd));
    PreambleHelper<FamilyType>::programPipelineSelect(&pipelineSelectStream, {}, *defaultHwInfo);

    auto expectedSubMask = pipelineSelectMediaSamplerDopClockGateMaskBits;

    EXPECT_TRUE(cmd.getMediaSamplerDopClockGateEnable());
    EXPECT_EQ(expectedSubMask, (cmd.getMaskBits() & expectedSubMask));
}
