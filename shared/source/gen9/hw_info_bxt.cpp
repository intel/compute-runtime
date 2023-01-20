/*
 * Copyright (C) 2018-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/gen9/hw_cmds_bxt.h"
#include "shared/source/helpers/constants.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_BROXTON>::abbreviation = "bxt";

const PLATFORM BXT::platform = {
    IGFX_BROXTON,
    PCH_UNKNOWN,
    IGFX_GEN9_CORE,
    IGFX_GEN9_CORE,
    PLATFORM_MOBILE, // default init
    0,               // usDeviceID
    0,               // usRevId. 0 sets the stepping to A0
    0,               // usDeviceID_PCH
    0,               // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable BXT::capabilityTable{
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_RCS, {true, true}}},   // directSubmissionEngines
    {0, 0, 0, 0, false, false, false, false},      // kmdNotifyProperties
    MemoryConstants::max48BitAddress,              // gpuAddressSpace
    0,                                             // sharedSystemMemCapabilities
    52.083,                                        // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                     // requiredPreemptionSurfaceSize
    "",                                            // deviceName
    PreemptionMode::MidThread,                     // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                        // defaultEngineType
    0,                                             // maxRenderFrequency
    30,                                            // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Bxt, // aubDeviceId
    0,                                             // extraQuantityThreadsPerEU
    64,                                            // slmSize
    sizeof(BXT::GRF),                              // grfSize
    36u,                                           // timestampValidBits
    32u,                                           // kernelTimestampValidBits
    false,                                         // blitterOperationsSupported
    false,                                         // ftrSupportsInteger64BitAtomics
    true,                                          // ftrSupportsFP64
    true,                                          // ftrSupports64BitMath
    false,                                         // ftrSvm
    true,                                          // ftrSupportsCoherency
    true,                                          // ftrSupportsVmeAvcTextureSampler
    false,                                         // ftrSupportsVmeAvcPreemption
    false,                                         // ftrRenderCompressedBuffers
    false,                                         // ftrRenderCompressedImages
    false,                                         // ftr64KBpages
    true,                                          // instrumentationEnabled
    true,                                          // sourceLevelDebuggerSupported
    true,                                          // supportsVme
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
    false                                          // l0DebuggerSupported;
};

WorkaroundTable BXT::workaroundTable = {};
FeatureTable BXT::featureTable = {};

void BXT::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->flags.ftrGpGpuMidBatchPreempt = true;
    featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = true;
    featureTable->flags.ftrL3IACoherency = true;
    featureTable->flags.ftrULT = true;
    featureTable->flags.ftrGpGpuMidThreadLevelPreempt = true;
    featureTable->flags.ftrLCIA = true;
    featureTable->flags.ftrPPGTT = true;
    featureTable->flags.ftrIA32eGfxPTEs = true;
    featureTable->flags.ftrDisplayYTiling = true;
    featureTable->flags.ftrTranslationTable = true;
    featureTable->flags.ftrUserModeTranslationTable = true;
    featureTable->flags.ftrFbc = true;
    featureTable->flags.ftrTileY = true;

    workaroundTable->flags.waLLCCachingUnsupported = true;
    workaroundTable->flags.waMsaa8xTileYDepthPitchAlignment = true;
    workaroundTable->flags.waFbcLinearSurfaceStride = true;
    workaroundTable->flags.wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->flags.waSendMIFLUSHBeforeVFE = true;
    workaroundTable->flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;
}

void BXT::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * BXT::threadsPerEu;
    gtSysInfo->TotalVsThreads = 112;
    gtSysInfo->TotalHsThreads = 112;
    gtSysInfo->TotalDsThreads = 112;
    gtSysInfo->TotalGsThreads = 112;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = BXT::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = BXT::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = BXT::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
}

const HardwareInfo BxtHw1x2x6::hwInfo = {
    &BXT::platform,
    &BXT::featureTable,
    &BXT::workaroundTable,
    &BxtHw1x2x6::gtSystemInfo,
    BXT::capabilityTable,
};
GT_SYSTEM_INFO BxtHw1x2x6::gtSystemInfo = {0};
void BxtHw1x2x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    BXT::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 1;
    gtSysInfo->MaxFillRate = 8;
};

const HardwareInfo BxtHw1x3x6::hwInfo = {
    &BXT::platform,
    &BXT::featureTable,
    &BXT::workaroundTable,
    &BxtHw1x3x6::gtSystemInfo,
    BXT::capabilityTable,
};
GT_SYSTEM_INFO BxtHw1x3x6::gtSystemInfo = {0};
void BxtHw1x3x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    BXT::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 1;
    gtSysInfo->MaxFillRate = 8;
};

const HardwareInfo BXT::hwInfo = BxtHw1x3x6::hwInfo;
const uint64_t BXT::defaultHardwareInfoConfig = 0x100030006;

void setupBXTHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    if (hwInfoConfig == 0x100020006) {
        BxtHw1x2x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100030006) {
        BxtHw1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x0) {
        // Default config
        BxtHw1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*BXT::setupHardwareInfo)(HardwareInfo *, bool, uint64_t) = setupBXTHardwareInfoImpl;
} // namespace NEO
