/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen11/hw_cmds.h"
#include "shared/source/memory_manager/memory_constants.h"
#include "opencl/source/aub_mem_dump/aub_services.h"

#include "engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_ELKHARTLAKE>::abbreviation = "ehl";

bool isSimulationEHL(unsigned short deviceId) {
    switch (deviceId) {
    case IEHL_1x4x8_SUPERSKU_DEVICE_A0_ID:
        return true;
    }
    return false;
};
const PLATFORM EHL::platform = {
    IGFX_ELKHARTLAKE,
    PCH_UNKNOWN,
    IGFX_GEN11_CORE,
    IGFX_GEN11_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable EHL::capabilityTable{
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_RCS, {true, true}}},   // directSubmissionEngines
    {0, 0, 0, false, false, false},                // kmdNotifyProperties
    MemoryConstants::max36BitAddress,              // gpuAddressSpace
    83.333,                                        // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                     // requiredPreemptionSurfaceSize
    &isSimulationEHL,                              // isSimulation
    PreemptionMode::MidThread,                     // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                        // defaultEngineType
    0,                                             // maxRenderFrequency
    12,                                            // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Ehl, // aubDeviceId
    1,                                             // extraQuantityThreadsPerEU
    64,                                            // slmSize
    sizeof(EHL::GRF),                              // grfSize
    false,                                         // blitterOperationsSupported
    false,                                         // ftrSupportsInteger64BitAtomics
    false,                                         // ftrSupportsFP64
    false,                                         // ftrSupports64BitMath
    false,                                         // ftrSvm
    true,                                          // ftrSupportsCoherency
    true,                                          // ftrSupportsVmeAvcTextureSampler
    true,                                          // ftrSupportsVmeAvcPreemption
    false,                                         // ftrRenderCompressedBuffers
    false,                                         // ftrRenderCompressedImages
    true,                                          // ftr64KBpages
    true,                                          // instrumentationEnabled
    true,                                          // forceStatelessCompilationFor32Bit
    "lp",                                          // platformType
    true,                                          // sourceLevelDebuggerSupported
    false,                                         // supportsVme
    false,                                         // supportCacheFlushAfterWalker
    true,                                          // supportsImages
    true,                                          // supportsDeviceEnqueue
    true                                           // hostPtrTrackingEnabled
};

WorkaroundTable EHL::workaroundTable = {};
FeatureTable EHL::featureTable = {};

void EHL::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->ftrL3IACoherency = true;
    featureTable->ftrPPGTT = true;
    featureTable->ftrSVM = true;
    featureTable->ftrIA32eGfxPTEs = true;
    featureTable->ftrStandardMipTailFormat = true;

    featureTable->ftrDisplayYTiling = true;
    featureTable->ftrTranslationTable = true;
    featureTable->ftrUserModeTranslationTable = true;
    featureTable->ftrTileMappedResource = true;
    featureTable->ftrEnableGuC = true;

    featureTable->ftrFbc = true;
    featureTable->ftrFbc2AddressTranslation = true;
    featureTable->ftrFbcBlitterTracking = true;
    featureTable->ftrFbcCpuTracking = true;
    featureTable->ftrTileY = true;

    featureTable->ftrAstcHdr2D = true;
    featureTable->ftrAstcLdr2D = true;

    featureTable->ftr3dMidBatchPreempt = true;
    featureTable->ftrGpGpuMidBatchPreempt = true;
    featureTable->ftrGpGpuMidThreadLevelPreempt = true;
    featureTable->ftrGpGpuThreadGroupLevelPreempt = true;
    featureTable->ftrPerCtxtPreemptionGranularityControl = true;

    workaroundTable->wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->waReportPerfCountUseGlobalContextID = true;
};

const HardwareInfo EHL_1x2x4::hwInfo = {
    &EHL::platform,
    &EHL::featureTable,
    &EHL::workaroundTable,
    &EHL_1x2x4::gtSystemInfo,
    EHL::capabilityTable,
};
GT_SYSTEM_INFO EHL_1x2x4::gtSystemInfo = {0};
void EHL_1x2x4::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * EHL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 1280;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 56;
    gtSysInfo->TotalHsThreads = 56;
    gtSysInfo->TotalDsThreads = 56;
    gtSysInfo->TotalGsThreads = 56;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = EHL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = EHL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = EHL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo EHL_1x4x4::hwInfo = {
    &EHL::platform,
    &EHL::featureTable,
    &EHL::workaroundTable,
    &EHL_1x4x4::gtSystemInfo,
    EHL::capabilityTable,
};
GT_SYSTEM_INFO EHL_1x4x4::gtSystemInfo = {0};
void EHL_1x4x4::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * EHL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 1280;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 112;
    gtSysInfo->TotalHsThreads = 112;
    gtSysInfo->TotalDsThreads = 112;
    gtSysInfo->TotalGsThreads = 112;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = EHL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = EHL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = EHL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo EHL_1x4x8::hwInfo = {
    &EHL::platform,
    &EHL::featureTable,
    &EHL::workaroundTable,
    &EHL_1x4x8::gtSystemInfo,
    EHL::capabilityTable,
};
GT_SYSTEM_INFO EHL_1x4x8::gtSystemInfo = {0};
void EHL_1x4x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * EHL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 1280;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 224;
    gtSysInfo->TotalHsThreads = 224;
    gtSysInfo->TotalDsThreads = 224;
    gtSysInfo->TotalGsThreads = 224;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = EHL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = EHL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = EHL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo EHL_1x4x6::hwInfo = {
    &EHL::platform,
    &EHL::featureTable,
    &EHL::workaroundTable,
    &EHL_1x4x6::gtSystemInfo,
    EHL::capabilityTable,
};
GT_SYSTEM_INFO EHL_1x4x6::gtSystemInfo = {0};
void EHL_1x4x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * EHL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 1280;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 168;
    gtSysInfo->TotalHsThreads = 168;
    gtSysInfo->TotalDsThreads = 168;
    gtSysInfo->TotalGsThreads = 168;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = EHL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = EHL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = EHL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo EHL::hwInfo = EHL_1x4x8::hwInfo;
const uint64_t EHL::defaultHardwareInfoConfig = 0x100040008;

void setupEHLHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    if (hwInfoConfig == 0x100040008) {
        EHL_1x4x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100040006) {
        EHL_1x4x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100040004) {
        EHL_1x4x4::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100020004) {
        EHL_1x2x4::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x0) {
        // Default config
        EHL_1x4x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}
void (*EHL::setupHardwareInfo)(HardwareInfo *, bool, uint64_t) = setupEHLHardwareInfoImpl;
} // namespace NEO
