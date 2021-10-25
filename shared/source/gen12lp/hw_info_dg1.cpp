/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/gen12lp/hw_cmds_dg1.h"
#include "shared/source/helpers/constants.h"

#include "engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_DG1>::abbreviation = "dg1";

bool isSimulationDG1(unsigned short deviceId) {
    switch (deviceId) {
    case 0x4905:
    case 0x4906:
    case 0x4907:
        return true;
    }

    return false;
};

const PLATFORM DG1::platform = {
    IGFX_DG1,
    PCH_UNKNOWN,
    IGFX_GEN12LP_CORE,
    IGFX_GEN12LP_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable DG1::capabilityTable{
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_RCS, {true, true}},
        {aub_stream::ENGINE_CCS, {true, true}}},   // directSubmissionEngines
    {0, 0, 0, false, false, false},                // kmdNotifyProperties
    MemoryConstants::max64BitAppAddress,           // gpuAddressSpace
    83.333,                                        // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                     // requiredPreemptionSurfaceSize
    &isSimulationDG1,                              // isSimulation
    PreemptionMode::MidThread,                     // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                        // defaultEngineType
    0,                                             // maxRenderFrequency
    30,                                            // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Dg1, // aubDeviceId
    1,                                             // extraQuantityThreadsPerEU
    64,                                            // slmSize
    sizeof(DG1::GRF),                              // grfSize
    36u,                                           // timestampValidBits
    32u,                                           // kernelTimestampValidBits
    false,                                         // blitterOperationsSupported
    true,                                          // ftrSupportsInteger64BitAtomics
    false,                                         // ftrSupportsFP64
    false,                                         // ftrSupports64BitMath
    true,                                          // ftrSvm
    false,                                         // ftrSupportsCoherency
    false,                                         // ftrSupportsVmeAvcTextureSampler
    false,                                         // ftrSupportsVmeAvcPreemption
    false,                                         // ftrRenderCompressedBuffers
    false,                                         // ftrRenderCompressedImages
    true,                                          // ftr64KBpages
    true,                                          // instrumentationEnabled
    "lp",                                          // platformType
    "",                                            // deviceName
    true,                                          // sourceLevelDebuggerSupported
    false,                                         // supportsVme
    true,                                          // supportCacheFlushAfterWalker
    true,                                          // supportsImages,
    false,                                         // supportsDeviceEnqueue
    false,                                         // supportsPipes
    true,                                          // supportsOcl21Features
    false,                                         // supportsOnDemandPageFaults
    false,                                         // supportsIndependentForwardProgress
    false,                                         // hostPtrTrackingEnabled
    true,                                          // levelZeroSupported
    false,                                         // isIntegratedDevice
    true,                                          // supportsMediaBlock
    true                                           // fusedEuEnabled
};

WorkaroundTable DG1::workaroundTable = {};
FeatureTable DG1::featureTable = {};

void DG1::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->ftrL3IACoherency = true;
    featureTable->ftrPPGTT = true;
    featureTable->ftrSVM = true;
    featureTable->ftrIA32eGfxPTEs = true;
    featureTable->ftrStandardMipTailFormat = true;
    featureTable->ftrLocalMemory = true;

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
    featureTable->ftrGpGpuThreadGroupLevelPreempt = true;
    featureTable->ftrPerCtxtPreemptionGranularityControl = true;
    featureTable->ftrBcsInfo = maxNBitValue(1);

    workaroundTable->wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->waEnablePreemptionGranularityControlByUMD = true;
};

const HardwareInfo DG1_CONFIG::hwInfo = {
    &DG1::platform,
    &DG1::featureTable,
    &DG1::workaroundTable,
    &DG1_CONFIG::gtSystemInfo,
    DG1::capabilityTable,
};
GT_SYSTEM_INFO DG1_CONFIG::gtSystemInfo = {0};
void DG1_CONFIG::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * DG1::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->DualSubSliceCount = 6;
    gtSysInfo->L3CacheSizeInKb = 16384;
    gtSysInfo->L3BankCount = 8;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 672;
    gtSysInfo->TotalHsThreads = 672;
    gtSysInfo->TotalDsThreads = 672;
    gtSysInfo->TotalGsThreads = 672;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = DG1::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = DG1::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = DG1::maxSubslicesSupported;
    gtSysInfo->MaxDualSubSlicesSupported = DG1::maxDualSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    gtSysInfo->CCSInfo.IsValid = true;
    gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;
    gtSysInfo->CCSInfo.Instances.CCSEnableMask = 0b1;

    if (setupFeatureTableAndWorkaroundTable) {
        DG1::setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo DG1::hwInfo = DG1_CONFIG::hwInfo;
const uint64_t DG1::defaultHardwareInfoConfig = 0x100060010;

void setupDG1HardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    if (hwInfoConfig == 0x100060010) {
        DG1_CONFIG::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x0) {
        // Default config
        DG1_CONFIG::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*DG1::setupHardwareInfo)(HardwareInfo *, bool, const uint64_t) = setupDG1HardwareInfoImpl;
} // namespace NEO
