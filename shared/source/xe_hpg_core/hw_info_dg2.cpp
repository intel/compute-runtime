/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/xe_hpg_core/hw_cmds_dg2.h"

#include "engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_DG2>::abbreviation = "dg2";

const PLATFORM DG2::platform = {
    IGFX_DG2,
    PCH_UNKNOWN,
    IGFX_XE_HPG_CORE,
    IGFX_XE_HPG_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable DG2::capabilityTable{
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_RCS, {false, false, false, false}},
        {aub_stream::ENGINE_CCS, {true, false, false, true}},
        {aub_stream::ENGINE_CCS1, {true, false, true, true}},
        {aub_stream::ENGINE_CCS2, {true, false, true, true}},
        {aub_stream::ENGINE_CCS3, {true, false, true, true}}}, // directSubmissionEngines
    {0, 0, 0, 0, false, false, false, false},                  // kmdNotifyProperties
    MemoryConstants::max48BitAddress,                          // gpuAddressSpace
    0,                                                         // sharedSystemMemCapabilities
    83.333,                                                    // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                                 // requiredPreemptionSurfaceSize
    "dg2",                                                     // platformType
    "",                                                        // deviceName
    PreemptionMode::ThreadGroup,                               // defaultPreemptionMode
    aub_stream::ENGINE_CCS,                                    // defaultEngineType
    0,                                                         // maxRenderFrequency
    30,                                                        // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Dg2,             // aubDeviceId
    0,                                                         // extraQuantityThreadsPerEU
    64,                                                        // slmSize
    sizeof(DG2::GRF),                                          // grfSize
    36u,                                                       // timestampValidBits
    32u,                                                       // kernelTimestampValidBits
    false,                                                     // blitterOperationsSupported
    true,                                                      // ftrSupportsInteger64BitAtomics
    false,                                                     // ftrSupportsFP64
    true,                                                      // ftrSupports64BitMath
    true,                                                      // ftrSvm
    false,                                                     // ftrSupportsCoherency
    false,                                                     // ftrSupportsVmeAvcTextureSampler
    false,                                                     // ftrSupportsVmeAvcPreemption
    false,                                                     // ftrRenderCompressedBuffers
    false,                                                     // ftrRenderCompressedImages
    true,                                                      // ftr64KBpages
    true,                                                      // instrumentationEnabled
    true,                                                      // sourceLevelDebuggerSupported
    false,                                                     // supportsVme
    true,                                                      // supportCacheFlushAfterWalker
    true,                                                      // supportsImages
    false,                                                     // supportsDeviceEnqueue
    false,                                                     // supportsPipes
    true,                                                      // supportsOcl21Features
    false,                                                     // supportsOnDemandPageFaults
    false,                                                     // supportsIndependentForwardProgress
    false,                                                     // hostPtrTrackingEnabled
    true,                                                      // levelZeroSupported
    false,                                                     // isIntegratedDevice
    true,                                                      // supportsMediaBlock
    true,                                                      // p2pAccessSupported
    false,                                                     // p2pAtomicAccessSupported
    true,                                                      // fusedEuEnabled
    true                                                       // l0DebuggerSupported
};

WorkaroundTable DG2::workaroundTable = {};
FeatureTable DG2::featureTable = {};

void DG2::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->flags.ftrL3IACoherency = true;
    featureTable->flags.ftrFlatPhysCCS = true;
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
    featureTable->flags.ftrAstcHdr2D = true;
    featureTable->flags.ftrAstcLdr2D = true;

    featureTable->flags.ftr3dMidBatchPreempt = true;
    featureTable->flags.ftrGpGpuMidBatchPreempt = true;
    featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = true;
    featureTable->flags.ftrPerCtxtPreemptionGranularityControl = true;

    featureTable->flags.ftrTileY = false;
    featureTable->flags.ftrLocalMemory = true;
    featureTable->flags.ftrLinearCCS = true;
    featureTable->flags.ftrE2ECompression = true;
    featureTable->flags.ftrCCSNode = true;
    featureTable->flags.ftrCCSRing = true;
    featureTable->flags.ftrCCSMultiInstance = true;

    featureTable->flags.ftrUnified3DMediaCompressionFormats = true;

    workaroundTable->flags.wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->flags.waEnablePreemptionGranularityControlByUMD = true;
};

void DG2::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * DG2::threadsPerEu;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = DG2::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = DG2::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = DG2::maxSubslicesSupported;
    gtSysInfo->MaxDualSubSlicesSupported = DG2::maxDualSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    adjustHardwareInfo(hwInfo);
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
}

const HardwareInfo Dg2HwConfig::hwInfo = {
    &DG2::platform,
    &DG2::featureTable,
    &DG2::workaroundTable,
    &Dg2HwConfig::gtSystemInfo,
    DG2::capabilityTable,
};
GT_SYSTEM_INFO Dg2HwConfig::gtSystemInfo = {0};
void Dg2HwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    // non-zero values for unit tests
    if (gtSysInfo->SliceCount == 0) {
        gtSysInfo->SliceCount = 2;
        gtSysInfo->SubSliceCount = 8;
        gtSysInfo->DualSubSliceCount = gtSysInfo->SubSliceCount;
        gtSysInfo->EUCount = 40;
        gtSysInfo->MaxEuPerSubSlice = gtSysInfo->EUCount / gtSysInfo->SubSliceCount;
        gtSysInfo->MaxSlicesSupported = gtSysInfo->SliceCount;
        gtSysInfo->MaxSubSlicesSupported = gtSysInfo->SubSliceCount;

        gtSysInfo->L3CacheSizeInKb = 1;
        gtSysInfo->L3BankCount = 1;

        gtSysInfo->CCSInfo.IsValid = true;
        gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;

        hwInfo->featureTable.ftrBcsInfo = 1;
        gtSysInfo->IsDynamicallyPopulated = true;
        for (uint32_t slice = 0; slice < gtSysInfo->SliceCount; slice++) {
            gtSysInfo->SliceInfo[slice].Enabled = true;
        }
    }

    adjustHardwareInfo(hwInfo);
    if (setupFeatureTableAndWorkaroundTable) {
        DG2::setupFeatureAndWorkaroundTable(hwInfo);
    }
};

#include "hw_info_setup_dg2.inl"
} // namespace NEO
