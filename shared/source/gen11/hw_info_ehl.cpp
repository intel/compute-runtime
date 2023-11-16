/*
 * Copyright (C) 2019-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/gen11/hw_cmds_ehl.h"
#include "shared/source/helpers/constants.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_ELKHARTLAKE>::abbreviation = "ehl";

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
    {0, 0, 0, 0, false, false, false, false},      // kmdNotifyProperties
    MemoryConstants::max36BitAddress,              // gpuAddressSpace
    0,                                             // sharedSystemMemCapabilities
    83.333,                                        // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                     // requiredPreemptionSurfaceSize
    "",                                            // deviceName
    nullptr,                                       // preferredPlatformName
    PreemptionMode::MidThread,                     // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                        // defaultEngineType
    0,                                             // maxRenderFrequency
    30,                                            // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Ehl, // aubDeviceId
    1,                                             // extraQuantityThreadsPerEU
    64,                                            // slmSize
    sizeof(EHL::GRF),                              // grfSize
    36u,                                           // timestampValidBits
    32u,                                           // kernelTimestampValidBits
    false,                                         // blitterOperationsSupported
    false,                                         // ftrSupportsInteger64BitAtomics
    false,                                         // ftrSupportsFP64
    false,                                         // ftrSupportsFP64Emulation
    false,                                         // ftrSupports64BitMath
    false,                                         // ftrSvm
    false,                                         // ftrSupportsCoherency
    false,                                         // ftrSupportsVmeAvcTextureSampler
    false,                                         // ftrSupportsVmeAvcPreemption
    false,                                         // ftrRenderCompressedBuffers
    false,                                         // ftrRenderCompressedImages
    true,                                          // ftr64KBpages
    true,                                          // instrumentationEnabled
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
    false,                                         // fusedEuEnabled
    false,                                         // l0DebuggerSupported;
    false                                          // supportsFloatAtomics
};

WorkaroundTable EHL::workaroundTable = {};
FeatureTable EHL::featureTable = {};

void EHL::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
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

    featureTable->flags.ftrFbc = true;
    featureTable->flags.ftrTileY = true;

    featureTable->flags.ftrAstcHdr2D = true;
    featureTable->flags.ftrAstcLdr2D = true;

    featureTable->flags.ftrGpGpuMidBatchPreempt = true;
    featureTable->flags.ftrGpGpuMidThreadLevelPreempt = true;
    featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = true;

    workaroundTable->flags.wa4kAlignUVOffsetNV12LinearSurface = true;
};

void EHL::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * 7u;
    gtSysInfo->TotalVsThreads = 0;
    gtSysInfo->TotalHsThreads = 0;
    gtSysInfo->TotalDsThreads = 0;
    gtSysInfo->TotalGsThreads = 0;
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
}

const HardwareInfo EhlHwConfig::hwInfo = {
    &EHL::platform,
    &EHL::featureTable,
    &EHL::workaroundTable,
    &EhlHwConfig::gtSystemInfo,
    EHL::capabilityTable};

GT_SYSTEM_INFO EhlHwConfig::gtSystemInfo = {0};
void EhlHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    EHL::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 1280;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 8;
};

const HardwareInfo EHL::hwInfo = EhlHwConfig::hwInfo;

void setupEHLHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    EhlHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}
void (*EHL::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupEHLHardwareInfoImpl;
} // namespace NEO
