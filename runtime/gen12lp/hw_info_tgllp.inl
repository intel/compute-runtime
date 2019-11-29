/*
 * Copyright (C) 2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/gen12lp/hw_cmds.h"
#include "core/memory_manager/memory_constants.h"
#include "runtime/aub_mem_dump/aub_services.h"

#include "engine_node.h"

namespace NEO {

const char *HwMapper<IGFX_TIGERLAKE_LP>::abbreviation = "tgllp";

bool isSimulationTGLLP(unsigned short deviceId) {
    switch (deviceId) {
    case IGEN12LP_GT1_MOB_DEVICE_F0_ID:
        return true;
    }
    return false;
};

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
    {0, 0, 0, false, false, false},                  // kmdNotifyProperties
    MemoryConstants::max64BitAppAddress,             // gpuAddressSpace
    83.333,                                          // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                       // requiredPreemptionSurfaceSize
    &isSimulationTGLLP,                              // isSimulation
    PreemptionMode::ThreadGroup,                     // defaultPreemptionMode
    aub_stream::ENGINE_CCS,                          // defaultEngineType
    0,                                               // maxRenderFrequency
    21,                                              // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Tgllp, // aubDeviceId
    1,                                               // extraQuantityThreadsPerEU
    64,                                              // slmSize
    false,                                           // blitterOperationsSupported
    true,                                            // ftrSupportsInteger64BitAtomics
    false,                                           // ftrSupportsFP64
    false,                                           // ftrSupports64BitMath
    true,                                            // ftrSvm
    true,                                            // ftrSupportsCoherency
    false,                                           // ftrSupportsVmeAvcTextureSampler
    false,                                           // ftrSupportsVmeAvcPreemption
    false,                                           // ftrRenderCompressedBuffers
    false,                                           // ftrRenderCompressedImages
    true,                                            // instrumentationEnabled
    true,                                            // forceStatelessCompilationFor32Bit
    true,                                            // ftr64KBpages
    "lp",                                            // platformType
    true,                                            // sourceLevelDebuggerSupported
    false,                                           // supportsVme
    false,                                           // supportCacheFlushAfterWalker
    true,                                            // supportsImages
    true                                             // supportsDeviceEnqueue
};

WorkaroundTable TGLLP::workaroundTable = {};
FeatureTable TGLLP::featureTable = {};

void TGLLP::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->ftrL3IACoherency = true;
    featureTable->ftrPPGTT = true;
    featureTable->ftrSVM = true;
    featureTable->ftrIA32eGfxPTEs = true;
    featureTable->ftrStandardMipTailFormat = true;

    featureTable->ftrTranslationTable = true;
    featureTable->ftrUserModeTranslationTable = true;
    featureTable->ftrTileMappedResource = true;
    featureTable->ftrEnableGuC = true;

    featureTable->ftrFbc = true;
    featureTable->ftrFbc2AddressTranslation = true;
    featureTable->ftrFbcBlitterTracking = true;
    featureTable->ftrFbcCpuTracking = true;
    featureTable->ftrTileY = true;

    featureTable->ftrAstcHdr2D = true;
    featureTable->ftrAstcLdr2D = true;

    workaroundTable->wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->waEnablePreemptionGranularityControlByUMD = true;
    workaroundTable->waUntypedBufferCompression = true;
    if (hwInfo->platform.usRevId < REVISION_B) {
        workaroundTable->waUseOffsetToSkipSetFFIDGP = true;
        workaroundTable->waForceDefaultRCSEngine = true;
    }
};

const HardwareInfo TGLLP_1x6x16::hwInfo = {
    &TGLLP::platform,
    &TGLLP::featureTable,
    &TGLLP::workaroundTable,
    &TGLLP_1x6x16::gtSystemInfo,
    TGLLP::capabilityTable,
};

GT_SYSTEM_INFO TGLLP_1x6x16::gtSystemInfo = {0};
void TGLLP_1x6x16::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * TGLLP::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->DualSubSliceCount = 6;
    gtSysInfo->L3CacheSizeInKb = 3840;
    gtSysInfo->L3BankCount = 8;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = TGLLP::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = TGLLP::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = TGLLP::maxSubslicesSupported;
    gtSysInfo->MaxDualSubSlicesSupported = TGLLP::maxDualSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    gtSysInfo->CCSInfo.IsValid = true;
    gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;
    gtSysInfo->CCSInfo.Instances.CCSEnableMask = 0b1;

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo TGLLP_1x2x16::hwInfo = {
    &TGLLP::platform,
    &TGLLP::featureTable,
    &TGLLP::workaroundTable,
    &TGLLP_1x2x16::gtSystemInfo,
    TGLLP::capabilityTable,
};

GT_SYSTEM_INFO TGLLP_1x2x16::gtSystemInfo = {0};
void TGLLP_1x2x16::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->ThreadCount = gtSysInfo->EUCount * TGLLP::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->DualSubSliceCount = 2;
    gtSysInfo->L3CacheSizeInKb = 1920;
    gtSysInfo->L3BankCount = 4;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 224;
    gtSysInfo->TotalHsThreads = 224;
    gtSysInfo->TotalDsThreads = 224;
    gtSysInfo->TotalGsThreads = 224;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = TGLLP::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = TGLLP::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = TGLLP::maxSubslicesSupported;
    gtSysInfo->MaxDualSubSlicesSupported = TGLLP::maxDualSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;

    gtSysInfo->CCSInfo.IsValid = true;
    gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;
    gtSysInfo->CCSInfo.Instances.CCSEnableMask = 0b1;

    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo TGLLP::hwInfo = TGLLP_1x6x16::hwInfo;
const std::string TGLLP::defaultHardwareInfoConfig = "1x6x16";

void setupTGLLPHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const std::string &hwInfoConfig) {
    if (hwInfoConfig == "1x6x16") {
        TGLLP_1x6x16::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "1x2x16") {
        TGLLP_1x2x16::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "default") {
        // Default config
        TGLLP_1x6x16::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*TGLLP::setupHardwareInfo)(HardwareInfo *, bool, const std::string &) = setupTGLLPHardwareInfoImpl;
} // namespace NEO
