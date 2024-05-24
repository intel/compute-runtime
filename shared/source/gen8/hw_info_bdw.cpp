/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/gen8/hw_cmds_bdw.h"
#include "shared/source/helpers/constants.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_BROADWELL>::abbreviation = "bdw";

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
    "",                                                // deviceName
    nullptr,                                           // preferredPlatformName
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
    false,                                             // ftrSupportsFP64Emulation
    true,                                              // ftrSupports64BitMath
    true,                                              // ftrSvm
    true,                                              // ftrSupportsCoherency
    false,                                             // ftrSupportsVmeAvcTextureSampler
    false,                                             // ftrSupportsVmeAvcPreemption
    false,                                             // ftrRenderCompressedBuffers
    false,                                             // ftrRenderCompressedImages
    false,                                             // ftr64KBpages
    true,                                              // instrumentationEnabled
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
    false,                                             // fusedEuEnabled
    false,                                             // l0DebuggerSupported;
    false,                                             // supportsFloatAtomics
    0                                                  // cxlType
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
    featureTable->flags.ftrTileY = true;

    workaroundTable->flags.waDisableLSQCROPERFforOCL = true;
    workaroundTable->flags.waUseVAlign16OnTileXYBpp816 = true;
    workaroundTable->flags.waModifyVFEStateAfterGPGPUPreemption = true;
    workaroundTable->flags.waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;
}

void BDW::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * 7u;
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
}

const HardwareInfo BdwHw1x2x6::hwInfo = {
    &BDW::platform,
    &BDW::featureTable,
    &BDW::workaroundTable,
    &BdwHw1x2x6::gtSystemInfo,
    BDW::capabilityTable};

GT_SYSTEM_INFO BdwHw1x2x6::gtSystemInfo = {0};
void BdwHw1x2x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    BDW::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 2;
    gtSysInfo->MaxFillRate = 8;
};

const HardwareInfo BdwHw1x3x6::hwInfo = {
    &BDW::platform,
    &BDW::featureTable,
    &BDW::workaroundTable,
    &BdwHw1x3x6::gtSystemInfo,
    BDW::capabilityTable};
GT_SYSTEM_INFO BdwHw1x3x6::gtSystemInfo = {0};
void BdwHw1x3x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    BDW::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 768;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 8;
};

const HardwareInfo BdwHw1x3x8::hwInfo = {
    &BDW::platform,
    &BDW::featureTable,
    &BDW::workaroundTable,
    &BdwHw1x3x8::gtSystemInfo,
    BDW::capabilityTable};
GT_SYSTEM_INFO BdwHw1x3x8::gtSystemInfo = {0};
void BdwHw1x3x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    BDW::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 2;
    gtSysInfo->MaxFillRate = 8;
};

const HardwareInfo BdwHw2x3x8::hwInfo = {
    &BDW::platform,
    &BDW::featureTable,
    &BDW::workaroundTable,
    &BdwHw2x3x8::gtSystemInfo,
    BDW::capabilityTable};
GT_SYSTEM_INFO BdwHw2x3x8::gtSystemInfo = {0};
void BdwHw2x3x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    BDW::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->SliceCount = 2;
    gtSysInfo->L3CacheSizeInKb = 1536;
    gtSysInfo->L3BankCount = 8;
    gtSysInfo->MaxFillRate = 16;
};

const HardwareInfo BDW::hwInfo = BdwHw1x3x8::hwInfo;

void setupBDWHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    if (hwInfoConfig == 0x200030008) {
        BdwHw2x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
    } else if (hwInfoConfig == 0x100030008) {
        BdwHw1x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
    } else if (hwInfoConfig == 0x100030006) {
        BdwHw1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
    } else if (hwInfoConfig == 0x100020006) {
        BdwHw1x2x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
    } else if (hwInfoConfig == 0x0) {
        // Default config
        BdwHw1x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*BDW::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupBDWHardwareInfoImpl;
} // namespace NEO
