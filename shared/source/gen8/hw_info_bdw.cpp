/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/gen8/hw_cmds.h"
#include "shared/source/helpers/constants.h"

#include "engine_node.h"

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
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_RCS, {true, true}}},       // directSubmissionEngines
    {50000, 5000, 200000, 0, true, true, true, false}, // kmdNotifyProperties
    MemoryConstants::max48BitAddress,                  // gpuAddressSpace
    0,                                                 // sharedSystemMemCapabilities
    80,                                                // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                         // requiredPreemptionSurfaceSize
    &isSimulationBDW,                                  // isSimulation
    "core",                                            // platformType
    "",                                                // deviceName
    PreemptionMode::Disabled,                          // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                            // defaultEngineType
    0,                                                 // maxRenderFrequency
    30,                                                // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Bdw,     // aubDeviceId
    0,                                                 // extraQuantityThreadsPerEU
    64,                                                // slmSize
    sizeof(BDW::GRF),                                  // grfSize
    36u,                                               // timestampValidBits
    32u,                                               // kernelTimestampValidBits
    false,                                             // blitterOperationsSupported
    true,                                              // ftrSupportsInteger64BitAtomics
    true,                                              // ftrSupportsFP64
    true,                                              // ftrSupports64BitMath
    true,                                              // ftrSvm
    true,                                              // ftrSupportsCoherency
    false,                                             // ftrSupportsVmeAvcTextureSampler
    false,                                             // ftrSupportsVmeAvcPreemption
    false,                                             // ftrRenderCompressedBuffers
    false,                                             // ftrRenderCompressedImages
    false,                                             // ftr64KBpages
    true,                                              // instrumentationEnabled
    false,                                             // sourceLevelDebuggerSupported
    false,                                             // supportsVme
    false,                                             // supportCacheFlushAfterWalker
    true,                                              // supportsImages
    false,                                             // supportsDeviceEnqueue
    true,                                              // supportsPipes
    true,                                              // supportsOcl21Features
    false,                                             // supportsOnDemandPageFaults
    true,                                              // supportsIndependentForwardProgress
    true,                                              // hostPtrTrackingEnabled
    false,                                             // levelZeroSupported
    true,                                              // isIntegratedDevice
    true,                                              // supportsMediaBlock
    false,                                             // p2pAccessSupported
    false,                                             // p2pAtomicAccessSupported
    false                                              // fusedEuEnabled
};

WorkaroundTable BDW::workaroundTable = {};
FeatureTable BDW::featureTable = {};

void BDW::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->flags.ftrL3IACoherency = true;
    featureTable->flags.ftrPPGTT = true;
    featureTable->flags.ftrSVM = true;
    featureTable->flags.ftrIA32eGfxPTEs = true;
    featureTable->flags.ftrFbc = true;
    featureTable->flags.ftrFbc2AddressTranslation = true;
    featureTable->flags.ftrFbcBlitterTracking = true;
    featureTable->flags.ftrFbcCpuTracking = true;
    featureTable->flags.ftrTileY = true;

    workaroundTable->flags.waDisableLSQCROPERFforOCL = true;
    workaroundTable->flags.waReportPerfCountUseGlobalContextID = true;
    workaroundTable->flags.waUseVAlign16OnTileXYBpp816 = true;
    workaroundTable->flags.waModifyVFEStateAfterGPGPUPreemption = true;
    workaroundTable->flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;
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
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * BDW::threadsPerEu;
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
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * BDW::threadsPerEu;
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
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * BDW::threadsPerEu;
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
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * BDW::threadsPerEu;
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
const uint64_t BDW::defaultHardwareInfoConfig = 0x100030008;

void setupBDWHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    if (hwInfoConfig == 0x200030008) {
        BDW_2x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100030008) {
        BDW_1x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100030006) {
        BDW_1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100020006) {
        BDW_1x2x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x0) {
        // Default config
        BDW_1x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*BDW::setupHardwareInfo)(HardwareInfo *, bool, uint64_t) = setupBDWHardwareInfoImpl;
} // namespace NEO
