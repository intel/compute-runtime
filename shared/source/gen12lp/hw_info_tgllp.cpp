/*
 * Copyright (C) 2019-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_info_tgllp.h"

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/gen12lp/hw_cmds_tgllp.h"
#include "shared/source/helpers/constants.h"

#include "aubstream/engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_TIGERLAKE_LP>::abbreviation = "tgllp";

const PLATFORM TGLLP::platform = {
    IGFX_TIGERLAKE_LP,
    PCH_UNKNOWN,
    IGFX_GEN12LP_CORE,
    IGFX_GEN12LP_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable TGLLP::capabilityTable{
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_RCS, {true, true}},
        {aub_stream::ENGINE_CCS, {true, true}}},                 // directSubmissionEngines
    {0, 0, 0, 0, false, false, false, false},                    // kmdNotifyProperties
    MemoryConstants::max64BitAppAddress,                         // gpuAddressSpace
    0,                                                           // sharedSystemMemCapabilities
    MemoryConstants::pageSize,                                   // requiredPreemptionSurfaceSize
    "",                                                          // deviceName
    nullptr,                                                     // preferredPlatformName
    PreemptionMode::ThreadGroup,                                 // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                                      // defaultEngineType
    0,                                                           // maxRenderFrequency
    30,                                                          // clVersionSupport
    AubMemDump::CmdServicesMemTraceVersion::DeviceValues::Tgllp, // aubDeviceId
    1,                                                           // extraQuantityThreadsPerEU
    64,                                                          // maxProgrammableSlmSize
    sizeof(TGLLP::GRF),                                          // grfSize
    36u,                                                         // timestampValidBits
    32u,                                                         // kernelTimestampValidBits
    false,                                                       // blitterOperationsSupported
    true,                                                        // ftrSupportsInteger64BitAtomics
    false,                                                       // ftrSupportsFP64
    false,                                                       // ftrSupportsFP64Emulation
    false,                                                       // ftrSupports64BitMath
    false,                                                       // ftrSupportsCoherency
    false,                                                       // ftrRenderCompressedBuffers
    false,                                                       // ftrRenderCompressedImages
    true,                                                        // instrumentationEnabled
    false,                                                       // supportCacheFlushAfterWalker
    true,                                                        // supportsImages
    true,                                                        // supportsOcl21Features
    false,                                                       // supportsOnDemandPageFaults
    false,                                                       // supportsIndependentForwardProgress
    true,                                                        // isIntegratedDevice
    true,                                                        // supportsMediaBlock
    false,                                                       // p2pAccessSupported
    false,                                                       // p2pAtomicAccessSupported
    true,                                                        // fusedEuEnabled
    false,                                                       // l0DebuggerSupported;
    true,                                                        // supportsFloatAtomics
    0                                                            // cxlType
};

WorkaroundTable TGLLP::workaroundTable = {};
FeatureTable TGLLP::featureTable = {};

void TGLLP::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
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

void TGLLP::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * 7u;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = TGLLP::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = TGLLP::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = TGLLP::maxSubslicesSupported;
    gtSysInfo->MaxDualSubSlicesSupported = TGLLP::maxDualSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
}

const HardwareInfo TgllpHw1x6x16::hwInfo = {
    &TGLLP::platform,
    &TGLLP::featureTable,
    &TGLLP::workaroundTable,
    &TgllpHw1x6x16::gtSystemInfo,
    TGLLP::capabilityTable};

GT_SYSTEM_INFO TgllpHw1x6x16::gtSystemInfo = {0};
void TgllpHw1x6x16::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    TGLLP::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->DualSubSliceCount = 6;
    gtSysInfo->L3CacheSizeInKb = 3840;
    gtSysInfo->L3BankCount = 8;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;

    gtSysInfo->CCSInfo.IsValid = true;
    gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;
    gtSysInfo->CCSInfo.Instances.CCSEnableMask = 0b1;
};

const HardwareInfo TgllpHw1x2x16::hwInfo = {
    &TGLLP::platform,
    &TGLLP::featureTable,
    &TGLLP::workaroundTable,
    &TgllpHw1x2x16::gtSystemInfo,
    TGLLP::capabilityTable};

GT_SYSTEM_INFO TgllpHw1x2x16::gtSystemInfo = {0};
void TgllpHw1x2x16::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper) {
    TGLLP::setupHardwareInfoBase(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);

    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->DualSubSliceCount = 2;
    gtSysInfo->L3CacheSizeInKb = 1920;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 224;
    gtSysInfo->TotalHsThreads = 224;
    gtSysInfo->TotalDsThreads = 224;
    gtSysInfo->TotalGsThreads = 224;

    gtSysInfo->CCSInfo.IsValid = true;
    gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;
    gtSysInfo->CCSInfo.Instances.CCSEnableMask = 0b1;
};

const HardwareInfo TGLLP::hwInfo = TgllpHw1x6x16::hwInfo;

void setupTGLLPHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper) {
    if (hwInfoConfig == 0x100060010) {
        TgllpHw1x6x16::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
    } else if (hwInfoConfig == 0x100020010) {
        TgllpHw1x2x16::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
    } else if (hwInfoConfig == 0x0) {
        // Default config
        TgllpHw1x6x16::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable, releaseHelper);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*TGLLP::setupHardwareInfo)(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = setupTGLLPHardwareInfoImpl;
} // namespace NEO
