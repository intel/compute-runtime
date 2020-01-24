/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen9/hw_cmds.h"
#include "core/memory_manager/memory_constants.h"
#include "runtime/aub_mem_dump/aub_services.h"

#include "engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_COFFEELAKE>::abbreviation = "cfl";

bool isSimulationCFL(unsigned short deviceId) {
    return false;
};

const PLATFORM CFL::platform = {
    IGFX_COFFEELAKE,
    PCH_UNKNOWN,
    IGFX_GEN9_CORE,
    IGFX_GEN9_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable CFL::capabilityTable{
    {0, 0, 0, false, false, false},                // kmdNotifyProperties
    MemoryConstants::max48BitAddress,              // gpuAddressSpace
    83.333,                                        // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                     // requiredPreemptionSurfaceSize
    &isSimulationCFL,                              // isSimulation
    PreemptionMode::MidThread,                     // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                        // defaultEngineType
    0,                                             // maxRenderFrequency
    21,                                            // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Cfl, // aubDeviceId
    0,                                             // extraQuantityThreadsPerEU
    64,                                            // slmSize
    sizeof(CFL::GRF),                              // grfSize
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
    true,                                          // forceStatelessCompilationFor32Bit
    "core",                                        // platformType
    true,                                          // sourceLevelDebuggerSupported
    true,                                          // supportsVme
    false,                                         // supportCacheFlushAfterWalker
    true,                                          // supportsImages
    true,                                          // supportsDeviceEnqueue
    true                                           // hostPtrTrackingEnabled
};

WorkaroundTable CFL::workaroundTable = {};
FeatureTable CFL::featureTable = {};

void CFL::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->ftrGpGpuMidBatchPreempt = true;
    featureTable->ftrGpGpuThreadGroupLevelPreempt = true;
    featureTable->ftrL3IACoherency = true;
    featureTable->ftrVEBOX = true;
    featureTable->ftrGpGpuMidThreadLevelPreempt = false;
    featureTable->ftr3dMidBatchPreempt = true;
    featureTable->ftr3dObjectLevelPreempt = true;
    featureTable->ftrPerCtxtPreemptionGranularityControl = true;
    featureTable->ftrPPGTT = true;
    featureTable->ftrSVM = true;
    featureTable->ftrIA32eGfxPTEs = true;
    featureTable->ftrDisplayYTiling = true;
    featureTable->ftrTranslationTable = true;
    featureTable->ftrUserModeTranslationTable = true;
    featureTable->ftrEnableGuC = true;
    featureTable->ftrFbc = true;
    featureTable->ftrFbc2AddressTranslation = true;
    featureTable->ftrFbcBlitterTracking = true;
    featureTable->ftrFbcCpuTracking = true;
    featureTable->ftrTileY = true;

    workaroundTable->waEnablePreemptionGranularityControlByUMD = true;
    workaroundTable->waSendMIFLUSHBeforeVFE = true;
    workaroundTable->waReportPerfCountUseGlobalContextID = true;
    workaroundTable->waMsaa8xTileYDepthPitchAlignment = true;
    workaroundTable->waLosslessCompressionSurfaceStride = true;
    workaroundTable->waFbcLinearSurfaceStride = true;
    workaroundTable->wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;
}

const HardwareInfo CFL_1x2x6::hwInfo = {
    &CFL::platform,
    &CFL::featureTable,
    &CFL::workaroundTable,
    &CFL_1x2x6::gtSystemInfo,
    CFL::capabilityTable,
};
GT_SYSTEM_INFO CFL_1x2x6::gtSystemInfo = {0};
void CFL_1x2x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * CFL::threadsPerEu;
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
    gtSysInfo->MaxEuPerSubSlice = CFL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = CFL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = CFL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo CFL_1x3x6::hwInfo = {
    &CFL::platform,
    &CFL::featureTable,
    &CFL::workaroundTable,
    &CFL_1x3x6::gtSystemInfo,
    CFL::capabilityTable,
};
GT_SYSTEM_INFO CFL_1x3x6::gtSystemInfo = {0};
void CFL_1x3x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * CFL::threadsPerEu;
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
    gtSysInfo->MaxEuPerSubSlice = CFL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = CFL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = CFL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo CFL_1x3x8::hwInfo = {
    &CFL::platform,
    &CFL::featureTable,
    &CFL::workaroundTable,
    &CFL_1x3x8::gtSystemInfo,
    CFL::capabilityTable,
};
GT_SYSTEM_INFO CFL_1x3x8::gtSystemInfo = {0};
void CFL_1x3x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * CFL::threadsPerEu;
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
    gtSysInfo->MaxEuPerSubSlice = CFL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = CFL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = CFL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo CFL_2x3x8::hwInfo = {
    &CFL::platform,
    &CFL::featureTable,
    &CFL::workaroundTable,
    &CFL_2x3x8::gtSystemInfo,
    CFL::capabilityTable,
};
GT_SYSTEM_INFO CFL_2x3x8::gtSystemInfo = {0};
void CFL_2x3x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * CFL::threadsPerEu;
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
    gtSysInfo->MaxEuPerSubSlice = CFL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = CFL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = CFL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo CFL_3x3x8::hwInfo = {
    &CFL::platform,
    &CFL::featureTable,
    &CFL::workaroundTable,
    &CFL_3x3x8::gtSystemInfo,
    CFL::capabilityTable,
};
GT_SYSTEM_INFO CFL_3x3x8::gtSystemInfo = {0};
void CFL_3x3x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * CFL::threadsPerEu;
    gtSysInfo->SliceCount = 3;
    gtSysInfo->L3CacheSizeInKb = 2304;
    gtSysInfo->L3BankCount = 12;
    gtSysInfo->MaxFillRate = 24;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = CFL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = CFL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = CFL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo CFL::hwInfo = CFL_1x3x6::hwInfo;
const uint64_t CFL::defaultHardwareInfoConfig = 0x100030006;

void setupCFLHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    if (hwInfoConfig == 0x100030008) {
        CFL_1x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x200030008) {
        CFL_2x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x300030008) {
        CFL_3x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100020006) {
        CFL_1x2x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100030006) {
        CFL_1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x0) {
        // Default config
        CFL_1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*CFL::setupHardwareInfo)(HardwareInfo *, bool, uint64_t) = setupCFLHardwareInfoImpl;
} // namespace NEO
