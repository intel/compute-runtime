/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "engine_node.h"
#include "hw_cmds.h"
#include "hw_info_glk.h"
#include "runtime/aub_mem_dump/aub_services.h"
#include "runtime/memory_manager/memory_constants.h"

namespace OCLRT {

const char *HwMapper<IGFX_GEMINILAKE>::abbreviation = "glk";

bool isSimulationGLK(unsigned short deviceId) {
    return false;
};

const PLATFORM GLK::platform = {
    IGFX_GEMINILAKE,
    PCH_UNKNOWN,
    IGFX_GEN9_CORE,
    IGFX_GEN9_CORE,
    PLATFORM_MOBILE, // default init
    0,               // usDeviceID
    0,               // usRevId. 0 sets the stepping to A0
    0,               // usDeviceID_PCH
    0,               // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable GLK::capabilityTable{
    {30000, 0, 0, true, false, false},             // kmdNotifyProperties
    {true, false},                                 // whitelistedRegisters
    MemoryConstants::max48BitAddress,              // gpuAddressSpace
    52.083,                                        // defaultProfilingTimerResolution
    MemoryConstants::pageSize,                     // requiredPreemptionSurfaceSize
    &isSimulationGLK,                              // isSimulation
    PreemptionMode::MidThread,                     // defaultPreemptionMode
    EngineType::ENGINE_RCS,                        // defaultEngineType
    0,                                             // maxRenderFrequency
    12,                                            // clVersionSupport
    CmdServicesMemTraceVersion::DeviceValues::Glk, // aubDeviceId
    0,                                             // extraQuantityThreadsPerEU
    64,                                            // slmSize
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

const HardwareInfo GLK_1x3x6::hwInfo = {
    &GLK::platform,
    &emptySkuTable,
    &emptyWaTable,
    &GLK_1x3x6::gtSystemInfo,
    GLK::capabilityTable,
};
GT_SYSTEM_INFO GLK_1x3x6::gtSystemInfo = {0};
void GLK_1x3x6::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 18;
    gtSysInfo->ThreadCount = 18 * GLK::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 3;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 2;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 112;
    gtSysInfo->TotalHsThreads = 112;
    gtSysInfo->TotalDsThreads = 112;
    gtSysInfo->TotalGsThreads = 112;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = GLK::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = GLK::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = GLK::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo GLK_1x2x6::hwInfo = {
    &GLK::platform,
    &emptySkuTable,
    &emptyWaTable,
    &GLK_1x2x6::gtSystemInfo,
    GLK::capabilityTable,
};
GT_SYSTEM_INFO GLK_1x2x6::gtSystemInfo = {0};
void GLK_1x2x6::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 12;
    gtSysInfo->ThreadCount = 12 * GLK::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 2;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 2;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 112;
    gtSysInfo->TotalHsThreads = 112;
    gtSysInfo->TotalDsThreads = 112;
    gtSysInfo->TotalGsThreads = 112;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = GLK::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = GLK::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = GLK::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo GLK::hwInfo = GLK_1x3x6::hwInfo;

void setupGLKHardwareInfoImpl(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable, const std::string &hwInfoConfig) {
    if (hwInfoConfig == "1x2x6") {
        GLK_1x2x6::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "1x3x6") {
        GLK_1x3x6::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "default") {
        // Default config
        GLK_1x3x6::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*GLK::setupHardwareInfo)(GT_SYSTEM_INFO *, FeatureTable *, bool, const std::string &) = setupGLKHardwareInfoImpl;
} // namespace OCLRT
