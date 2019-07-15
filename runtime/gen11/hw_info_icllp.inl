/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub_mem_dump/aub_services.h"
#include "runtime/memory_manager/memory_constants.h"

#include "engine_node.h"
#include "hw_cmds.h"
#include "hw_info_icllp.h"

namespace NEO {

const char *HwMapper<IGFX_ICELAKE_LP>::abbreviation = "icllp";

bool isSimulationICLLP(unsigned short deviceId) {
    switch (deviceId) {
    case IICL_LP_GT1_MOB_DEVICE_F0_ID:
        return true;
    }
    return false;
};

const PLATFORM ICLLP::platform = {
    IGFX_ICELAKE_LP,
    PCH_UNKNOWN,
    IGFX_GEN11_CORE,
    IGFX_GEN11_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable ICLLP::capabilityTable{
    {0, 0, 0, false, false, false},                  // kmdNotifyProperties
    {true, false},                                   // whitelistedRegisters
    MemoryConstants::max48BitAddress,                // gpuAddressSpace
    83.333,                                          // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                       // requiredPreemptionSurfaceSize
    &isSimulationICLLP,                              // isSimulation
    PreemptionMode::MidThread,                       // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                          // defaultEngineType
    0,                                               // maxRenderFrequency
    21,                                              // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Icllp, // aubDeviceId
    1,                                               // extraQuantityThreadsPerEU
    64,                                              // slmSize
    false,                                           // blitterOperationsSupported
    false,                                           // ftrSupportsFP64
    false,                                           // ftrSupports64BitMath
    true,                                            // ftrSvm
    true,                                            // ftrSupportsCoherency
    true,                                            // ftrSupportsVmeAvcTextureSampler
    true,                                            // ftrSupportsVmeAvcPreemption
    false,                                           // ftrRenderCompressedBuffers
    false,                                           // ftrRenderCompressedImages
    false,                                           // ftr64KBpages
    true,                                            // instrumentationEnabled
    true,                                            // forceStatelessCompilationFor32Bit
    false,                                           // isCore
    true,                                            // sourceLevelDebuggerSupported
    true,                                            // supportsVme
    false,                                           // supportCacheFlushAfterWalker
    true,                                            // supportsImages
    true                                             // supportsDeviceEnqueue
};

WorkaroundTable ICLLP::workaroundTable = {};
FeatureTable ICLLP::featureTable = {};

void ICLLP::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
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

const HardwareInfo ICLLP_1x8x8::hwInfo = {
    &ICLLP::platform,
    &ICLLP::featureTable,
    &ICLLP::workaroundTable,
    &ICLLP_1x8x8::gtSystemInfo,
    ICLLP::capabilityTable,
};

GT_SYSTEM_INFO ICLLP_1x8x8::gtSystemInfo = {0};
void ICLLP_1x8x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->EUCount = 63;
    gtSysInfo->ThreadCount = 63 * ICLLP::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 8;
    gtSysInfo->L3CacheSizeInKb = 3072;
    gtSysInfo->L3BankCount = 8;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = ICLLP::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = ICLLP::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = ICLLP::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo ICLLP_1x4x8::hwInfo = {
    &ICLLP::platform,
    &ICLLP::featureTable,
    &ICLLP::workaroundTable,
    &ICLLP_1x4x8::gtSystemInfo,
    ICLLP::capabilityTable,
};

GT_SYSTEM_INFO ICLLP_1x4x8::gtSystemInfo = {0};
void ICLLP_1x4x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->EUCount = 31;
    gtSysInfo->ThreadCount = 31 * ICLLP::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 4;
    gtSysInfo->L3CacheSizeInKb = 2304;
    gtSysInfo->L3BankCount = 6;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 364;
    gtSysInfo->TotalHsThreads = 224;
    gtSysInfo->TotalDsThreads = 364;
    gtSysInfo->TotalGsThreads = 224;
    gtSysInfo->TotalPsThreadsWindowerRange = 128;
    gtSysInfo->CsrSizeInMb = 5;
    gtSysInfo->MaxEuPerSubSlice = ICLLP::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = 1;
    gtSysInfo->MaxSubSlicesSupported = 4;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};
const HardwareInfo ICLLP_1x6x8::hwInfo = {
    &ICLLP::platform,
    &ICLLP::featureTable,
    &ICLLP::workaroundTable,
    &ICLLP_1x6x8::gtSystemInfo,
    ICLLP::capabilityTable,
};

GT_SYSTEM_INFO ICLLP_1x6x8::gtSystemInfo = {0};
void ICLLP_1x6x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->EUCount = 47;
    gtSysInfo->ThreadCount = 47 * ICLLP::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 6;
    gtSysInfo->L3CacheSizeInKb = 2304;
    gtSysInfo->L3BankCount = 6;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 364;
    gtSysInfo->TotalHsThreads = 224;
    gtSysInfo->TotalDsThreads = 364;
    gtSysInfo->TotalGsThreads = 224;
    gtSysInfo->TotalPsThreadsWindowerRange = 128;
    gtSysInfo->CsrSizeInMb = 5;
    gtSysInfo->MaxEuPerSubSlice = ICLLP::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = 1;
    gtSysInfo->MaxSubSlicesSupported = 4;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo ICLLP_1x1x8::hwInfo = {
    &ICLLP::platform,
    &ICLLP::featureTable,
    &ICLLP::workaroundTable,
    &ICLLP_1x1x8::gtSystemInfo,
    ICLLP::capabilityTable,
};
GT_SYSTEM_INFO ICLLP_1x1x8::gtSystemInfo = {0};
void ICLLP_1x1x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->EUCount = 8;
    gtSysInfo->ThreadCount = 8 * ICLLP::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 2304;
    gtSysInfo->L3BankCount = 6;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 364;
    gtSysInfo->TotalHsThreads = 224;
    gtSysInfo->TotalDsThreads = 364;
    gtSysInfo->TotalGsThreads = 224;
    gtSysInfo->TotalPsThreadsWindowerRange = 128;
    gtSysInfo->CsrSizeInMb = 5;
    gtSysInfo->MaxEuPerSubSlice = ICLLP::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = 1;
    gtSysInfo->MaxSubSlicesSupported = 4;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo ICLLP::hwInfo = ICLLP_1x8x8::hwInfo;

void setupICLLPHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const std::string &hwInfoConfig) {
    if (hwInfoConfig == "1x8x8") {
        ICLLP_1x8x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "1x4x8") {
        ICLLP_1x4x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "1x6x8") {
        ICLLP_1x6x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "1x1x8") {
        ICLLP_1x1x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "default") {
        // Default config
        ICLLP_1x8x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*ICLLP::setupHardwareInfo)(HardwareInfo *, bool, const std::string &) = setupICLLPHardwareInfoImpl;
} // namespace NEO
