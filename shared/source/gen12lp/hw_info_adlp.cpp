/*
 * Copyright (C) 2021-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_info_adlp.h"

#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/gen12lp/hw_cmds_adlp.h"
#include "shared/source/helpers/constants.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_ALDERLAKE_P>::abbreviation = "adlp";

const PLATFORM ADLP::platform = {
    IGFX_ALDERLAKE_P,
    PCH_UNKNOWN,
    IGFX_GEN12LP_CORE,
    IGFX_GEN12LP_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable ADLP::capabilityTable{
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
    .clVersionSupport = 30,
    .extraQuantityThreadsPerEU = 1,
    .maxProgrammableSlmSize = 64,
    .grfSize = sizeof(ADLP::GRF),
    .timestampValidBits = 36u,
    .kernelTimestampValidBits = 32u,
    .blitterOperationsSupported = false,
    .ftrSupportsInteger64BitAtomics = true,
    .ftrSupportsFP64 = false,
    .ftrSupportsFP64Emulation = false,
    .ftrSupports64BitMath = false,
    .ftrSupportsCoherency = false,
    .ftrRenderCompressedBuffers = false,
    .ftrRenderCompressedImages = false,
    .instrumentationEnabled = true,
    .supportCacheFlushAfterWalker = false,
    .supportsImages = true,
    .supportsOcl21Features = true,
    .supportsOnDemandPageFaults = false,
    .supportsIndependentForwardProgress = false,
    .isIntegratedDevice = true,
    .supportsMediaBlock = true,
    .fusedEuEnabled = true,
    .l0DebuggerSupported = false,
    .supportsFloatAtomics = true,
    .cxlType = 0};

WorkaroundTable ADLP::workaroundTable = {};
FeatureTable ADLP::featureTable = {};

void ADLP::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
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
    featureTable->flags.ftrTileY = false;

    featureTable->flags.ftrAstcHdr2D = true;
    featureTable->flags.ftrAstcLdr2D = true;

    featureTable->flags.ftrGpGpuMidBatchPreempt = true;
    featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = true;

    workaroundTable->flags.wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->flags.waUntypedBufferCompression = true;
};

void ADLP::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->NumThreadsPerEu = 7u;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * gtSysInfo->NumThreadsPerEu;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = ADLP::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = ADLP::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = ADLP::maxSubslicesSupported;
    gtSysInfo->MaxDualSubSlicesSupported = ADLP::maxDualSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }

    applyDebugOverrides(*hwInfo);
}

const HardwareInfo AdlpHwConfig::hwInfo = {
    &ADLP::platform,
    &ADLP::featureTable,
    &ADLP::workaroundTable,
    &AdlpHwConfig::gtSystemInfo,
    ADLP::capabilityTable};

GT_SYSTEM_INFO AdlpHwConfig::gtSystemInfo = {0};
void AdlpHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->IsDynamicallyPopulated = false;

    // non-zero values for unit tests
    if (gtSysInfo->SliceCount == 0) {
        gtSysInfo->SliceCount = 2;
        gtSysInfo->SubSliceCount = 8;
        gtSysInfo->EUCount = 40;
        gtSysInfo->MaxEuPerSubSlice = ADLP::maxEuPerSubslice;
        gtSysInfo->MaxSlicesSupported = ADLP::maxSlicesSupported;
        gtSysInfo->MaxSubSlicesSupported = ADLP::maxSubslicesSupported;

        gtSysInfo->L3BankCount = 1;

        gtSysInfo->CCSInfo.IsValid = true;
        gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;
    }
    gtSysInfo->L3CacheSizeInKb = 1;

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};
const HardwareInfo ADLP::hwInfo = AdlpHwConfig::hwInfo;

void setupADLPHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    AdlpHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

void (*ADLP::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupADLPHardwareInfoImpl;
} // namespace NEO
