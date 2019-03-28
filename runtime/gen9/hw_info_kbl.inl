/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub_mem_dump/aub_services.h"
#include "runtime/memory_manager/memory_constants.h"

#include "engine_node.h"
#include "hw_cmds.h"
#include "hw_info_kbl.h"

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
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable KBL::capabilityTable{
    {0, 0, 0, false, false, false},                // kmdNotifyProperties
    {true, false},                                 // whitelistedRegisters
    MemoryConstants::max48BitAddress,              // gpuAddressSpace
    83.333,                                        // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                     // requiredPreemptionSurfaceSize
    &isSimulationKBL,                              // isSimulation
    PreemptionMode::MidThread,                     // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                        // defaultEngineType
    0,                                             // maxRenderFrequency
    21,                                            // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Kbl, // aubDeviceId
    0,                                             // extraQuantityThreadsPerEU
    64,                                            // slmSize
    false,                                         // blitterOperationsSupported
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
    true,                                          // isCore
    true,                                          // sourceLevelDebuggerSupported
    true,                                          // supportsVme
    false                                          // supportCacheFlushAfterWalker
};

WorkaroundTable KBL::workaroundTable = {};
FeatureTable KBL::featureTable = {};

void KBL::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    PLATFORM *pPlatform = const_cast<PLATFORM *>(hwInfo->pPlatform);
    FeatureTable *pSkuTable = const_cast<FeatureTable *>(hwInfo->pSkuTable);
    WorkaroundTable *pWaTable = const_cast<WorkaroundTable *>(hwInfo->pWaTable);

    pSkuTable->ftrVEBOX = true;
    pSkuTable->ftrGpGpuMidThreadLevelPreempt = true;
    pSkuTable->ftr3dMidBatchPreempt = true;
    pSkuTable->ftr3dObjectLevelPreempt = true;
    pSkuTable->ftrPerCtxtPreemptionGranularityControl = true;
    pSkuTable->ftrPPGTT = true;
    pSkuTable->ftrSVM = true;
    pSkuTable->ftrIA32eGfxPTEs = true;
    pSkuTable->ftrDisplayYTiling = true;
    pSkuTable->ftrTranslationTable = true;
    pSkuTable->ftrUserModeTranslationTable = true;
    pSkuTable->ftrEnableGuC = true;
    pSkuTable->ftrFbc = true;
    pSkuTable->ftrFbc2AddressTranslation = true;
    pSkuTable->ftrFbcBlitterTracking = true;
    pSkuTable->ftrFbcCpuTracking = true;
    pSkuTable->ftrTileY = true;

    pWaTable->waEnablePreemptionGranularityControlByUMD = true;
    pWaTable->waSendMIFLUSHBeforeVFE = true;
    pWaTable->waReportPerfCountUseGlobalContextID = true;
    pWaTable->waMsaa8xTileYDepthPitchAlignment = true;
    pWaTable->waLosslessCompressionSurfaceStride = true;
    pWaTable->waFbcLinearSurfaceStride = true;
    pWaTable->wa4kAlignUVOffsetNV12LinearSurface = true;
    pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;

    if (pPlatform->usRevId <= 0x6) {
        pWaTable->waDisableLSQCROPERFforOCL = true;
        pWaTable->waEncryptedEdramOnlyPartials = true;
    }
    if (pPlatform->usRevId <= 0x8) {
        pWaTable->waForcePcBbFullCfgRestore = true;
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
    GT_SYSTEM_INFO *gtSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    gtSysInfo->EUCount = 11;
    gtSysInfo->ThreadCount = 11 * KBL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 2;
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
    GT_SYSTEM_INFO *gtSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    gtSysInfo->EUCount = 17;
    gtSysInfo->ThreadCount = 17 * KBL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 3;
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
    GT_SYSTEM_INFO *gtSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    gtSysInfo->EUCount = 23;
    gtSysInfo->ThreadCount = 23 * KBL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 3;
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
    GT_SYSTEM_INFO *gtSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    gtSysInfo->EUCount = 47;
    gtSysInfo->ThreadCount = 47 * KBL::threadsPerEu;
    gtSysInfo->SliceCount = 2;
    gtSysInfo->SubSliceCount = 6;
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
    GT_SYSTEM_INFO *gtSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    gtSysInfo->EUCount = 71;
    gtSysInfo->ThreadCount = 71 * KBL::threadsPerEu;
    gtSysInfo->SliceCount = 3;
    gtSysInfo->SubSliceCount = 9;
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

void setupKBLHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const std::string &hwInfoConfig) {
    if (hwInfoConfig == "1x3x8") {
        KBL_1x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "2x3x8") {
        KBL_2x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "3x3x8") {
        KBL_3x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "1x2x6") {
        KBL_1x2x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "1x3x6") {
        KBL_1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "default") {
        // Default config
        KBL_1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*KBL::setupHardwareInfo)(HardwareInfo *, bool, const std::string &) = setupKBLHardwareInfoImpl;
} // namespace NEO
