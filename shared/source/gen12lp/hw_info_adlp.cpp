/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/helpers/constants.h"

#include "engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_ALDERLAKE_P>::abbreviation = "adlp";

bool isSimulationADLP(unsigned short deviceId) {
    return false;
};

const PLATFORM ADLP::platform = {
    IGFX_ALDERLAKE_P,
    PCH_UNKNOWN,
    IGFX_GEN12LP_CORE,
    IGFX_GEN12LP_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable ADLP::capabilityTable{
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_RCS, {true, true}},
        {aub_stream::ENGINE_CCS, {true, true}}},    // directSubmissionEngines
    {0, 0, 0, 0, false, false, false, false},       // kmdNotifyProperties
    MemoryConstants::max64BitAppAddress,            // gpuAddressSpace
    0,                                              // sharedSystemMemCapabilities
    83.333,                                         // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                      // requiredPreemptionSurfaceSize
    &isSimulationADLP,                              // isSimulation
    "lp",                                           // platformType
    "",                                             // deviceName
    PreemptionMode::MidThread,                      // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                         // defaultEngineType
    0,                                              // maxRenderFrequency
    30,                                             // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Adlp, // aubDeviceId
    1,                                              // extraQuantityThreadsPerEU
    64,                                             // slmSize
    sizeof(ADLP::GRF),                              // grfSize
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
    true,                                           // fusedEuEnabled
    false                                           // l0DebuggerSupported;
};

WorkaroundTable ADLP::workaroundTable = {};
FeatureTable ADLP::featureTable = {};

void ADLP::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
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
    featureTable->flags.ftrTileY = false;

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

void ADLP::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * ADLP::threadsPerEu;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = ADLP::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = ADLP::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = ADLP::maxSubslicesSupported;
    gtSysInfo->MaxDualSubSlicesSupported = ADLP::maxDualSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
}

const HardwareInfo AdlpHwConfig::hwInfo = {
    &ADLP::platform,
    &ADLP::featureTable,
    &ADLP::workaroundTable,
    &AdlpHwConfig::gtSystemInfo,
    ADLP::capabilityTable,
};

GT_SYSTEM_INFO AdlpHwConfig::gtSystemInfo = {0};
void AdlpHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->IsDynamicallyPopulated = false;

    // non-zero values for unit tests
    if (gtSysInfo->SliceCount == 0) {
        gtSysInfo->SliceCount = 2;
        gtSysInfo->SubSliceCount = 8;
        gtSysInfo->EUCount = 40;
        gtSysInfo->MaxEuPerSubSlice = ADLP::maxEuPerSubslice;
        gtSysInfo->MaxSlicesSupported = ADLP::maxSlicesSupported;
        gtSysInfo->MaxSubSlicesSupported = ADLP::maxSubslicesSupported;

        gtSysInfo->L3CacheSizeInKb = 1;
        gtSysInfo->L3BankCount = 1;

        gtSysInfo->CCSInfo.IsValid = true;
        gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;
    }

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};
#include "hw_info_setup_adlp.inl"
} // namespace NEO
