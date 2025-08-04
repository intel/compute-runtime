/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/helpers/pipeline_select_args.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/common/cmd_parse/hw_parse.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"

using PreambleTest = ::testing::Test;

using namespace NEO;

HWTEST2_F(PreambleTest, givenDisableEUFusionWhenProgramVFEStateThenFusedEUDispatchIsSetCorrectly, IsXeHpgCore) {
    typedef typename FamilyType::CFE_STATE CFE_STATE;

    auto bufferSize = PreambleHelper<FamilyType>::getVFECommandsSize();
    auto buffer = std::unique_ptr<char[]>(new char[bufferSize]);
    LinearStream stream(buffer.get(), bufferSize);

    stream.setGpuBase(0x1000);
    uint64_t expectedBufferGpuAddress = stream.getCurrentGpuAddressPosition();
    uint64_t cmdBufferGpuAddress = 0;

    auto feCmdPtr = PreambleHelper<FamilyType>::getSpaceForVfeState(&stream, *defaultHwInfo.get(), EngineGroupType::renderCompute, &cmdBufferGpuAddress);
    EXPECT_EQ(expectedBufferGpuAddress, cmdBufferGpuAddress);

    StreamProperties props;
    props.frontEndState.disableEUFusion.set(true);
    MockExecutionEnvironment executionEnvironment{};
    PreambleHelper<FamilyType>::programVfeState(feCmdPtr, *executionEnvironment.rootDeviceEnvironments[0], 0, 0, 0, props);

    auto cfeCmd = reinterpret_cast<CFE_STATE *>(feCmdPtr);
    EXPECT_EQ(1u, cfeCmd->getFusedEuDispatch());
}

HWTEST2_F(PreambleTest, givenSpecificDeviceWhenProgramPipelineSelectIsCalledThenExtraPipeControlIsAdded, IsXeHpgCore) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    const uint32_t rootDeviceIndex = 0u;
    NEO::HardwareInfo hwInfo = *NEO::defaultHwInfo.get();
    hwInfo.ipVersion.architecture = 12;
    hwInfo.ipVersion.release = 74;
    hwInfo.ipVersion.revision = 0;

    auto mockDevice = std::unique_ptr<MockDevice>(MockDevice::createWithNewExecutionEnvironment<NEO::MockDevice>(&hwInfo, rootDeviceIndex));

    constexpr size_t bufferSize = 256;
    uint8_t buffer[bufferSize];
    LinearStream stream(buffer, bufferSize);

    PipelineSelectArgs pipelineArgs;

    PreambleHelper<FamilyType>::programPipelineSelect(&stream, pipelineArgs, mockDevice->getRootDeviceEnvironment());
    size_t usedSpace = stream.getUsed();

    EXPECT_EQ(usedSpace, PreambleHelper<FamilyType>::getCmdSizeForPipelineSelect(mockDevice->getRootDeviceEnvironment()));
    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream);

    auto numPipeControl = hwParser.getCommandsList<PIPE_CONTROL>().size();
    EXPECT_EQ(1u, numPipeControl);

    auto pipeControl = hwParser.getCommand<PIPE_CONTROL>();
    EXPECT_TRUE(pipeControl->getRenderTargetCacheFlushEnable());

    auto numPipelineSelect = hwParser.getCommandsList<PIPELINE_SELECT>().size();
    EXPECT_EQ(1u, numPipelineSelect);
}
