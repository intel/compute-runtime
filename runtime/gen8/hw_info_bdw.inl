/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/memory_constants.h"
#include "runtime/aub_mem_dump/aub_services.h"
#include "runtime/gen8/hw_cmds.h"

#include "engine_node.h"
#include "hw_info.h"

namespace NEO {

const char *HwMapper<IGFX_BROADWELL>::abbreviation = "bdw";

bool isSimulationBDW(unsigned short deviceId) {
    switch (deviceId) {
    case IBDW_GT0_DESK_DEVICE_F0_ID:
    case IBDW_GT1_DESK_DEVICE_F0_ID:
    case IBDW_GT2_DESK_DEVICE_F0_ID:
    case IBDW_GT3_DESK_DEVICE_F0_ID:
    case IBDW_GT4_DESK_DEVICE_F0_ID:
        return true;
    }
    return false;
};

const PLATFORM BDW::platform = {
    IGFX_BROADWELL,
    PCH_UNKNOWN,
    IGFX_GEN8_CORE,
    IGFX_GEN8_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable BDW::capabilityTable{
    {50000, 5000, 200000, true, true, true},       // kmdNotifyProperties
    MemoryConstants::max48BitAddress,              // gpuAddressSpace
    80,                                            // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                     // requiredPreemptionSurfaceSize
    &isSimulationBDW,                              // isSimulation
    PreemptionMode::Disabled,                      // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                        // defaultEngineType
    0,                                             // maxRenderFrequency
    21,                                            // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Bdw, // aubDeviceId
    0,                                             // extraQuantityThreadsPerEU
    64,                                            // slmSize
    false,                                         // blitterOperationsSupported
    true,                                          // ftrSupportsInteger64BitAtomics
    true,                                          // ftrSupportsFP64
    true,                                          // ftrSupports64BitMath
    true,                                          // ftrSvm
    true,                                          // ftrSupportsCoherency
    false,                                         // ftrSupportsVmeAvcTextureSampler
    false,                                         // ftrSupportsVmeAvcPreemption
    false,                                         // ftrRenderCompressedBuffers
    false,                                         // ftrRenderCompressedImages
    false,                                         // ftr64KBpages
    true,                                          // instrumentationEnabled
    true,                                          // forceStatelessCompilationFor32Bit
    "core",                                        // platformType
    false,                                         // sourceLevelDebuggerSupported
    false,                                         // supportsVme
    false,                                         // supportCacheFlushAfterWalker
    true,                                          // supportsImages
    true                                           // supportsDeviceEnqueue
};

WorkaroundTable BDW::workaroundTable = {};
FeatureTable BDW::featureTable = {};

void BDW::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->ftrL3IACoherency = true;
    featureTable->ftrPPGTT = true;
    featureTable->ftrSVM = true;
    featureTable->ftrIA32eGfxPTEs = true;
    featureTable->ftrFbc = true;
    featureTable->ftrFbc2AddressTranslation = true;
    featureTable->ftrFbcBlitterTracking = true;
    featureTable->ftrFbcCpuTracking = true;
    featureTable->ftrTileY = true;

    workaroundTable->waDisableLSQCROPERFforOCL = true;
    workaroundTable->waReportPerfCountUseGlobalContextID = true;
    workaroundTable->waUseVAlign16OnTileXYBpp816 = true;
    workaroundTable->waModifyVFEStateAfterGPGPUPreemption = true;
    workaroundTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;
}

const HardwareInfo BDW_1x2x6::hwInfo = {
    &BDW::platform,
    &BDW::featureTable,
    &BDW::workaroundTable,
    &BDW_1x2x6::gtSystemInfo,
    BDW::capabilityTable,
};

GT_SYSTEM_INFO BDW_1x2x6::gtSystemInfo = {0};
void BDW_1x2x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->EUCount = 12;
    gtSysInfo->ThreadCount = 12 * BDW::threadsPerEu;
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
    gtSysInfo->MaxEuPerSubSlice = BDW::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = BDW::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = BDW::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo BDW_1x3x6::hwInfo = {
    &BDW::platform,
    &BDW::featureTable,
    &BDW::workaroundTable,
    &BDW_1x3x6::gtSystemInfo,
    BDW::capabilityTable,
};
GT_SYSTEM_INFO BDW_1x3x6::gtSystemInfo = {0};
void BDW_1x3x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->EUCount = 18;
    gtSysInfo->ThreadCount = 18 * BDW::threadsPerEu;
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
    gtSysInfo->MaxEuPerSubSlice = BDW::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = BDW::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = BDW::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo BDW_1x3x8::hwInfo = {
    &BDW::platform,
    &BDW::featureTable,
    &BDW::workaroundTable,
    &BDW_1x3x8::gtSystemInfo,
    BDW::capabilityTable,
};
GT_SYSTEM_INFO BDW_1x3x8::gtSystemInfo = {0};
void BDW_1x3x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->EUCount = 23;
    gtSysInfo->ThreadCount = 23 * BDW::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 3;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 2;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = BDW::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = BDW::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = BDW::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo BDW_2x3x8::hwInfo = {
    &BDW::platform,
    &BDW::featureTable,
    &BDW::workaroundTable,
    &BDW_2x3x8::gtSystemInfo,
    BDW::capabilityTable,
};
GT_SYSTEM_INFO BDW_2x3x8::gtSystemInfo = {0};
void BDW_2x3x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->EUCount = 47;
    gtSysInfo->ThreadCount = 47 * BDW::threadsPerEu;
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
    gtSysInfo->MaxEuPerSubSlice = BDW::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = BDW::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = BDW::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo BDW::hwInfo = BDW_1x3x8::hwInfo;

void setupBDWHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const std::string &hwInfoConfig) {
    if (hwInfoConfig == "2x3x8") {
        BDW_2x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "1x3x8") {
        BDW_1x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "1x3x6") {
        BDW_1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "1x2x6") {
        BDW_1x2x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "default") {
        // Default config
        BDW_1x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*BDW::setupHardwareInfo)(HardwareInfo *, bool, const std::string &) = setupBDWHardwareInfoImpl;
} // namespace NEO
