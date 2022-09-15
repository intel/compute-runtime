/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/csr_definitions.h"
#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/gen12lp/hw_cmds_adlp.h"
#include "shared/source/helpers/pipeline_select_helper.h"
#include "shared/source/helpers/preamble.h"
#include "shared/test/common/helpers/dispatch_flags_helper.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/common/test_macros/test.h"

using namespace NEO;

using PreambleHelperTestsAdlp = ::testing::Test;

ADLPTEST_F(PreambleHelperTestsAdlp, givenSystolicPipelineSelectModeDisabledWhenProgrammingPipelineSelectThenDisableSystolicMode) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    constexpr static auto bufferSize = sizeof(PIPELINE_SELECT);

    char streamBuffer[bufferSize];
    LinearStream stream{streamBuffer, sizeof(bufferSize)};

    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    flags.pipelineSelectArgs.systolicPipelineSelectMode = false;
    flags.pipelineSelectArgs.systolicPipelineSelectSupport = PreambleHelper<FamilyType>::isSystolicModeConfigurable(ADLP::hwInfo);

    auto *pCmd = static_cast<PIPELINE_SELECT *>(stream.getSpace(0));
    PreambleHelper<FamilyType>::programPipelineSelect(&stream, flags.pipelineSelectArgs, ADLP::hwInfo);

    const auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits | pipelineSelectSystolicModeEnableMaskBits;
    EXPECT_FALSE(pCmd->getSpecialModeEnable());
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU, pCmd->getPipelineSelection());
}

ADLPTEST_F(PreambleHelperTestsAdlp, givenSystolicPipelineSelectModeEnabledWhenProgrammingPipelineSelectThenEnableSystolicMode) {
    using PIPELINE_SELECT = typename FamilyType::PIPELINE_SELECT;
    constexpr static auto bufferSize = sizeof(PIPELINE_SELECT);

    char streamBuffer[bufferSize];
    LinearStream stream{streamBuffer, sizeof(bufferSize)};

    DispatchFlags flags = DispatchFlagsHelper::createDefaultDispatchFlags();
    flags.pipelineSelectArgs.systolicPipelineSelectMode = true;
    flags.pipelineSelectArgs.systolicPipelineSelectSupport = PreambleHelper<FamilyType>::isSystolicModeConfigurable(ADLP::hwInfo);

    auto *pCmd = static_cast<PIPELINE_SELECT *>(stream.getSpace(0));
    PreambleHelper<FamilyType>::programPipelineSelect(&stream, flags.pipelineSelectArgs, ADLP::hwInfo);

    const auto expectedMask = pipelineSelectEnablePipelineSelectMaskBits | pipelineSelectMediaSamplerDopClockGateMaskBits | pipelineSelectSystolicModeEnableMaskBits;
    EXPECT_TRUE(pCmd->getSpecialModeEnable());
    EXPECT_EQ(expectedMask, pCmd->getMaskBits());
    EXPECT_EQ(PIPELINE_SELECT::PIPELINE_SELECTION_GPGPU, pCmd->getPipelineSelection());
}
