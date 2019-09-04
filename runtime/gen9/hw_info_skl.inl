/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/memory_manager/memory_constants.h"
#include "runtime/aub_mem_dump/aub_services.h"
#include "runtime/gen9/hw_cmds.h"

#include "engine_node.h"
#include "hw_info_skl.h"

namespace NEO {

const char *HwMapper<IGFX_SKYLAKE>::abbreviation = "skl";

bool isSimulationSKL(unsigned short deviceId) {
    switch (deviceId) {
    case ISKL_GT0_DESK_DEVICE_F0_ID:
    case ISKL_GT1_DESK_DEVICE_F0_ID:
    case ISKL_GT2_DESK_DEVICE_F0_ID:
    case ISKL_GT3_DESK_DEVICE_F0_ID:
    case ISKL_GT4_DESK_DEVICE_F0_ID:
        return true;
    }
    return false;
};

const PLATFORM SKL::platform = {
    IGFX_SKYLAKE,
    PCH_UNKNOWN,
    IGFX_GEN9_CORE,
    IGFX_GEN9_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    9,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable SKL::capabilityTable{
    {0, 0, 0, false, false, false},                // kmdNotifyProperties
    MemoryConstants::max48BitAddress,              // gpuAddressSpace
    83.333,                                        // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                     // requiredPreemptionSurfaceSize
    &isSimulationSKL,                              // isSimulation
    PreemptionMode::MidThread,                     // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                        // defaultEngineType
    0,                                             // maxRenderFrequency
    21,                                            // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Skl, // aubDeviceId
    0,                                             // extraQuantityThreadsPerEU
    64,                                            // slmSize
    false,                                         // blitterOperationsSupported
    true,                                          // ftrSupportsInteger64BitAtomics
    true,                                          // ftrSupportsFP64
    true,                                          // ftrSupports64BitMath
    true,                                          // ftrSvm
    true,                                          // ftrSupportsCoherency
    true,                                          // ftrSupportsVmeAvcTextureSampler
    false,                                         // ftrSupportsVmeAvcPreemption
    false,                                         // ftrRenderCompressedBuffers
    false,                                         // ftrRenderCompressedImages
    true,                                          // ftr64KBpages
    true,                                          // instrumentationEnabled
    true,                                          // forceStatelessCompilationFor32Bit
    "core",                                        // platformType
    true,                                          // sourceLevelDebuggerSupported
    true,                                          // supportsVme
    false,                                         // supportCacheFlushAfterWalker
    true,                                          // supportsImages
    true                                           // supportsDeviceEnqueue
};
WorkaroundTable SKL::workaroundTable = {};
FeatureTable SKL::featureTable = {};
void SKL::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->ftrGpGpuMidBatchPreempt = true;
    featureTable->ftrGpGpuThreadGroupLevelPreempt = true;
    featureTable->ftrL3IACoherency = true;
    featureTable->ftrGpGpuMidThreadLevelPreempt = true;
    featureTable->ftr3dMidBatchPreempt = true;
    featureTable->ftr3dObjectLevelPreempt = true;
    featureTable->ftrPerCtxtPreemptionGranularityControl = true;
    featureTable->ftrPPGTT = true;
    featureTable->ftrSVM = true;
    featureTable->ftrIA32eGfxPTEs = true;
    featureTable->ftrDisplayYTiling = true;
    featureTable->ftrTranslationTable = true;
    featureTable->ftrUserModeTranslationTable = true;
    featureTable->ftrEnableGuC = true;
    featureTable->ftrFbc = true;
    featureTable->ftrFbc2AddressTranslation = true;
    featureTable->ftrFbcBlitterTracking = true;
    featureTable->ftrFbcCpuTracking = true;
    featureTable->ftrVcs2 = featureTable->ftrGT3 || featureTable->ftrGT4;
    featureTable->ftrVEBOX = true;
    featureTable->ftrSingleVeboxSlice = featureTable->ftrGT1 || featureTable->ftrGT2;
    featureTable->ftrTileY = true;

    workaroundTable->waEnablePreemptionGranularityControlByUMD = true;
    workaroundTable->waSendMIFLUSHBeforeVFE = true;
    workaroundTable->waReportPerfCountUseGlobalContextID = true;
    workaroundTable->waDisableLSQCROPERFforOCL = true;
    workaroundTable->waMsaa8xTileYDepthPitchAlignment = true;
    workaroundTable->waLosslessCompressionSurfaceStride = true;
    workaroundTable->waFbcLinearSurfaceStride = true;
    workaroundTable->wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->waEncryptedEdramOnlyPartials = true;
    workaroundTable->waDisableEdramForDisplayRT = true;
    workaroundTable->waForcePcBbFullCfgRestore = true;
    workaroundTable->waSamplerCacheFlushBetweenRedescribedSurfaceReads = true;

    if ((1 << hwInfo->platform.usRevId) & 0x0eu) {
        workaroundTable->waCompressedResourceRequiresConstVA21 = true;
    }
    if ((1 << hwInfo->platform.usRevId) & 0x0fu) {
        workaroundTable->waDisablePerCtxtPreemptionGranularityControl = true;
        workaroundTable->waModifyVFEStateAfterGPGPUPreemption = true;
    }
    if ((1 << hwInfo->platform.usRevId) & 0x3f) {
        workaroundTable->waCSRUncachable = true;
    }
}
const HardwareInfo SKL_1x2x6::hwInfo = {
    &SKL::platform,
    &SKL::featureTable,
    &SKL::workaroundTable,
    &SKL_1x2x6::gtSystemInfo,
    SKL::capabilityTable,
};
GT_SYSTEM_INFO SKL_1x2x6::gtSystemInfo = {0};
void SKL_1x2x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->EUCount = 11;
    gtSysInfo->ThreadCount = 11 * SKL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 2;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 2;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = SKL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = SKL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = SKL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo SKL_1x3x6::hwInfo = {
    &SKL::platform,
    &SKL::featureTable,
    &SKL::workaroundTable,
    &SKL_1x3x6::gtSystemInfo,
    SKL::capabilityTable,
};
GT_SYSTEM_INFO SKL_1x3x6::gtSystemInfo = {0};
void SKL_1x3x6::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->EUCount = 17;
    gtSysInfo->ThreadCount = 17 * SKL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 3;
    gtSysInfo->L3CacheSizeInKb = 768;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = SKL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = SKL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = SKL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo SKL_1x3x8::hwInfo = {
    &SKL::platform,
    &SKL::featureTable,
    &SKL::workaroundTable,
    &SKL_1x3x8::gtSystemInfo,
    SKL::capabilityTable,
};
GT_SYSTEM_INFO SKL_1x3x8::gtSystemInfo = {0};
void SKL_1x3x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->EUCount = 23;
    gtSysInfo->ThreadCount = 23 * SKL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 3;
    gtSysInfo->L3CacheSizeInKb = 768;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = SKL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = SKL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = SKL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo SKL_2x3x8::hwInfo = {
    &SKL::platform,
    &SKL::featureTable,
    &SKL::workaroundTable,
    &SKL_2x3x8::gtSystemInfo,
    SKL::capabilityTable,
};
GT_SYSTEM_INFO SKL_2x3x8::gtSystemInfo = {0};
void SKL_2x3x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->EUCount = 47;
    gtSysInfo->ThreadCount = 47 * SKL::threadsPerEu;
    gtSysInfo->SliceCount = 2;
    gtSysInfo->SubSliceCount = 6;
    gtSysInfo->L3CacheSizeInKb = 1536;
    gtSysInfo->L3BankCount = 8;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = SKL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = SKL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = SKL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo SKL_3x3x8::hwInfo = {
    &SKL::platform,
    &SKL::featureTable,
    &SKL::workaroundTable,
    &SKL_3x3x8::gtSystemInfo,
    SKL::capabilityTable,
};
GT_SYSTEM_INFO SKL_3x3x8::gtSystemInfo = {0};
void SKL_3x3x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->EUCount = 71;
    gtSysInfo->ThreadCount = 71 * SKL::threadsPerEu;
    gtSysInfo->SliceCount = 3;
    gtSysInfo->SubSliceCount = 9;
    gtSysInfo->L3CacheSizeInKb = 2304;
    gtSysInfo->L3BankCount = 12;
    gtSysInfo->MaxFillRate = 24;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = SKL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = SKL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = SKL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo SKL::hwInfo = SKL_1x3x8::hwInfo;

void setupSKLHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const std::string &hwInfoConfig) {
    if (hwInfoConfig == "1x3x8") {
        SKL_1x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "2x3x8") {
        SKL_2x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "3x3x8") {
        SKL_3x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "1x2x6") {
        SKL_1x2x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "1x3x6") {
        SKL_1x3x6::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "default") {
        // Default config
        SKL_1x3x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*SKL::setupHardwareInfo)(HardwareInfo *, bool, const std::string &) = setupSKLHardwareInfoImpl;
} // namespace NEO
