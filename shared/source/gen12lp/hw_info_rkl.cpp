/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/gen12lp/hw_cmds_rkl.h"
#include "shared/source/helpers/constants.h"

#include "engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_ROCKETLAKE>::abbreviation = "rkl";

bool isSimulationRKL(unsigned short deviceId) {
    return false;
};

const PLATFORM RKL::platform = {
    IGFX_ROCKETLAKE,
    PCH_UNKNOWN,
    IGFX_GEN12LP_CORE,
    IGFX_GEN12LP_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable RKL::capabilityTable{
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_RCS, {true, true}},
        {aub_stream::ENGINE_CCS, {true, true}}},     // directSubmissionEngines
    {0, 0, 0, 0, false, false, false, false},        // kmdNotifyProperties
    MemoryConstants::max48BitAddress,                // gpuAddressSpace
    0,                                               // sharedSystemMemCapabilities
    83.333,                                          // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                       // requiredPreemptionSurfaceSize
    &isSimulationRKL,                                // isSimulation
    "lp",                                            // platformType
    "",                                              // deviceName
    PreemptionMode::MidThread,                       // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                          // defaultEngineType
    0,                                               // maxRenderFrequency
    30,                                              // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Tgllp, // aubDeviceId
    1,                                               // extraQuantityThreadsPerEU
    64,                                              // slmSize
    sizeof(RKL::GRF),                                // grfSize
    36u,                                             // timestampValidBits
    32u,                                             // kernelTimestampValidBits
    false,                                           // blitterOperationsSupported
    true,                                            // ftrSupportsInteger64BitAtomics
    false,                                           // ftrSupportsFP64
    false,                                           // ftrSupports64BitMath
    true,                                            // ftrSvm
    false,                                           // ftrSupportsCoherency
    false,                                           // ftrSupportsVmeAvcTextureSampler
    false,                                           // ftrSupportsVmeAvcPreemption
    false,                                           // ftrRenderCompressedBuffers
    false,                                           // ftrRenderCompressedImages
    true,                                            // ftr64KBpages
    true,                                            // instrumentationEnabled
    true,                                            // sourceLevelDebuggerSupported
    false,                                           // supportsVme
    false,                                           // supportCacheFlushAfterWalker
    true,                                            // supportsImages
    false,                                           // supportsDeviceEnqueue
    false,                                           // supportsPipes
    true,                                            // supportsOcl21Features
    false,                                           // supportsOnDemandPageFaults
    false,                                           // supportsIndependentForwardProgress
    false,                                           // hostPtrTrackingEnabled
    true,                                            // levelZeroSupported
    true,                                            // isIntegratedDevice
    true,                                            // supportsMediaBlock
    false,                                           // p2pAccessSupported
    false,                                           // p2pAtomicAccessSupported
    true,                                            // fusedEuEnabled
    false,                                           // l0DebuggerSupported;
};

WorkaroundTable RKL::workaroundTable = {};
FeatureTable RKL::featureTable = {};

void RKL::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
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
    featureTable->flags.ftrEnableGuC = true;

    featureTable->flags.ftrFbc = true;
    featureTable->flags.ftrFbc2AddressTranslation = true;
    featureTable->flags.ftrFbcBlitterTracking = true;
    featureTable->flags.ftrFbcCpuTracking = true;
    featureTable->flags.ftrTileY = true;

    featureTable->flags.ftrAstcHdr2D = true;
    featureTable->flags.ftrAstcLdr2D = true;

    featureTable->flags.ftr3dMidBatchPreempt = true;
    featureTable->flags.ftrGpGpuMidBatchPreempt = true;
    featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = true;
    featureTable->flags.ftrPerCtxtPreemptionGranularityControl = true;

    workaroundTable->flags.wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->flags.waEnablePreemptionGranularityControlByUMD = true;
};

void RKL::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * RKL::threadsPerEu;
    gtSysInfo->TotalVsThreads = 0;
    gtSysInfo->TotalHsThreads = 0;
    gtSysInfo->TotalDsThreads = 0;
    gtSysInfo->TotalGsThreads = 0;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = RKL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = RKL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = 2;
    gtSysInfo->MaxDualSubSlicesSupported = 2;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
}

const HardwareInfo RklHwConfig::hwInfo = {
    &RKL::platform,
    &RKL::featureTable,
    &RKL::workaroundTable,
    &RklHwConfig::gtSystemInfo,
    RKL::capabilityTable,
};

GT_SYSTEM_INFO RklHwConfig::gtSystemInfo = {0};
void RklHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    RKL::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->DualSubSliceCount = gtSysInfo->SubSliceCount;
    gtSysInfo->L3CacheSizeInKb = 1920;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 8;

    gtSysInfo->CCSInfo.IsValid = true;
    gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;
    gtSysInfo->CCSInfo.Instances.CCSEnableMask = 0b1;
};

const HardwareInfo RKL::hwInfo = RklHwConfig::hwInfo;
const uint64_t RKL::defaultHardwareInfoConfig = 0x100020010;

void setupRKLHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    RklHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
}

void (*RKL::setupHardwareInfo)(HardwareInfo *, bool, const uint64_t) = setupRKLHardwareInfoImpl;
} // namespace NEO
