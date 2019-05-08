/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/aub_mem_dump/aub_services.h"
#include "runtime/memory_manager/memory_constants.h"

#include "engine_node.h"
#include "hw_cmds_lkf.h"
#include "hw_info_lkf.h"

namespace NEO {

const char *HwMapper<IGFX_LAKEFIELD>::abbreviation = "lkf";

bool isSimulationLKF(unsigned short deviceId) {
    switch (deviceId) {
    case ILKF_1x8x8_DESK_DEVICE_F0_ID:
        return true;
    }
    return false;
};
const PLATFORM LKF::platform = {
    IGFX_LAKEFIELD,
    PCH_UNKNOWN,
    IGFX_GEN11_CORE,
    IGFX_GEN11_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable LKF::capabilityTable{
    {0, 0, 0, false, false, false},                // kmdNotifyProperties
    {true, false},                                 // whitelistedRegisters
    MemoryConstants::max36BitAddress,              // gpuAddressSpace
    83.333,                                        // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                     // requiredPreemptionSurfaceSize
    &isSimulationLKF,                              // isSimulation
    PreemptionMode::MidThread,                     // defaultPreemptionMode
    aub_stream::ENGINE_RCS,                        // defaultEngineType
    0,                                             // maxRenderFrequency
    12,                                            // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Lkf, // aubDeviceId
    1,                                             // extraQuantityThreadsPerEU
    64,                                            // slmSize
    false,                                         // blitterOperationsSupported
    false,                                         // ftrSupportsFP64
    false,                                         // ftrSupports64BitMath
    false,                                         // ftrSvm
    true,                                          // ftrSupportsCoherency
    true,                                          // ftrSupportsVmeAvcTextureSampler
    true,                                          // ftrSupportsVmeAvcPreemption
    false,                                         // ftrRenderCompressedBuffers
    false,                                         // ftrRenderCompressedImages
    true,                                          // ftr64KBpages
    true,                                          // instrumentationEnabled
    true,                                          // forceStatelessCompilationFor32Bit
    false,                                         // isCore
    true,                                          // sourceLevelDebuggerSupported
    false,                                         // supportsVme
    false                                          // supportCacheFlushAfterWalker
};

WorkaroundTable LKF::workaroundTable = {};
FeatureTable LKF::featureTable = {};

void LKF::setupFeatureAndWorkaroundTable(HardwareInfo *hwInfo) {
    FeatureTable *featureTable = &hwInfo->featureTable;
    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    featureTable->ftrL3IACoherency = true;
    featureTable->ftrPPGTT = true;
    featureTable->ftrSVM = true;
    featureTable->ftrIA32eGfxPTEs = true;
    featureTable->ftrStandardMipTailFormat = true;

    featureTable->ftrDisplayYTiling = true;
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

    featureTable->ftr3dMidBatchPreempt = true;
    featureTable->ftrGpGpuMidBatchPreempt = true;
    featureTable->ftrGpGpuMidThreadLevelPreempt = true;
    featureTable->ftrGpGpuThreadGroupLevelPreempt = true;
    featureTable->ftrPerCtxtPreemptionGranularityControl = true;

    workaroundTable->wa4kAlignUVOffsetNV12LinearSurface = true;
    workaroundTable->waReportPerfCountUseGlobalContextID = true;
};

const HardwareInfo LKF_1x8x8::hwInfo = {
    &LKF::platform,
    &LKF::featureTable,
    &LKF::workaroundTable,
    &LKF_1x8x8::gtSystemInfo,
    LKF::capabilityTable,
};
GT_SYSTEM_INFO LKF_1x8x8::gtSystemInfo = {0};
void LKF_1x8x8::setupHardwareInfo(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->EUCount = 64;
    gtSysInfo->ThreadCount = 64 * LKF::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 8;
    gtSysInfo->L3CacheSizeInKb = 2560;
    gtSysInfo->L3BankCount = 8;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 448;
    gtSysInfo->TotalHsThreads = 448;
    gtSysInfo->TotalDsThreads = 448;
    gtSysInfo->TotalGsThreads = 448;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = LKF::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = LKF::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = LKF::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
    if (setupFeatureTableAndWorkaroundTable) {
        setupFeatureAndWorkaroundTable(hwInfo);
    }
};

const HardwareInfo LKF::hwInfo = LKF_1x8x8::hwInfo;

void setupLKFHardwareInfoImpl(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const std::string &hwInfoConfig) {
    if (hwInfoConfig == "1x8x8") {
        LKF_1x8x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else if (hwInfoConfig == "default") {
        // Default config
        LKF_1x8x8::setupHardwareInfo(hwInfo, setupFeatureTableAndWorkaroundTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*LKF::setupHardwareInfo)(HardwareInfo *, bool, const std::string &) = setupLKFHardwareInfoImpl;
} // namespace NEO
