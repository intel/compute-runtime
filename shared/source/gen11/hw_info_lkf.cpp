/*
 * Copyright (C) 2019-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/gen11/hw_cmds_lkf.h"
#include "shared/source/helpers/constants.h"

#include "engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_LAKEFIELD>::abbreviation = "lkf";

bool isSimulationLKF(unsigned short deviceId) {
    switch (deviceId) {
    case ILKF_1x8x8_DESK_DEVICE_F0_ID:
        return true;
    }
    return false;
};
const PLATFORM LKF::platform = {
    IGFX_LAKEFIELD,
    PCH_UNKNOWN,
    IGFX_GEN11_CORE,
    IGFX_GEN11_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable LKF::capabilityTable{
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_RCS, {true, true}}},   // directSubmissionEngines
    {0, 0, 0, 0, false, false, false, false},      // kmdNotifyProperties
    MemoryConstants::max36BitAddress,              // gpuAddressSpace
    0,                                             // sharedSystemMemCapabilities
    83.333,                                        // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                     // requiredPreemptionSurfaceSize
    &isSimulationLKF,                              // isSimulation
    "lp",                                          // platformType
    "",                                            // deviceName
    PreemptionMode::MidThread,                     // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                        // defaultEngineType
    0,                                             // maxRenderFrequency
    30,                                            // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Lkf, // aubDeviceId
    1,                                             // extraQuantityThreadsPerEU
    64,                                            // slmSize
    sizeof(LKF::GRF),                              // grfSize
    36u,                                           // timestampValidBits
    32u,                                           // kernelTimestampValidBits
    false,                                         // blitterOperationsSupported
    false,                                         // ftrSupportsInteger64BitAtomics
    false,                                         // ftrSupportsFP64
    false,                                         // ftrSupports64BitMath
    false,                                         // ftrSvm
    false,                                         // ftrSupportsCoherency
    false,                                         // ftrSupportsVmeAvcTextureSampler
    false,                                         // ftrSupportsVmeAvcPreemption
    false,                                         // ftrRenderCompressedBuffers
    false,                                         // ftrRenderCompressedImages
    true,                                          // ftr64KBpages
    true,                                          // instrumentationEnabled
    true,                                          // sourceLevelDebuggerSupported
    false,                                         // supportsVme
    false,                                         // supportCacheFlushAfterWalker
    true,                                          // supportsImages
    false,                                         // supportsDeviceEnqueue
    false,                                         // supportsPipes
    false,                                         // supportsOcl21Features
    false,                                         // supportsOnDemandPageFaults
    false,                                         // supportsIndependentForwardProgress
    true,                                          // hostPtrTrackingEnabled
    false,                                         // levelZeroSupported
    true,                                          // isIntegratedDevice
    true,                                          // supportsMediaBlock
    false,                                         // p2pAccessSupported
    false,                                         // p2pAtomicAccessSupported
    false                                          // fusedEuEnabled
};

WorkaroundTable LKF::workaroundTable = {};
FeatureTable LKF::featureTable = {};

void LKF::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
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

const HardwareInfo LKF_1x8x8::hwInfo = {
    &LKF::platform,
    &LKF::featureTable,
    &LKF::workaroundTable,
    &LKF_1x8x8::gtSystemInfo,
    LKF::capabilityTable,
};
GT_SYSTEM_INFO LKF_1x8x8::gtSystemInfo = {0};
void LKF_1x8x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * LKF::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 2560;
    gtSysInfo->L3BankCount = 8;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 448;
    gtSysInfo->TotalHsThreads = 448;
    gtSysInfo->TotalDsThreads = 448;
    gtSysInfo->TotalGsThreads = 448;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = LKF::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = LKF::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = LKF::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo LKF::hwInfo = LKF_1x8x8::hwInfo;
const uint64_t LKF::defaultHardwareInfoConfig = 0x100080008;

void setupLKFHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    if (hwInfoConfig == 0x100080008) {
        LKF_1x8x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x0) {
        // Default config
        LKF_1x8x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*LKF::setupHardwareInfo)(HardwareInfo *, bool, uint64_t) = setupLKFHardwareInfoImpl;
} // namespace NEO
