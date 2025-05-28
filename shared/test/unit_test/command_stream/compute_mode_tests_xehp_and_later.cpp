/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/bit_helpers.h"
#include "shared/source/helpers/ptr_math.h"
#include "shared/source/release_helper/release_helper.h"
#include "shared/test/common/helpers/debug_manager_state_restore.h"
#include "shared/test/common/helpers/engine_descriptor_helper.h"
#include "shared/test/common/helpers/unit_test_helper.h"
#include "shared/test/common/mocks/mock_allocation_properties.h"
#include "shared/test/common/mocks/mock_device.h"
#include "shared/test/common/mocks/mock_os_context.h"
#include "shared/test/common/test_macros/hw_test.h"
#include "shared/test/unit_test/command_stream/compute_mode_tests.h"

#include "test_traits_common.h"

using namespace NEO;

HWCMDTEST_F(IGFX_XE_HP_CORE, ComputeModeRequirements, givenCoherencyWithoutSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;
    setUpImpl<FamilyType>();

    const auto &productHelper = device->getProductHelper();
    auto *releaseHelper = device->getReleaseHelper();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, csr->isRcs(), releaseHelper);
    std::ignore = isExtendedWARequired;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    if (isBasicWARequired) {
        cmdsSize += +sizeof(PIPE_CONTROL);
    }

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

HWCMDTEST_F(IGFX_XE_HP_CORE, ComputeModeRequirements, givenCoherencyWithSharedHandlesWhenCommandSizeIsCalculatedThenCorrectCommandSizeIsReturned) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    const auto &productHelper = device->getProductHelper();
    auto *releaseHelper = device->getReleaseHelper();

    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, csr->isRcs(), releaseHelper);
    std::ignore = isExtendedWARequired;

    overrideComputeModeRequest<FamilyType>(false, false, true);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    overrideComputeModeRequest<FamilyType>(false, true, true);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + MemorySynchronizationCommands<FamilyType>::getSizeForStallingBarrier();
    if (isBasicWARequired) {
        cmdsSize += +sizeof(PIPE_CONTROL);
    }

    overrideComputeModeRequest<FamilyType>(true, true, true);
    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);

    overrideComputeModeRequest<FamilyType>(true, false, true);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdsSize, retSize);
}

struct ForceNonCoherentSupportedMatcher {
    template <PRODUCT_FAMILY productFamily>
    static constexpr bool isMatched() {
        if constexpr (HwMapper<productFamily>::GfxProduct::supportsCmdSet(IGFX_XE_HP_CORE)) {
            return TestTraits<NEO::ToGfxCoreFamily<productFamily>::get()>::forceNonCoherentSupported;
        }
        return false;
    }
};

HWTEST2_F(ComputeModeRequirements, givenCoherencyWithoutSharedHandlesWhenComputeModeIsProgrammedThenCorrectCommandsAreAdded, ForceNonCoherentSupportedMatcher) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto *releaseHelper = device->getReleaseHelper();
    const auto &productHelper = device->getProductHelper();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, csr->isRcs(), releaseHelper);
    std::ignore = isExtendedWARequired;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    if (isBasicWARequired) {
        cmdsSize += +sizeof(PIPE_CONTROL);
    }

    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    auto expectedBitsMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;

    overrideComputeModeRequest<FamilyType>(true, false, false, false, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    if (isBasicWARequired) {
        scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), sizeof(PIPE_CONTROL)));
    }
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    auto startOffset = stream.getUsed();

    overrideComputeModeRequest<FamilyType>(true, true, false, false, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize * 2, stream.getUsed());

    expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED);
    scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), startOffset));
    if (isBasicWARequired) {
        scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), startOffset + sizeof(PIPE_CONTROL)));
    }
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(ComputeModeRequirements, givenCoherencyWithSharedHandlesWhenComputeModeIsProgrammedThenCorrectCommandsAreAdded, ForceNonCoherentSupportedMatcher) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto *releaseHelper = device->getReleaseHelper();
    const auto &productHelper = device->getProductHelper();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, csr->isRcs(), releaseHelper);
    std::ignore = isExtendedWARequired;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL);
    if (isBasicWARequired) {
        cmdsSize += +sizeof(PIPE_CONTROL);
    }
    char buff[1024] = {0};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    auto expectedBitsMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;

    auto expectedPcCmd = FamilyType::cmdInitPipeControl;

    overrideComputeModeRequest<FamilyType>(true, false, true, false, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    if (isBasicWARequired) {
        scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), sizeof(PIPE_CONTROL)));
    }
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    auto pcCmd = reinterpret_cast<PIPE_CONTROL *>(ptrOffset(stream.getCpuBase(), sizeof(STATE_COMPUTE_MODE)));
    if (isBasicWARequired) {
        pcCmd = reinterpret_cast<PIPE_CONTROL *>(ptrOffset(stream.getCpuBase(), sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL)));
    }
    EXPECT_TRUE(memcmp(&expectedPcCmd, pcCmd, sizeof(PIPE_CONTROL)) == 0);

    auto startOffset = stream.getUsed();

    overrideComputeModeRequest<FamilyType>(true, true, true, false, true);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize * 2, stream.getUsed());

    expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_DISABLED);
    scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), startOffset));
    if (isBasicWARequired) {
        scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), startOffset + sizeof(PIPE_CONTROL)));
    }
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    pcCmd = reinterpret_cast<PIPE_CONTROL *>(ptrOffset(stream.getCpuBase(), startOffset + sizeof(STATE_COMPUTE_MODE)));
    if (isBasicWARequired) {
        pcCmd = reinterpret_cast<PIPE_CONTROL *>(ptrOffset(stream.getCpuBase(), startOffset + sizeof(STATE_COMPUTE_MODE) + sizeof(PIPE_CONTROL)));
    }
    EXPECT_TRUE(memcmp(&expectedPcCmd, pcCmd, sizeof(PIPE_CONTROL)) == 0);
}

