/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/aub_mem_dump/definitions/aub_services.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/xe_hp_core/hw_cmds.h"

#include "engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_XE_HP_SDV>::abbreviation = "xe_hp_sdv";

const PLATFORM XE_HP_SDV::platform = {
    IGFX_XE_HP_SDV,
    PCH_UNKNOWN,
    IGFX_XE_HP_CORE,
    IGFX_XE_HP_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    4,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable XE_HP_SDV::capabilityTable{
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_CCS, {true, true, false, true}},
        {aub_stream::ENGINE_CCS1, {true, false, true, true}},
        {aub_stream::ENGINE_CCS2, {true, false, true, true}},
        {aub_stream::ENGINE_CCS3, {true, false, true, true}}}, // directSubmissionEngines
    {0, 0, 0, 0, false, false, false, false},                  // kmdNotifyProperties
    MemoryConstants::max48BitAddress,                          // gpuAddressSpace
    0,                                                         // sharedSystemMemCapabilities
    83.333,                                                    // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                                 // requiredPreemptionSurfaceSize
    "core",                                                    // platformType
    "",                                                        // deviceName
    PreemptionMode::ThreadGroup,                               // defaultPreemptionMode
    aub_stream::ENGINE_CCS,                                    // defaultEngineType
    0,                                                         // maxRenderFrequency
    30,                                                        // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::XeHP_SDV,        // aubDeviceId
    0,                                                         // extraQuantityThreadsPerEU
    64,                                                        // slmSize
    sizeof(XE_HP_SDV::GRF),                                    // grfSize
    36u,                                                       // timestampValidBits
    32u,                                                       // kernelTimestampValidBits
    false,                                                     // blitterOperationsSupported
    true,                                                      // ftrSupportsInteger64BitAtomics
    true,                                                      // ftrSupportsFP64
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
    true                                                       // l0DebuggerSupported;
};

WorkaroundTable XE_HP_SDV::workaroundTable = {};
FeatureTable XE_HP_SDV::featureTable = {};

void XE_HP_SDV::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
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
    featureTable->flags.ftrMultiTileArch = true;
    featureTable->flags.ftrCCSMultiInstance = true;

    workaroundTable->flags.wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->flags.waEnablePreemptionGranularityControlByUMD = true;
};

void XE_HP_SDV::setupHardwareInfoBase(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * XE_HP_SDV::threadsPerEu;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = XE_HP_SDV::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = XE_HP_SDV::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = XE_HP_SDV::maxSubslicesSupported;
    gtSysInfo->MaxDualSubSlicesSupported = XE_HP_SDV::maxDualSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
}

const HardwareInfo XehpSdvHwConfig::hwInfo = {
    &XE_HP_SDV::platform,
    &XE_HP_SDV::featureTable,
    &XE_HP_SDV::workaroundTable,
    &XehpSdvHwConfig::gtSystemInfo,
    XE_HP_SDV::capabilityTable,
};
GT_SYSTEM_INFO XehpSdvHwConfig::gtSystemInfo = {0};
void XehpSdvHwConfig::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    // non-zero values for unit tests
    if (gtSysInfo->SliceCount == 0) {
        gtSysInfo->SliceCount = 2;
        gtSysInfo->SubSliceCount = 8;
        gtSysInfo->EUCount = 40;
        gtSysInfo->MaxEuPerSubSlice = gtSysInfo->EUCount / gtSysInfo->SubSliceCount;
        gtSysInfo->MaxSlicesSupported = gtSysInfo->SliceCount;
        gtSysInfo->MaxSubSlicesSupported = gtSysInfo->SubSliceCount;

        gtSysInfo->L3CacheSizeInKb = 1;
        gtSysInfo->L3BankCount = 1;

        gtSysInfo->CCSInfo.IsValid = true;
        gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;

        hwInfo->featureTable.ftrBcsInfo = 1;
    }

    if (setupFeatureTableAndWorkaroundTable) {
        XE_HP_SDV::setupFeatureAndWorkaroundTable(hwInfo);
    }
};

#include "hw_info_setup_xehp.inl"
} // namespace NEO
