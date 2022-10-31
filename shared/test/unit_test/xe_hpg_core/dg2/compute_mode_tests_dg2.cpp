/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/ptr_math.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/test.h"
#include "shared/test/unit_test/command_stream/compute_mode_tests.h"

#include "test_traits_common.h"

using namespace NEO;

HWTEST2_F(ComputeModeRequirements, GivenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledThenCorrectCommandsAreAdded, IsDG2) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(true);

    setUpImpl<FamilyType>();

    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    auto expectedBitsMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;

    overrideComputeModeRequest<FamilyType>(true, false, false, false, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto startOffset = getCsrHw<FamilyType>()->commandStream.getUsed();

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, startOffset);

    auto pipeControlIterator = find<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControlIterator);

    EXPECT_TRUE(pipeControlCmd->getHdcPipelineFlush());
    EXPECT_TRUE(pipeControlCmd->getAmfsFlushEnable());
    EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControlCmd->getInstructionCacheInvalidateEnable());
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getUnTypedDataPortCacheFlush());
    EXPECT_TRUE(pipeControlCmd->getConstantCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getStateCacheInvalidationEnable());

    auto stateComputeModeCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), sizeof(PIPE_CONTROL)));
    EXPECT_TRUE(isValueSet(stateComputeModeCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(stateComputeModeCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, stateComputeModeCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(ComputeModeRequirements, GivenMultipleCCSEnabledSetupThenCorrectCommandsAreAdded, IsDG2) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 2;

    setUpImpl<FamilyType>(&hwInfo);
    MockOsContext ccsOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular}));

    getCsrHw<FamilyType>()->setupContext(ccsOsContext);

    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    auto expectedBitsMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;

    overrideComputeModeRequest<FamilyType>(true, false, false, false, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, hwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto startOffset = getCsrHw<FamilyType>()->commandStream.getUsed();

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, startOffset);

    auto pipeControlIterator = find<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControlIterator);

    EXPECT_TRUE(pipeControlCmd->getHdcPipelineFlush());
    EXPECT_TRUE(pipeControlCmd->getAmfsFlushEnable());
    EXPECT_TRUE(pipeControlCmd->getCommandStreamerStallEnable());
    EXPECT_TRUE(pipeControlCmd->getInstructionCacheInvalidateEnable());
    EXPECT_TRUE(pipeControlCmd->getTextureCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getUnTypedDataPortCacheFlush());
    EXPECT_TRUE(pipeControlCmd->getConstantCacheInvalidationEnable());
    EXPECT_TRUE(pipeControlCmd->getStateCacheInvalidationEnable());

    auto stateComputeModeCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), sizeof(PIPE_CONTROL)));
    EXPECT_TRUE(isValueSet(stateComputeModeCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(stateComputeModeCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, stateComputeModeCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(ComputeModeRequirements, GivenSingleCCSEnabledSetupThenCorrectCommandsAreAdded, IsDG2) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    setUpImpl<FamilyType>(&hwInfo);
    MockOsContext ccsOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::Regular}));

    getCsrHw<FamilyType>()->setupContext(ccsOsContext);

    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using FORCE_NON_COHERENT = typename STATE_COMPUTE_MODE::FORCE_NON_COHERENT;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    auto expectedBitsMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;

    overrideComputeModeRequest<FamilyType>(true, false, false, false, true);

    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_EQ(cmdsSize, retSize);

    getCsrHw<FamilyType>()->programComputeMode(stream, flags, hwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto startOffset = getCsrHw<FamilyType>()->commandStream.getUsed();

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(stream, startOffset);

    auto pipeControlIterator = find<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
    auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControlIterator);

    EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControlCmd));
    EXPECT_TRUE(pipeControlCmd->getUnTypedDataPortCacheFlush());

    EXPECT_FALSE(pipeControlCmd->getAmfsFlushEnable());
    EXPECT_FALSE(pipeControlCmd->getInstructionCacheInvalidateEnable());
    EXPECT_FALSE(pipeControlCmd->getTextureCacheInvalidationEnable());
    EXPECT_FALSE(pipeControlCmd->getConstantCacheInvalidationEnable());
    EXPECT_FALSE(pipeControlCmd->getStateCacheInvalidationEnable());

    auto stateComputeModeCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), sizeof(PIPE_CONTROL)));
    EXPECT_TRUE(isValueSet(stateComputeModeCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(stateComputeModeCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, stateComputeModeCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(ComputeModeRequirements, GivenProgramExtendedPipeControlPriorToNonPipelinedStateCommandEnabledThenCommandSizeIsCalculatedAndCorrectCommandSizeIsReturned, IsDG2) {
    DebugManagerStateRestore dbgRestorer;
    DebugManager.flags.ProgramExtendedPipeControlPriorToNonPipelinedStateCommand.set(true);

    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    setUpImpl<FamilyType>();

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);

    overrideComputeModeRequest<FamilyType>(false, false, false);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    overrideComputeModeRequest<FamilyType>(false, true, false);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    overrideComputeModeRequest<FamilyType>(true, true, false);
    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);

    overrideComputeModeRequest<FamilyType>(true, false, false);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
}