HWTEST2_F(ComputeModeRequirements, givenFlushWithoutSharedHandlesWhenPreviouslyUsedThenPcAndSCMAreNotProgrammed, ForceNonCoherentSupportedMatcher) {
    setUpImpl<FamilyType>();

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

HWCMDTEST_F(IGFX_XE_HP_CORE, ComputeModeRequirements, givenComputeModeCmdSizeWhenLargeGrfModeChangeIsRequiredThenSCMCommandSizeIsCalculated) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    overrideComputeModeRequest<FamilyType>(false, false, false, false, 128u);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());

    auto *releaseHelper = device->getReleaseHelper();
    const auto &productHelper = device->getProductHelper();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, csr->isRcs(), releaseHelper);
    std::ignore = isExtendedWARequired;

    auto cmdSize = sizeof(STATE_COMPUTE_MODE);
    if (isBasicWARequired) {
        cmdSize += +sizeof(PIPE_CONTROL);
    }

    overrideComputeModeRequest<FamilyType>(false, false, false, true, 256u);
    auto retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdSize, retSize);

    overrideComputeModeRequest<FamilyType>(true, false, false, true, 256u);
    retSize = getCsrHw<FamilyType>()->getCmdSizeForComputeMode();
    EXPECT_TRUE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
    EXPECT_EQ(cmdSize, retSize);
}

HWTEST2_F(ComputeModeRequirements, givenComputeModeProgrammingWhenLargeGrfModeChangeIsRequiredThenCorrectCommandsAreAdded, ForceNonCoherentSupportedMatcher) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto *releaseHelper = device->getReleaseHelper();
    const auto &productHelper = device->getProductHelper();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, csr->isRcs(), releaseHelper);
    std::ignore = isExtendedWARequired;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    if (isBasicWARequired) {
        cmdsSize += +sizeof(PIPE_CONTROL);
    }
    char buff[1024];
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    auto expectedBitsMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;

    expectedScmCmd.setLargeGrfMode(true);

    overrideComputeModeRequest<FamilyType>(true, false, false, true, 256u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    if (isBasicWARequired) {
        scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), sizeof(PIPE_CONTROL)));
    }
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);

    auto startOffset = stream.getUsed();

    overrideComputeModeRequest<FamilyType>(true, false, false, true, 128u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize * 2, stream.getUsed());

    expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(false);
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), startOffset));
    if (isBasicWARequired) {
        scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), startOffset + sizeof(PIPE_CONTROL)));
    }
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWCMDTEST_F(IGFX_XE_HP_CORE, ComputeModeRequirements, givenComputeModeProgrammingWhenLargeGrfModeDoesntChangeThenSCMIsNotAdded) {
    setUpImpl<FamilyType>();

    char buff[1024];
    LinearStream stream(buff, 1024);

    overrideComputeModeRequest<FamilyType>(false, false, false, false, 256u);
    EXPECT_FALSE(getCsrHw<FamilyType>()->streamProperties.stateComputeMode.isDirty());
}

