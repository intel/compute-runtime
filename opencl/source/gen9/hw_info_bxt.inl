/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen9/hw_cmds.h"
#include "shared/source/memory_manager/memory_constants.h"
#include "opencl/source/aub_mem_dump/aub_services.h"

#include "engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_BROXTON>::abbreviation = "bxt";

bool isSimulationBXT(unsigned short deviceId) {
    switch (deviceId) {
    case IBXT_A_DEVICE_F0_ID:
    case IBXT_C_DEVICE_F0_ID:
        return true;
    }
    return false;
};

const PLATFORM BXT::platform = {
    IGFX_BROXTON,
    PCH_UNKNOWN,
    IGFX_GEN9_CORE,
    IGFX_GEN9_CORE,
    PLATFORM_MOBILE, // default init
    0,               // usDeviceID
    0,               // usRevId. 0 sets the stepping to A0
    0,               // usDeviceID_PCH
    0,               // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable BXT::capabilityTable{
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_RCS, {true, true}}},   // directSubmissionEngines
    {0, 0, 0, false, false, false},                // kmdNotifyProperties
    MemoryConstants::max48BitAddress,              // gpuAddressSpace
    52.083,                                        // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                     // requiredPreemptionSurfaceSize
    &isSimulationBXT,                              // isSimulation
    PreemptionMode::MidThread,                     // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                        // defaultEngineType
    0,                                             // maxRenderFrequency
    12,                                            // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Bxt, // aubDeviceId
    0,                                             // extraQuantityThreadsPerEU
    64,                                            // slmSize
    sizeof(BXT::GRF),                              // grfSize
    false,                                         // blitterOperationsSupported
    false,                                         // ftrSupportsInteger64BitAtomics
    true,                                          // ftrSupportsFP64
    true,                                          // ftrSupports64BitMath
    false,                                         // ftrSvm
    true,                                          // ftrSupportsCoherency
    true,                                          // ftrSupportsVmeAvcTextureSampler
    false,                                         // ftrSupportsVmeAvcPreemption
    false,                                         // ftrRenderCompressedBuffers
    false,                                         // ftrRenderCompressedImages
    false,                                         // ftr64KBpages
    true,                                          // instrumentationEnabled
    false,                                         // forceStatelessCompilationFor32Bit
    "lp",                                          // platformType
    true,                                          // sourceLevelDebuggerSupported
    true,                                          // supportsVme
    false,                                         // supportCacheFlushAfterWalker
    true,                                          // supportsImages
    false,                                         // supportsDeviceEnqueue
    true                                           // hostPtrTrackingEnabled
};

WorkaroundTable BXT::workaroundTable = {};
FeatureTable BXT::featureTable = {};

void BXT::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    PLATFORM *platform = &hwInfo->platform;
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->ftrGpGpuMidBatchPreempt = true;
    featureTable->ftrGpGpuThreadGroupLevelPreempt = true;
    featureTable->ftrL3IACoherency = true;
    featureTable->ftrVEBOX = true;
    featureTable->ftrULT = true;
    featureTable->ftrGpGpuMidThreadLevelPreempt = true;
    featureTable->ftr3dMidBatchPreempt = true;
    featureTable->ftr3dObjectLevelPreempt = true;
    featureTable->ftrPerCtxtPreemptionGranularityControl = true;
    featureTable->ftrLCIA = true;
    featureTable->ftrPPGTT = true;
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

    if (platform->usRevId >= 3) {
        featureTable->ftrGttCacheInvalidation = true;
    }

    workaroundTable->waLLCCachingUnsupported = true;
    workaroundTable->waMsaa8xTileYDepthPitchAlignment = true;
    workaroundTable->waFbcLinearSurfaceStride = true;
    workaroundTable->wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->waEnablePreemptionGranularityControlByUMD = true;
    workaroundTable->waSendMIFLUSHBeforeVFE = true;
    workaroundTable->waForcePcBbFullCfgRestore = true;
    workaroundTable->waReportPerfCountUseGlobalContextID = true;
    workaroundTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;
}

const HardwareInfo BXT_1x2x6::hwInfo = {
    &BXT::platform,
    &BXT::featureTable,
    &BXT::workaroundTable,
    &BXT_1x2x6::gtSystemInfo,
    BXT::capabilityTable,
};
GT_SYSTEM_INFO BXT_1x2x6::gtSystemInfo = {0};
void BXT_1x2x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * BXT::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 1;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 112;
    gtSysInfo->TotalHsThreads = 112;
    gtSysInfo->TotalDsThreads = 112;
    gtSysInfo->TotalGsThreads = 112;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = BXT::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = BXT::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = BXT::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo BXT_1x3x6::hwInfo = {
    &BXT::platform,
    &BXT::featureTable,
    &BXT::workaroundTable,
    &BXT_1x3x6::gtSystemInfo,
    BXT::capabilityTable,
};
GT_SYSTEM_INFO BXT_1x3x6::gtSystemInfo = {0};
void BXT_1x3x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * BXT::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 1;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 112;
    gtSysInfo->TotalHsThreads = 112;
    gtSysInfo->TotalDsThreads = 112;
    gtSysInfo->TotalGsThreads = 112;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = BXT::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = BXT::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = BXT::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo BXT::hwInfo = BXT_1x3x6::hwInfo;
const uint64_t BXT::defaultHardwareInfoConfig = 0x100030006;

void setupBXTHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    if (hwInfoConfig == 0x100020006) {
        BXT_1x2x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100030006) {
        BXT_1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x0) {
        // Default config
        BXT_1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*BXT::setupHardwareInfo)(HardwareInfo *, bool, uint64_t) = setupBXTHardwareInfoImpl;
} // namespace NEO
