/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/constants.h"

#include "engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_ALDERLAKE_S>::abbreviation = "adls";

bool isSimulationADLS(unsigned short deviceId) {
    return false;
};

const PLATFORM ADLS::platform = {
    IGFX_ALDERLAKE_S,
    PCH_UNKNOWN,
    IGFX_GEN12LP_CORE,
    IGFX_GEN12LP_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable ADLS::capabilityTable{
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_RCS, {true, true}},
        {aub_stream::ENGINE_CCS, {true, true}}},    // directSubmissionEngines
    {0, 0, 0, 0, false, false, false, false},       // kmdNotifyProperties
    MemoryConstants::max64BitAppAddress,            // gpuAddressSpace
    0,                                              // sharedSystemMemCapabilities
    83.333,                                         // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                      // requiredPreemptionSurfaceSize
    &isSimulationADLS,                              // isSimulation
    "lp",                                           // platformType
    "",                                             // deviceName
    PreemptionMode::MidThread,                      // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                         // defaultEngineType
    0,                                              // maxRenderFrequency
    30,                                             // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Adls, // aubDeviceId
    1,                                              // extraQuantityThreadsPerEU
    64,                                             // slmSize
    sizeof(ADLS::GRF),                              // grfSize
    36u,                                            // timestampValidBits
    32u,                                            // kernelTimestampValidBits
    false,                                          // blitterOperationsSupported
    true,                                           // ftrSupportsInteger64BitAtomics
    false,                                          // ftrSupportsFP64
    false,                                          // ftrSupports64BitMath
    true,                                           // ftrSvm
    false,                                          // ftrSupportsCoherency
    false,                                          // ftrSupportsVmeAvcTextureSampler
    false,                                          // ftrSupportsVmeAvcPreemption
    false,                                          // ftrRenderCompressedBuffers
    false,                                          // ftrRenderCompressedImages
    true,                                           // instrumentationEnabled
    true,                                           // ftr64KBpages
    true,                                           // sourceLevelDebuggerSupported
    false,                                          // supportsVme
    false,                                          // supportCacheFlushAfterWalker
    true,                                           // supportsImages
    false,                                          // supportsDeviceEnqueue
    false,                                          // supportsPipes
    true,                                           // supportsOcl21Features
    false,                                          // supportsOnDemandPageFaults
    false,                                          // supportsIndependentForwardProgress
    false,                                          // hostPtrTrackingEnabled
    true,                                           // levelZeroSupported
    true,                                           // isIntegratedDevice
    true,                                           // supportsMediaBlock
    false,                                          // p2pAccessSupported
    false,                                          // p2pAtomicAccessSupported
    true                                            // fusedEuEnabled
};

WorkaroundTable ADLS::workaroundTable = {};
FeatureTable ADLS::featureTable = {};

void ADLS::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
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
    workaroundTable->flags.waUntypedBufferCompression = true;
};

const HardwareInfo ADLS_HW_CONFIG::hwInfo = {
    &ADLS::platform,
    &ADLS::featureTable,
    &ADLS::workaroundTable,
    &ADLS_HW_CONFIG::gtSystemInfo,
    ADLS::capabilityTable,
};

GT_SYSTEM_INFO ADLS_HW_CONFIG::gtSystemInfo = {0};
void ADLS_HW_CONFIG::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * ADLS::threadsPerEu;
    gtSysInfo->DualSubSliceCount = gtSysInfo->SubSliceCount;
    gtSysInfo->L3CacheSizeInKb = 1920;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 0;
    gtSysInfo->TotalHsThreads = 0;
    gtSysInfo->TotalDsThreads = 0;
    gtSysInfo->TotalGsThreads = 0;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = ADLS::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = ADLS::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = 1;
    gtSysInfo->MaxDualSubSlicesSupported = 2;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    gtSysInfo->CCSInfo.IsValid = true;
    gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;
    gtSysInfo->CCSInfo.Instances.CCSEnableMask = 0b1;

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo ADLS::hwInfo = ADLS_HW_CONFIG::hwInfo;
const uint64_t ADLS::defaultHardwareInfoConfig = 0x100020010;

void setupADLSHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    ADLS_HW_CONFIG::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
}

void (*ADLS::setupHardwareInfo)(HardwareInfo *, bool, const uint64_t) = setupADLSHardwareInfoImpl;
} // namespace NEO
