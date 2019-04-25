/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub_mem_dump/aub_services.h"
#include "runtime/memory_manager/memory_constants.h"

#include "engine_node.h"
#include "hw_cmds.h"
#include "hw_info_bxt.h"

namespace NEO {

const char *HwMapper<IGFX_BROXTON>::abbreviation = "bxt";

bool isSimulationBXT(unsigned short deviceId) {
    switch (deviceId) {
    case IBXT_A_DEVICE_F0_ID:
    case IBXT_C_DEVICE_F0_ID:
        return true;
    }
    return false;
};

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
    {0, 0, 0, false, false, false},                // kmdNotifyProperties
    {true, false},                                 // whitelistedRegisters
    MemoryConstants::max48BitAddress,              // gpuAddressSpace
    52.083,                                        // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                     // requiredPreemptionSurfaceSize
    &isSimulationBXT,                              // isSimulation
    PreemptionMode::MidThread,                     // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                        // defaultEngineType
    0,                                             // maxRenderFrequency
    12,                                            // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Bxt, // aubDeviceId
    0,                                             // extraQuantityThreadsPerEU
    64,                                            // slmSize
    false,                                         // blitterOperationsSupported
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
    false,                                         // forceStatelessCompilationFor32Bit
    false,                                         // isCore
    true,                                          // sourceLevelDebuggerSupported
    true,                                          // supportsVme
    false                                          // supportCacheFlushAfterWalker
};

WorkaroundTable BXT::workaroundTable = {};
FeatureTable BXT::featureTable = {};

void BXT::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    PLATFORM *pPlatform = const_cast<PLATFORM *>(hwInfo->pPlatform);
    FeatureTable *pSkuTable = const_cast<FeatureTable *>(hwInfo->pSkuTable);
    WorkaroundTable *pWaTable = const_cast<WorkaroundTable *>(hwInfo->pWaTable);

    pSkuTable->ftrGpGpuMidBatchPreempt = true;
    pSkuTable->ftrGpGpuThreadGroupLevelPreempt = true;
    pSkuTable->ftrL3IACoherency = true;
    pSkuTable->ftrVEBOX = true;
    pSkuTable->ftrULT = true;
    pSkuTable->ftrGpGpuMidThreadLevelPreempt = true;
    pSkuTable->ftr3dMidBatchPreempt = true;
    pSkuTable->ftr3dObjectLevelPreempt = true;
    pSkuTable->ftrPerCtxtPreemptionGranularityControl = true;
    pSkuTable->ftrLCIA = true;
    pSkuTable->ftrPPGTT = true;
    pSkuTable->ftrIA32eGfxPTEs = true;
    pSkuTable->ftrDisplayYTiling = true;
    pSkuTable->ftrTranslationTable = true;
    pSkuTable->ftrUserModeTranslationTable = true;
    pSkuTable->ftrEnableGuC = true;
    pSkuTable->ftrFbc = true;
    pSkuTable->ftrFbc2AddressTranslation = true;
    pSkuTable->ftrFbcBlitterTracking = true;
    pSkuTable->ftrFbcCpuTracking = true;
    pSkuTable->ftrTileY = true;

    if (pPlatform->usRevId >= 3) {
        pSkuTable->ftrGttCacheInvalidation = true;
    }

    pWaTable->waLLCCachingUnsupported = true;
    pWaTable->waMsaa8xTileYDepthPitchAlignment = true;
    pWaTable->waFbcLinearSurfaceStride = true;
    pWaTable->wa4kAlignUVOffsetNV12LinearSurface = true;
    pWaTable->waEnablePreemptionGranularityControlByUMD = true;
    pWaTable->waSendMIFLUSHBeforeVFE = true;
    pWaTable->waForcePcBbFullCfgRestore = true;
    pWaTable->waReportPerfCountUseGlobalContextID = true;
    pWaTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;
}

const HardwareInfo BXT_1x2x6::hwInfo = {
    &BXT::platform,
    &BXT::featureTable,
    &BXT::workaroundTable,
    &BXT_1x2x6::gtSystemInfo,
    BXT::capabilityTable,
};
GT_SYSTEM_INFO BXT_1x2x6::gtSystemInfo = {0};
void BXT_1x2x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    gtSysInfo->EUCount = 12;
    gtSysInfo->ThreadCount = 12 * BXT::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 2;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 1;
    gtSysInfo->MaxFillRate = 8;
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
};

const HardwareInfo BXT_1x3x6::hwInfo = {
    &BXT::platform,
    &BXT::featureTable,
    &BXT::workaroundTable,
    &BXT_1x3x6::gtSystemInfo,
    BXT::capabilityTable,
};
GT_SYSTEM_INFO BXT_1x3x6::gtSystemInfo = {0};
void BXT_1x3x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);
    gtSysInfo->EUCount = 18;
    gtSysInfo->ThreadCount = 18 * BXT::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 3;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 1;
    gtSysInfo->MaxFillRate = 8;
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
};

const HardwareInfo BXT::hwInfo = BXT_1x3x6::hwInfo;

void setupBXTHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const std::string &hwInfoConfig) {
    if (hwInfoConfig == "1x2x6") {
        BXT_1x2x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "1x3x6") {
        BXT_1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "default") {
        // Default config
        BXT_1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*BXT::setupHardwareInfo)(HardwareInfo *, bool, const std::string &) = setupBXTHardwareInfoImpl;
} // namespace NEO
