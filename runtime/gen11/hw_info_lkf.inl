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

const HardwareInfo LKF_1x8x8::hwInfo = {
    &LKF::platform,
    &emptySkuTable,
    &emptyWaTable,
    &LKF_1x8x8::gtSystemInfo,
    LKF::capabilityTable,
};
GT_SYSTEM_INFO LKF_1x8x8::gtSystemInfo = {0};
void LKF_1x8x8::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
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
};

const HardwareInfo LKF::hwInfo = LKF_1x8x8::hwInfo;

void setupLKFHardwareInfoImpl(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable, const std::string &hwInfoConfig) {
    if (hwInfoConfig == "1x8x8") {
        LKF_1x8x8::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "default") {
        // Default config
        LKF_1x8x8::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
} // namespace NEO

void (*LKF::setupHardwareInfo)(GT_SYSTEM_INFO *, FeatureTable *, bool, const std::string &) = setupLKFHardwareInfoImpl;
} // namespace NEO
