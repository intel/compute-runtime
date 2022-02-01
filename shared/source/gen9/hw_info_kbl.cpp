/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/helpers/constants.h"

#include "engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_KABYLAKE>::abbreviation = "kbl";

bool isSimulationKBL(unsigned short deviceId) {
    return false;
};

const PLATFORM KBL::platform = {
    IGFX_KABYLAKE,
    PCH_UNKNOWN,
    IGFX_GEN9_CORE,
    IGFX_GEN9_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    9,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable KBL::capabilityTable{
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_RCS, {true, true}}},   // directSubmissionEngines
    {0, 0, 0, 0, false, false, false, false},      // kmdNotifyProperties
    MemoryConstants::max48BitAddress,              // gpuAddressSpace
    0,                                             // sharedSystemMemCapabilities
    83.333,                                        // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                     // requiredPreemptionSurfaceSize
    &isSimulationKBL,                              // isSimulation
    "core",                                        // platformType
    "",                                            // deviceName
    PreemptionMode::MidThread,                     // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                        // defaultEngineType
    0,                                             // maxRenderFrequency
    30,                                            // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Kbl, // aubDeviceId
    0,                                             // extraQuantityThreadsPerEU
    64,                                            // slmSize
    sizeof(KBL::GRF),                              // grfSize
    36u,                                           // timestampValidBits
    32u,                                           // kernelTimestampValidBits
    false,                                         // blitterOperationsSupported
    true,                                          // ftrSupportsInteger64BitAtomics
    true,                                          // ftrSupportsFP64
    true,                                          // ftrSupports64BitMath
    true,                                          // ftrSvm
    true,                                          // ftrSupportsCoherency
    true,                                          // ftrSupportsVmeAvcTextureSampler
    false,                                         // ftrSupportsVmeAvcPreemption
    false,                                         // ftrRenderCompressedBuffers
    false,                                         // ftrRenderCompressedImages
    true,                                          // ftr64KBpages
    true,                                          // instrumentationEnabled
    true,                                          // sourceLevelDebuggerSupported
    true,                                          // supportsVme
    false,                                         // supportCacheFlushAfterWalker
    true,                                          // supportsImages
    false,                                         // supportsDeviceEnqueue
    true,                                          // supportsPipes
    true,                                          // supportsOcl21Features
    false,                                         // supportsOnDemandPageFaults
    true,                                          // supportsIndependentForwardProgress
    true,                                          // hostPtrTrackingEnabled
    true,                                          // levelZeroSupported
    true,                                          // isIntegratedDevice
    true,                                          // supportsMediaBlock
    false,                                         // p2pAccessSupported
    false,                                         // p2pAtomicAccessSupported
    false                                          // fusedEuEnabled
};

WorkaroundTable KBL::workaroundTable = {};
FeatureTable KBL::featureTable = {};

void KBL::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    PLATFORM *platform = &hwInfo->platform;
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->flags.ftrGpGpuMidBatchPreempt = true;
    featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = true;
    featureTable->flags.ftrL3IACoherency = true;
    featureTable->flags.ftrGpGpuMidThreadLevelPreempt = true;
    featureTable->flags.ftr3dMidBatchPreempt = true;
    featureTable->flags.ftr3dObjectLevelPreempt = true;
    featureTable->flags.ftrPerCtxtPreemptionGranularityControl = true;
    featureTable->flags.ftrPPGTT = true;
    featureTable->flags.ftrSVM = true;
    featureTable->flags.ftrIA32eGfxPTEs = true;
    featureTable->flags.ftrDisplayYTiling = true;
    featureTable->flags.ftrTranslationTable = true;
    featureTable->flags.ftrUserModeTranslationTable = true;
    featureTable->flags.ftrEnableGuC = true;
    featureTable->flags.ftrFbc = true;
    featureTable->flags.ftrFbc2AddressTranslation = true;
    featureTable->flags.ftrFbcBlitterTracking = true;
    featureTable->flags.ftrFbcCpuTracking = true;
    featureTable->flags.ftrTileY = true;

    workaroundTable->flags.waEnablePreemptionGranularityControlByUMD = true;
    workaroundTable->flags.waSendMIFLUSHBeforeVFE = true;
    workaroundTable->flags.waReportPerfCountUseGlobalContextID = true;
    workaroundTable->flags.waMsaa8xTileYDepthPitchAlignment = true;
    workaroundTable->flags.waLosslessCompressionSurfaceStride = true;
    workaroundTable->flags.waFbcLinearSurfaceStride = true;
    workaroundTable->flags.wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;

    if (platform->usRevId <= 0x6) {
        workaroundTable->flags.waDisableLSQCROPERFforOCL = true;
        workaroundTable->flags.waEncryptedEdramOnlyPartials = true;
    }
    if (platform->usRevId <= 0x8) {
        workaroundTable->flags.waForcePcBbFullCfgRestore = true;
    }
}

