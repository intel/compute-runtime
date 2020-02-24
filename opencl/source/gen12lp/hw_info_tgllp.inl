/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/gen12lp/hw_cmds.h"
#include "shared/source/memory_manager/memory_constants.h"

#include "opencl/source/aub_mem_dump/aub_services.h"

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
    EngineDirectSubmissionInitVec{
        {aub_stream::ENGINE_RCS, {true, true}},
        {aub_stream::ENGINE_CCS, {true, true}}},     // directSubmissionEngines
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
    sizeof(TGLLP::GRF),                              // grfSize
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
    true,                                            // supportsDeviceEnqueue
    false                                            // hostPtrTrackingEnabled
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
    featureTable->ftrE2ECompression = true;

    featureTable->ftrAstcHdr2D = true;
    featureTable->ftrAstcLdr2D = true;

    featureTable->ftr3dMidBatchPreempt = true;
    featureTable->ftrGpGpuMidBatchPreempt = true;
    featureTable->ftrGpGpuThreadGroupLevelPreempt = true;
    featureTable->ftrPerCtxtPreemptionGranularityControl = true;

    workaroundTable->wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->waEnablePreemptionGranularityControlByUMD = true;
    workaroundTable->waUntypedBufferCompression = true;
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
const uint64_t TGLLP::defaultHardwareInfoConfig = 0x100060016;

void setupTGLLPHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig) {
    if (hwInfoConfig == 0x100060016) {
        TGLLP_1x6x16::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x100020016) {
        TGLLP_1x2x16::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == 0x0) {
        // Default config
        TGLLP_1x6x16::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*TGLLP::setupHardwareInfo)(HardwareInfo *, bool, uint64_t) = setupTGLLPHardwareInfoImpl;
} // namespace NEO
