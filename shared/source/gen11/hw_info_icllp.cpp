/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/gen11/hw_cmds_icllp.h"
#include "shared/source/helpers/constants.h"

#include "engine_node.h"

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
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_RCS, {true, true}}},     // directSubmissionEngines
    {0, 0, 0, 0, false, false, false, false},        // kmdNotifyProperties
    MemoryConstants::max48BitAddress,                // gpuAddressSpace
    0,                                               // sharedSystemMemCapabilities
    83.333,                                          // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                       // requiredPreemptionSurfaceSize
    &isSimulationICLLP,                              // isSimulation
    "lp",                                            // platformType
    "",                                              // deviceName
    PreemptionMode::MidThread,                       // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                          // defaultEngineType
    0,                                               // maxRenderFrequency
    30,                                              // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Icllp, // aubDeviceId
    1,                                               // extraQuantityThreadsPerEU
    64,                                              // slmSize
    sizeof(ICLLP::GRF),                              // grfSize
    36u,                                             // timestampValidBits
    32u,                                             // kernelTimestampValidBits
    false,                                           // blitterOperationsSupported
    true,                                            // ftrSupportsInteger64BitAtomics
    false,                                           // ftrSupportsFP64
    false,                                           // ftrSupports64BitMath
    true,                                            // ftrSvm
    false,                                           // ftrSupportsCoherency
    true,                                            // ftrSupportsVmeAvcTextureSampler
    true,                                            // ftrSupportsVmeAvcPreemption
    false,                                           // ftrRenderCompressedBuffers
    false,                                           // ftrRenderCompressedImages
    false,                                           // ftr64KBpages
    true,                                            // instrumentationEnabled
    true,                                            // sourceLevelDebuggerSupported
    true,                                            // supportsVme
    false,                                           // supportCacheFlushAfterWalker
    true,                                            // supportsImages
    false,                                           // supportsDeviceEnqueue
    true,                                            // supportsPipes
    true,                                            // supportsOcl21Features
    false,                                           // supportsOnDemandPageFaults
    true,                                            // supportsIndependentForwardProgress
    true,                                            // hostPtrTrackingEnabled
    true,                                            // levelZeroSupported
    true,                                            // isIntegratedDevice
    true,                                            // supportsMediaBlock
    false,                                           // p2pAccessSupported
    false,                                           // p2pAtomicAccessSupported
    false,                                           // fusedEuEnabled
    false                                            // l0DebuggerSupported;
};

WorkaroundTable ICLLP::workaroundTable = {};
FeatureTable ICLLP::featureTable = {};

void ICLLP::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->flags.ftrL3IACoherency = true;
    featureTable->flags.ftrPPGTT = true;
    featureTable->flags.ftrSVM = true;
    featureTable->flags.ftrIA32eGfxPTEs = true;
    featureTable->flags.ftrStandardMipTailFormat = true;

    featureTable->flags.ftrDisplayYTiling = true;
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
    featureTable->flags.ftrGpGpuMidThreadLevelPreempt = true;
    featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = true;
    featureTable->flags.ftrPerCtxtPreemptionGranularityControl = true;

    workaroundTable->flags.wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->flags.waReportPerfCountUseGlobalContextID = true;
};

void ICLLP::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * ICLLP::threadsPerEu;
    gtSysInfo->TotalHsThreads = 224;
    gtSysInfo->TotalGsThreads = 224;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 128;
    gtSysInfo->CsrSizeInMb = 5;
    gtSysInfo->MaxEuPerSubSlice = ICLLP::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = ICLLP::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = ICLLP::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
}

const HardwareInfo IcllpHw1x8x8::hwInfo = {
    &ICLLP::platform,
    &ICLLP::featureTable,
    &ICLLP::workaroundTable,
    &IcllpHw1x8x8::gtSystemInfo,
    ICLLP::capabilityTable,
};

GT_SYSTEM_INFO IcllpHw1x8x8::gtSystemInfo = {0};
void IcllpHw1x8x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    ICLLP::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 3072;
    gtSysInfo->L3BankCount = 8;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
};

const HardwareInfo IcllpHw1x4x8::hwInfo = {
    &ICLLP::platform,
    &ICLLP::featureTable,
    &ICLLP::workaroundTable,
    &IcllpHw1x4x8::gtSystemInfo,
    ICLLP::capabilityTable,
};

GT_SYSTEM_INFO IcllpHw1x4x8::gtSystemInfo = {0};
void IcllpHw1x4x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    ICLLP::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 2304;
    gtSysInfo->L3BankCount = 6;
    gtSysInfo->MaxFillRate = 8;
};

const HardwareInfo IcllpHw1x6x8::hwInfo = {
    &ICLLP::platform,
    &ICLLP::featureTable,
    &ICLLP::workaroundTable,
    &IcllpHw1x6x8::gtSystemInfo,
    ICLLP::capabilityTable,
};

GT_SYSTEM_INFO IcllpHw1x6x8::gtSystemInfo = {0};
void IcllpHw1x6x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    ICLLP::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 2304;
    gtSysInfo->L3BankCount = 6;
    gtSysInfo->MaxFillRate = 8;
};

const HardwareInfo ICLLP::hwInfo = IcllpHw1x8x8::hwInfo;
const uint64_t ICLLP::defaultHardwareInfoConfig = 0x100080008;

void setupICLLPHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    if (hwInfoConfig == 0x100080008) {
        IcllpHw1x8x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100040008) {
        IcllpHw1x4x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100060008) {
        IcllpHw1x6x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x0) {
        // Default config
        IcllpHw1x8x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*ICLLP::setupHardwareInfo)(HardwareInfo *, bool, uint64_t) = setupICLLPHardwareInfoImpl;
} // namespace NEO