HWTEST2_F(ComputeModeRequirements, givenComputeModeProgrammingWhenRequiredGRFNumberIsLowerThan128ThenSmallGRFModeIsProgrammed, ForceNonCoherentSupportedMatcher) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto *releaseHelper = device->getReleaseHelper();
    const auto &productHelper = device->getProductHelper();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, csr->isRcs(), releaseHelper);
    std::ignore = isExtendedWARequired;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    if (isBasicWARequired) {
        cmdsSize += +sizeof(PIPE_CONTROL);
    }
    char buff[1024];
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(false);
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    auto expectedBitsMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;

    overrideComputeModeRequest<FamilyType>(true, false, false, true, 127u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    if (isBasicWARequired) {
        scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), sizeof(PIPE_CONTROL)));
    }
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(ComputeModeRequirements, givenComputeModeProgrammingWhenRequiredGRFNumberIsGreaterThan128ThenLargeGRFModeIsProgrammed, ForceNonCoherentSupportedMatcher) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto *releaseHelper = device->getReleaseHelper();
    const auto &productHelper = device->getProductHelper();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, csr->isRcs(), releaseHelper);
    std::ignore = isExtendedWARequired;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    if (isBasicWARequired) {
        cmdsSize += +sizeof(PIPE_CONTROL);
    }
    char buff[1024];
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setForceNonCoherent(STATE_COMPUTE_MODE::FORCE_NON_COHERENT_FORCE_GPU_NON_COHERENT);
    expectedScmCmd.setLargeGrfMode(true);
    auto expectedBitsMask = FamilyType::stateComputeModeForceNonCoherentMask | FamilyType::stateComputeModeLargeGrfModeMask;

    overrideComputeModeRequest<FamilyType>(true, false, false, true, 256u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(stream.getCpuBase());
    if (isBasicWARequired) {
        scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), sizeof(PIPE_CONTROL)));
    }
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(ComputeModeRequirements, GivenSingleCCSEnabledSetupThenCorrectCommandsAreAdded, IsXeHpgCore) {
    HardwareInfo hwInfo = *defaultHwInfo;
    hwInfo.gtSystemInfo.CCSInfo.NumberOfCCSEnabled = 1;

    setUpImpl<FamilyType>(&hwInfo);
    MockOsContext ccsOsContext(0, EngineDescriptorHelper::getDefaultDescriptor({aub_stream::ENGINE_CCS, EngineUsage::regular}));

    getCsrHw<FamilyType>()->setupContext(ccsOsContext);

    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto *releaseHelper = device->getReleaseHelper();
    EXPECT_NE(nullptr, releaseHelper);
    const auto &productHelper = device->getProductHelper();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, csr->isRcs(), releaseHelper);
    std::ignore = isExtendedWARequired;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    auto cmdOffset = 0u;
    if (isBasicWARequired) {
        cmdsSize += sizeof(PIPE_CONTROL);
        cmdOffset = sizeof(PIPE_CONTROL);
    }
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
    if (isBasicWARequired) {
        auto pipeControlIterator = find<PIPE_CONTROL *>(hwParser.cmdList.begin(), hwParser.cmdList.end());
        auto pipeControlCmd = genCmdCast<PIPE_CONTROL *>(*pipeControlIterator);

        EXPECT_TRUE(UnitTestHelper<FamilyType>::getPipeControlHdcPipelineFlush(*pipeControlCmd));
        EXPECT_TRUE(pipeControlCmd->getUnTypedDataPortCacheFlush());

        EXPECT_FALSE(pipeControlCmd->getAmfsFlushEnable());
        EXPECT_FALSE(pipeControlCmd->getInstructionCacheInvalidateEnable());
        EXPECT_FALSE(pipeControlCmd->getTextureCacheInvalidationEnable());
        EXPECT_FALSE(pipeControlCmd->getConstantCacheInvalidationEnable());
        EXPECT_FALSE(pipeControlCmd->getStateCacheInvalidationEnable());
    }

    auto stateComputeModeCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), cmdOffset));
    expectedScmCmd.setMaskBits(stateComputeModeCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, stateComputeModeCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}

HWTEST2_F(ComputeModeRequirements, givenComputeModeProgrammingWhenRequiredGRFNumberIsLowerThan128ThenSmallGRFModeIsProgrammed, IsXeHpgCore) {
    setUpImpl<FamilyType>();
    using STATE_COMPUTE_MODE = typename FamilyType::STATE_COMPUTE_MODE;
    using PIPE_CONTROL = typename FamilyType::PIPE_CONTROL;

    auto *releaseHelper = device->getReleaseHelper();
    EXPECT_NE(nullptr, releaseHelper);
    const auto &productHelper = device->getProductHelper();
    const auto &[isBasicWARequired, isExtendedWARequired] = productHelper.isPipeControlPriorToNonPipelinedStateCommandsWARequired(*defaultHwInfo, csr->isRcs(), releaseHelper);
    std::ignore = isExtendedWARequired;

    auto cmdsSize = sizeof(STATE_COMPUTE_MODE);
    auto cmdOffset = 0u;
    if (isBasicWARequired) {
        cmdsSize += sizeof(PIPE_CONTROL);
        cmdOffset = sizeof(PIPE_CONTROL);
    }
    char buff[1024]{};
    LinearStream stream(buff, 1024);

    auto expectedScmCmd = FamilyType::cmdInitStateComputeMode;
    expectedScmCmd.setLargeGrfMode(false);
    auto expectedBitsMask = FamilyType::stateComputeModeLargeGrfModeMask;
    overrideComputeModeRequest<FamilyType>(false, false, false, true, 127u);
    getCsrHw<FamilyType>()->programComputeMode(stream, flags, *defaultHwInfo);
    EXPECT_EQ(cmdsSize, stream.getUsed());

    auto scmCmd = reinterpret_cast<STATE_COMPUTE_MODE *>(ptrOffset(stream.getCpuBase(), cmdOffset));
    EXPECT_TRUE(isValueSet(scmCmd->getMaskBits(), expectedBitsMask));
    expectedScmCmd.setMaskBits(scmCmd->getMaskBits());
    EXPECT_TRUE(memcmp(&expectedScmCmd, scmCmd, sizeof(STATE_COMPUTE_MODE)) == 0);
}
