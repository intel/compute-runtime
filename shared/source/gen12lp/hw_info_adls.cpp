/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_info_adls.h"

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/gen12lp/hw_cmds_adls.h"
#include "shared/source/helpers/constants.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_ALDERLAKE_S>::abbreviation = "adls";

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
        {aub_stream::ENGINE_CCS, {true, true}}},                // directSubmissionEngines
    {0, 0, 0, 0, false, false, false, false},                   // kmdNotifyProperties
    MemoryConstants::max64BitAppAddress,                        // gpuAddressSpace
    0,                                                          // sharedSystemMemCapabilities
    MemoryConstants::pageSize,                                  // requiredPreemptionSurfaceSize
    "",                                                         // deviceName
    nullptr,                                                    // preferredPlatformName
    PreemptionMode::ThreadGroup,                                // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                                     // defaultEngineType
    0,                                                          // maxRenderFrequency
    30,                                                         // clVersionSupport
    AubMemDump::CmdServicesMemTraceVersion::DeviceValues::Adls, // aubDeviceId
    1,                                                          // extraQuantityThreadsPerEU
    64,                                                         // maxProgrammableSlmSize
    sizeof(ADLS::GRF),                                          // grfSize
    36u,                                                        // timestampValidBits
    32u,                                                        // kernelTimestampValidBits
    false,                                                      // blitterOperationsSupported
    true,                                                       // ftrSupportsInteger64BitAtomics
    false,                                                      // ftrSupportsFP64
    false,                                                      // ftrSupportsFP64Emulation
    false,                                                      // ftrSupports64BitMath
    false,                                                      // ftrSupportsCoherency
    false,                                                      // ftrRenderCompressedBuffers
    false,                                                      // ftrRenderCompressedImages
    true,                                                       // instrumentationEnabled
    false,                                                      // supportCacheFlushAfterWalker
    true,                                                       // supportsImages
    true,                                                       // supportsOcl21Features
    false,                                                      // supportsOnDemandPageFaults
    false,                                                      // supportsIndependentForwardProgress
    true,                                                       // isIntegratedDevice
    true,                                                       // supportsMediaBlock
    false,                                                      // p2pAccessSupported
    false,                                                      // p2pAtomicAccessSupported
    true,                                                       // fusedEuEnabled
    false,                                                      // l0DebuggerSupported;
    true,                                                       // supportsFloatAtomics
    0                                                           // cxlType
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

    featureTable->flags.ftrFbc = true;
    featureTable->flags.ftrTileY = true;

    featureTable->flags.ftrAstcHdr2D = true;
    featureTable->flags.ftrAstcLdr2D = true;

    featureTable->flags.ftrGpGpuMidBatchPreempt = true;
    featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = true;

    workaroundTable->flags.wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->flags.waUntypedBufferCompression = true;
};

void ADLS::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * 7u;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = ADLS::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = ADLS::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = ADLS::maxSubslicesSupported;
    gtSysInfo->MaxDualSubSlicesSupported = ADLS::maxDualSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
}

const HardwareInfo AdlsHwConfig::hwInfo = {
    &ADLS::platform,
    &ADLS::featureTable,
    &ADLS::workaroundTable,
    &AdlsHwConfig::gtSystemInfo,
    ADLS::capabilityTable};

GT_SYSTEM_INFO AdlsHwConfig::gtSystemInfo = {0};
void AdlsHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->DualSubSliceCount = gtSysInfo->SubSliceCount;
    gtSysInfo->L3CacheSizeInKb = 1920;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 0;
    gtSysInfo->TotalHsThreads = 0;
    gtSysInfo->TotalDsThreads = 0;
    gtSysInfo->TotalGsThreads = 0;
    if (gtSysInfo->MaxSubSlicesSupported < 1) {
        gtSysInfo->MaxSubSlicesSupported = 1;
    }
    if (gtSysInfo->MaxDualSubSlicesSupported < 2) {
        gtSysInfo->MaxDualSubSlicesSupported = 2;
    }

    gtSysInfo->CCSInfo.IsValid = true;
    gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;
    gtSysInfo->CCSInfo.Instances.CCSEnableMask = 0b1;
};

const HardwareInfo ADLS::hwInfo = AdlsHwConfig::hwInfo;

void setupADLSHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    AdlsHwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
}

void (*ADLS::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupADLSHardwareInfoImpl;
} // namespace NEO
