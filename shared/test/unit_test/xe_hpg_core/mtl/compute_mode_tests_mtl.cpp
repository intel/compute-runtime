/*
 * Copyright (C) 2022-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/xe_hpg_core/hw_cmds_xe_hpg_core_base.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/header/per_product_test_definitions.h"
#include "shared/test/unit_test/command_stream/compute_mode_tests.h"

#include "test_traits_common.h"

using namespace NEO;

using MtlComputeModeRequirements = ComputeModeRequirements;

MTLTEST_F(MtlComputeModeRequirements, givenFlushWithoutSharedHandlesWhenPreviouslyUsedThenPcAndSCMAreNotProgrammed) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto graphicAlloc = csr->getMemoryManager()->allocateGraphicsMemoryWithProperties(MockAllocationProperties{csr->getRootDeviceIndex(), MemoryConstants::pageSize});
    IndirectHeap stream(graphicAlloc);

    makeResidentSharedAlloc();
    csr->flushTask(stream, 0, &stream, &stream, &stream, 0, flags, *device);
    EXPECT_TRUE(getCsrHw<FamilyType>()->getCsrRequestFlags()->hasSharedHandles);
    auto startOffset = getCsrHw<FamilyType>()->commandStream.getUsed();

    csr->flushTask(stream, 0, &stream, &stream, &stream, 0, flags, *device);
    EXPECT_TRUE(getCsrHw<FamilyType>()->getCsrRequestFlags()->hasSharedHandles);

    HardwareParse hwParser;
    hwParser.parseCommands<FamilyType>(getCsrHw<FamilyType>()->commandStream, startOffset);

    EXPECT_EQ(0u, hwParser.cmdList.size());

    csr->getMemoryManager()->freeGraphicsMemory(graphicAlloc);
}

MTLTEST_F(MtlComputeModeRequirements, givenComputeModeProgrammingWhenLargeGrfModeChangeIsRequiredThenCorrectCommandsAreAdded) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);
    char buff[1024]{};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    auto expectedBitsMask = FamilyType::stateComputeModeLargeGrfModeMask;

    expectedScmCmd.setLargeGrfMode(true);

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 256u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), sizeof(PIPE_CONTROL)));
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    auto startOffset = stream.getUsed();

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 128u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize * 2, stream.getUsed());

    expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(false);
    scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), sizeof(PIPE_CONTROL) + startOffset));
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

MTLTEST_F(MtlComputeModeRequirements, GivenSingleCCSEnabledSetupThenCorrectCommandsAreAdded) {
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
    overrideComputeModeRequest<FamilyType>(true, false, false, false);

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
    expectedScmCmd.setMaskBits(stateComputeModeCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, stateComputeModeCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

MTLTEST_F(MtlComputeModeRequirements, givenComputeModeProgrammingWhenRequiredGRFNumberIsLowerThan128ThenSmallGRFModeIsProgrammed) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);
    char buff[1024]{};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(false);
    auto expectedBitsMask = FamilyType::stateComputeModeLargeGrfModeMask;

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 127u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), sizeof(PIPE_CONTROL)));
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

MTLTEST_F(MtlComputeModeRequirements, givenComputeModeProgrammingWhenRequiredGRFNumberIsGreaterThan128ThenLargeGRFModeIsProgrammed) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);
    char buff[1024]{};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(true);
    auto expectedBitsMask = FamilyType::stateComputeModeLargeGrfModeMask;

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 256u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), sizeof(PIPE_CONTROL)));
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}
