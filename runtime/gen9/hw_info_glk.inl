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

const RuntimeCapabilityTable GLK::capabilityTable{0,
                                                  52.083,
                                                  12,
                                                  true,
                                                  true,
                                                  false, // ftrSvm
                                                  true,
                                                  true,  // ftrSupportsVmeAvcTextureSampler
                                                  false, // ftrSupportsVmeAvcPreemption
                                                  false, // ftrRenderCompressedBuffers
                                                  false, // ftrRenderCompressedImages
                                                  PreemptionMode::MidThread,
                                                  {true, false},
                                                  &isSimulationGLK,
                                                  true,
                                                  false,                             // forceStatelessCompilationFor32Bit
                                                  {true, 30000, false, 0, false, 0}, // KmdNotifyProperties
                                                  false,                             // ftr64KBpages
                                                  EngineType::ENGINE_RCS,            // defaultEngineType
                                                  MemoryConstants::pageSize,         // requiredPreemptionSurfaceSize
                                                  false,                             // isCore
                                                  true,                              // sourceLevelDebuggerSupported
                                                  CmdServicesMemTraceVersion::DeviceValues::Glk,
                                                  0,                                 // extraQuantityThreadsPerEU
                                                  true,                              // SupportsVme
                                                  MemoryConstants::max48BitAddress}; // gpuAddressSpace

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
