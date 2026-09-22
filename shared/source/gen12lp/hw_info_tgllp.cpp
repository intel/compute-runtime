/*
 * Copyright (C) 2019-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_info_tgllp.h"

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/gen12lp/hw_cmds_tgllp.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/release_helpers/caps/caps_setup.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_TIGERLAKE_LP>::abbreviation = "tgllp";

const PLATFORM TGLLP::platform = {
    IGFX_TIGERLAKE_LP,
    PCH_UNKNOWN,
    IGFX_GEN12LP_CORE,
    IGFX_GEN12LP_CORE,
    PLATFORM_NONE, // default init
    0x9A49,        // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable TGLLP::capabilityTable{
    .directSubmissionEngines = makeDirectSubmissionPropertiesPerEngine({
        {aub_stream::ENGINE_RCS, {.engineSupported = true, .submitOnInit = true, .useNonDefault = false, .useRootDevice = false}},
        {aub_stream::ENGINE_CCS, {.engineSupported = true, .submitOnInit = true, .useNonDefault = false, .useRootDevice = false}},
    }),
    .kmdNotifyProperties = {0, 0, 0, 0, false, false, false, false},
    .gpuAddressSpace = MemoryConstants::max64BitAppAddress,
    .sharedSystemMemCapabilities = 0,
    .requiredPreemptionSurfaceSize = MemoryConstants::pageSize,
    .deviceName = "",
    .preferredPlatformName = nullptr,
    .defaultPreemptionMode = PreemptionMode::ThreadGroup,
    .defaultEngineType = aub_stream::ENGINE_RCS,
    .maxRenderFrequency = 0,
    .extraQuantityThreadsPerEU = 1,
    .maxProgrammableSlmSize = 64,
    .grfSize = sizeof(TGLLP::GRF),
    .timestampValidBits = 36u,
    .kernelTimestampValidBits = 32u,
    .blitterOperationsSupported = false,
    .ftrSupportsFP64 = false,
    .ftrSupportsFP64Emulation = false,
    .ftrSupports64BitMath = false,
    .ftrSupportsCoherency = false,
    .ftrRenderCompressedBuffers = false,
    .ftrRenderCompressedImages = false,
    .instrumentationEnabled = true,
    .supportCacheFlushAfterWalker = false,
    .supportsImages = true,
    .supportsOnDemandPageFaults = false,
    .supportsIndependentForwardProgress = false,
    .isIntegratedDevice = true,
    .supportsMediaBlock = true,
    .fusedEuEnabled = true,
    .l0DebuggerSupported = false,
};

WorkaroundTable TGLLP::workaroundTable = {};
FeatureTable TGLLP::featureTable = {};

void TGLLP::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->flags.ftrL3IACoherency = true;
    featureTable->flags.ftrPPGTT = true;
    featureTable->flags.ftrSVM = true;
    featureTable->flags.ftrIA32eGfxPTEs = true;
    featureTable->flags.ftrStandardMipTailFormat = true;

    featureTable->flags.ftrTranslationTable = true;
    featureTable->flags.ftrUserModeTranslationTable = true;
    featureTable->flags.ftrTileMappedResource = true;

    featureTable->flags.ftrFbc = true;
    featureTable->flags.ftrTileY = true;

    featureTable->flags.ftrAstcHdr2D = true;
    featureTable->flags.ftrAstcLdr2D = true;

    featureTable->flags.ftrGpGpuMidBatchPreempt = true;
    featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = true;

    workaroundTable->flags.wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->flags.waUntypedBufferCompression = true;
};

void TGLLP::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const CompilerReleaseHelper *compilerReleaseHelper) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->NumThreadsPerEu = 7u;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * gtSysInfo->NumThreadsPerEu;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = TGLLP::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = TGLLP::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = TGLLP::maxSubslicesSupported;
    gtSysInfo->MaxDualSubSlicesSupported = TGLLP::maxDualSubslicesSupported;
    gtSysInfo->IsDynamicallyPopulated = false;

    setupCaps(*hwInfo);
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }

    applyDebugOverrides(*hwInfo);
}

const HardwareInfo TgllpHwConfig::hwInfo = {
    &TGLLP::platform,
    &TGLLP::featureTable,
    &TGLLP::workaroundTable,
    &TgllpHwConfig::gtSystemInfo,
    TGLLP::capabilityTable};

GT_SYSTEM_INFO TgllpHwConfig::gtSystemInfo = {0};
void TgllpHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const CompilerReleaseHelper *compilerReleaseHelper) {
    TGLLP::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, compilerReleaseHelper);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;

    const bool isHw1x2x16 = TGLLP::isHw1x2x16(*hwInfo);
    if (gtSysInfo->SliceCount == 0) {
        gtSysInfo->SliceCount = 1;
        gtSysInfo->DualSubSliceCount = isHw1x2x16 ? 2 : 6;
    }
    gtSysInfo->L3CacheSizeInKb = isHw1x2x16 ? 1920 : 3840;
    gtSysInfo->L3BankCount = isHw1x2x16 ? 4 : 8;

    gtSysInfo->CCSInfo.IsValid = true;
    gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;
    gtSysInfo->CCSInfo.Instances.CCSEnableMask = 0b1;
};

const HardwareInfo TGLLP::hwInfo = TgllpHwConfig::hwInfo;

void setupTGLLPHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const CompilerReleaseHelper *compilerReleaseHelper) {
    TgllpHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, compilerReleaseHelper);
}

void (*TGLLP::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const CompilerReleaseHelper *) = setupTGLLPHardwareInfoImpl;
} // namespace NEO