const HardwareInfo KBL_1x2x6::hwInfo = {
    &KBL::platform,
    &KBL::featureTable,
    &KBL::workaroundTable,
    &KBL_1x2x6::gtSystemInfo,
    KBL::capabilityTable,
};
GT_SYSTEM_INFO KBL_1x2x6::gtSystemInfo = {0};
void KBL_1x2x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * KBL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 2;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = KBL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = KBL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = KBL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo KBL_1x3x6::hwInfo = {
    &KBL::platform,
    &KBL::featureTable,
    &KBL::workaroundTable,
    &KBL_1x3x6::gtSystemInfo,
    KBL::capabilityTable,
};

GT_SYSTEM_INFO KBL_1x3x6::gtSystemInfo = {0};
void KBL_1x3x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * KBL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 768;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = KBL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = KBL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = KBL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo KBL_1x3x8::hwInfo = {
    &KBL::platform,
    &KBL::featureTable,
    &KBL::workaroundTable,
    &KBL_1x3x8::gtSystemInfo,
    KBL::capabilityTable,
};
GT_SYSTEM_INFO KBL_1x3x8::gtSystemInfo = {0};
void KBL_1x3x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * KBL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 768;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = KBL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = KBL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = KBL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo KBL_2x3x8::hwInfo = {
    &KBL::platform,
    &KBL::featureTable,
    &KBL::workaroundTable,
    &KBL_2x3x8::gtSystemInfo,
    KBL::capabilityTable,
};
GT_SYSTEM_INFO KBL_2x3x8::gtSystemInfo = {0};
void KBL_2x3x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * KBL::threadsPerEu;
    gtSysInfo->SliceCount = 2;
    gtSysInfo->L3CacheSizeInKb = 1536;
    gtSysInfo->L3BankCount = 8;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = KBL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = KBL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = KBL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo KBL_3x3x8::hwInfo = {
    &KBL::platform,
    &KBL::featureTable,
    &KBL::workaroundTable,
    &KBL_3x3x8::gtSystemInfo,
    KBL::capabilityTable,
};
GT_SYSTEM_INFO KBL_3x3x8::gtSystemInfo = {0};
void KBL_3x3x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * KBL::threadsPerEu;
    gtSysInfo->SliceCount = 3;
    gtSysInfo->L3CacheSizeInKb = 2304;
    gtSysInfo->L3BankCount = 12;
    gtSysInfo->MaxFillRate = 23;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = KBL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = KBL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = KBL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo KBL::hwInfo = KBL_1x3x6::hwInfo;
const uint64_t KBL::defaultHardwareInfoConfig = 0x100030006;

void setupKBLHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    if (hwInfoConfig == 0x100030008) {
        KBL_1x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x200030008) {
        KBL_2x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x300030008) {
        KBL_3x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100020006) {
        KBL_1x2x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100030006) {
        KBL_1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x0) {
        // Default config
        KBL_1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*KBL::setupHardwareInfo)(HardwareInfo *, bool, uint64_t) = setupKBLHardwareInfoImpl;
} // namespace NEO
