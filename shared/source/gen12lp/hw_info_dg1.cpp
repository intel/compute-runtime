/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_info_dg1.h"

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/gen12lp/hw_cmds_dg1.h"
#include "shared/source/helpers/constants.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_DG1>::abbreviation = "dg1";

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
        {aub_stream::ENGINE_CCS, {true, true}}},               // directSubmissionEngines
    {0, 0, 0, 0, false, false, false, false},                  // kmdNotifyProperties
    MemoryConstants::max64BitAppAddress,                       // gpuAddressSpace
    0,                                                         // sharedSystemMemCapabilities
    MemoryConstants::pageSize,                                 // requiredPreemptionSurfaceSize
    "",                                                        // deviceName
    nullptr,                                                   // preferredPlatformName
    PreemptionMode::ThreadGroup,                               // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                                    // defaultEngineType
    0,                                                         // maxRenderFrequency
    30,                                                        // clVersionSupport
    AubMemDump::CmdServicesMemTraceVersion::DeviceValues::Dg1, // aubDeviceId
    1,                                                         // extraQuantityThreadsPerEU
    64,                                                        // maxProgrammableSlmSize
    sizeof(DG1::GRF),                                          // grfSize
    36u,                                                       // timestampValidBits
    32u,                                                       // kernelTimestampValidBits
    false,                                                     // blitterOperationsSupported
    true,                                                      // ftrSupportsInteger64BitAtomics
    false,                                                     // ftrSupportsFP64
    false,                                                     // ftrSupportsFP64Emulation
    false,                                                     // ftrSupports64BitMath
    false,                                                     // ftrSupportsCoherency
    false,                                                     // ftrRenderCompressedBuffers
    false,                                                     // ftrRenderCompressedImages
    true,                                                      // instrumentationEnabled
    true,                                                      // supportCacheFlushAfterWalker
    true,                                                      // supportsImages,
    true,                                                      // supportsOcl21Features
    false,                                                     // supportsOnDemandPageFaults
    false,                                                     // supportsIndependentForwardProgress
    false,                                                     // isIntegratedDevice
    true,                                                      // supportsMediaBlock
    true,                                                      // p2pAccessSupported
    false,                                                     // p2pAtomicAccessSupported
    true,                                                      // fusedEuEnabled
    true,                                                      // l0DebuggerSupported;
    true,                                                      // supportsFloatAtomics
    0                                                          // cxlType
};

WorkaroundTable DG1::workaroundTable = {};
FeatureTable DG1::featureTable = {};

void DG1::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->flags.ftrL3IACoherency = true;
    featureTable->flags.ftrPPGTT = true;
    featureTable->flags.ftrSVM = true;
    featureTable->flags.ftrIA32eGfxPTEs = true;
    featureTable->flags.ftrStandardMipTailFormat = true;
    featureTable->flags.ftrLocalMemory = true;

    featureTable->flags.ftrTranslationTable = true;
    featureTable->flags.ftrUserModeTranslationTable = true;
    featureTable->flags.ftrTileMappedResource = true;

    featureTable->flags.ftrFbc = true;
    featureTable->flags.ftrTileY = true;

    featureTable->flags.ftrAstcHdr2D = true;
    featureTable->flags.ftrAstcLdr2D = true;

    featureTable->flags.ftrGpGpuMidBatchPreempt = true;
    featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = true;
    featureTable->ftrBcsInfo = maxNBitValue(1);

    workaroundTable->flags.wa4kAlignUVOffsetNV12LinearSurface = true;
};

void DG1::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * 7u;
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

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
}

const HardwareInfo Dg1HwConfig::hwInfo = {
    &DG1::platform,
    &DG1::featureTable,
    &DG1::workaroundTable,
    &Dg1HwConfig::gtSystemInfo,
    DG1::capabilityTable};

GT_SYSTEM_INFO Dg1HwConfig::gtSystemInfo = {0};
void Dg1HwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    DG1::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->DualSubSliceCount = 6;
    gtSysInfo->L3CacheSizeInKb = 16384;
    gtSysInfo->L3BankCount = 8;
    gtSysInfo->MaxFillRate = 16;

    gtSysInfo->CCSInfo.IsValid = true;
    gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;
    gtSysInfo->CCSInfo.Instances.CCSEnableMask = 0b1;
};

const HardwareInfo DG1::hwInfo = Dg1HwConfig::hwInfo;

void setupDG1HardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    if (hwInfoConfig == 0x100060010) {
        Dg1HwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
    } else if (hwInfoConfig == 0x0) {
        // Default config
        Dg1HwConfig::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*DG1::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupDG1HardwareInfoImpl;
} // namespace NEO
