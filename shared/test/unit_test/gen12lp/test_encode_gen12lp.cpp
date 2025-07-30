/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_container/command_encoder.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/command_stream/stream_properties.h"
#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/test/common/helpers/default_hw_info.h"
#include "shared/test/common/mocks/mock_execution_environment.h"
#include "shared/test/common/test_macros/hw_test.h"
using namespace NEO;

using CommandEncodeGen12LpTest = ::testing::Test;

GEN12LPTEST_F(CommandEncodeGen12LpTest, whenProgrammingStateComputeModeThenProperFieldsAreSet) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    uint8_t buffer[64]{};
    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    StateComputeModeProperties properties;
    auto pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment);
    auto pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    EXPECT_EQ(FamilyType::stateComputeModeForceNonCoherentMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT, pScm->getForceNonCoherent());

    properties.isCoherencyRequired.value = 1;
    pLinearStream = std::make_unique<LinearStream>(buffer, sizeof(buffer));
    EncodeComputeMode<FamilyType>::programComputeModeCommand(*pLinearStream, properties, rootDeviceEnvironment);
    pScm = reinterpret_cast<STATE_COMPUTE_MODE *>(pLinearStream->getCpuBase());
    EXPECT_EQ(FamilyType::stateComputeModeForceNonCoherentMask, pScm->getMaskBits());
    EXPECT_EQ(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED, pScm->getForceNonCoherent());
}

GEN12LPTEST_F(CommandEncodeGen12LpTest, givenEncodeDataInMemoryWhenProgrammingFeCmdThenTakeNoAction) {
    constexpr size_t bufferSize = 256;
    alignas(8) uint8_t buffer[bufferSize] = {0x0};
    void *bufferPtr = buffer;
    alignas(8) uint8_t zeroBuffer[bufferSize] = {0x0};
    LinearStream cmdStream(buffer, bufferSize);

    MockExecutionEnvironment executionEnvironment{};
    auto &rootDeviceEnvironment = *executionEnvironment.rootDeviceEnvironments[0];
    StreamProperties properties;

    uint64_t dstGpuAddress = 0x1000;

    EncodeDataMemory<FamilyType>::programFrontEndState(bufferPtr, dstGpuAddress, rootDeviceEnvironment, 0x0, 0x0, 0x40, properties);
    EXPECT_EQ(0, memcmp(buffer, zeroBuffer, bufferSize));

    EncodeDataMemory<FamilyType>::programFrontEndState(cmdStream, dstGpuAddress, rootDeviceEnvironment, 0x0, 0x0, 0x40, properties);
    EXPECT_EQ(0u, cmdStream.getUsed());
}
