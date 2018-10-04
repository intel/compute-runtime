/*
 * Copyright (C) 2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "hw_info_cfl.h"
#include "runtime/aub_mem_dump/aub_services.h"
#include "runtime/helpers/engine_node.h"
#include "runtime/memory_manager/memory_constants.h"

namespace OCLRT {

const char *HwMapper<IGFX_COFFEELAKE>::abbreviation = "cfl";

bool isSimulationCFL(unsigned short deviceId) {
    return false;
};

const PLATFORM CFL::platform = {
    IGFX_COFFEELAKE,
    PCH_UNKNOWN,
    IGFX_GEN9_CORE,
    IGFX_GEN9_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable CFL::capabilityTable{0,
                                                  83.333,
                                                  21,
                                                  true,
                                                  true,
                                                  true,
                                                  true,
                                                  true,  // ftrSupportsVmeAvcTextureSampler
                                                  false, // ftrSupportsVmeAvcPreemption
                                                  false, // ftrRenderCompressedBuffers
                                                  false, // ftrRenderCompressedImages
                                                  PreemptionMode::MidThread,
                                                  {true, false},
                                                  &isSimulationCFL,
                                                  true,
                                                  true,                           // forceStatelessCompilationFor32Bit
                                                  {false, 0, false, 0, false, 0}, // KmdNotifyProperties
                                                  true,                           // ftr64KBpages
                                                  EngineType::ENGINE_RCS,         // defaultEngineType
                                                  MemoryConstants::pageSize,      // requiredPreemptionSurfaceSize
                                                  true,                           // isCore
                                                  true,                           // sourceLevelDebuggerSupported
                                                  CmdServicesMemTraceVersion::DeviceValues::Cfl,
                                                  0,                                 // extraQuantityThreadsPerEU
                                                  MemoryConstants::max48BitAddress}; // gpuAddressSpace

const HardwareInfo CFL_1x2x6::hwInfo = {
    &CFL::platform,
    &emptySkuTable,
    &emptyWaTable,
    &CFL_1x2x6::gtSystemInfo,
    CFL::capabilityTable,
};
GT_SYSTEM_INFO CFL_1x2x6::gtSystemInfo = {0};
void CFL_1x2x6::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 11;
    gtSysInfo->ThreadCount = 11 * CFL::threadsPerEu;
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
    gtSysInfo->MaxEuPerSubSlice = CFL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = CFL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = CFL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo CFL_1x3x6::hwInfo = {
    &CFL::platform,
    &emptySkuTable,
    &emptyWaTable,
    &CFL_1x3x6::gtSystemInfo,
    CFL::capabilityTable,
};
GT_SYSTEM_INFO CFL_1x3x6::gtSystemInfo = {0};
void CFL_1x3x6::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 17;
    gtSysInfo->ThreadCount = 17 * CFL::threadsPerEu;
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
    gtSysInfo->MaxEuPerSubSlice = CFL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = CFL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = CFL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo CFL_1x3x8::hwInfo = {
    &CFL::platform,
    &emptySkuTable,
    &emptyWaTable,
    &CFL_1x3x8::gtSystemInfo,
    CFL::capabilityTable,
};
GT_SYSTEM_INFO CFL_1x3x8::gtSystemInfo = {0};
void CFL_1x3x8::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 23;
    gtSysInfo->ThreadCount = 23 * CFL::threadsPerEu;
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
    gtSysInfo->MaxEuPerSubSlice = CFL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = CFL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = CFL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo CFL_2x3x8::hwInfo = {
    &CFL::platform,
    &emptySkuTable,
    &emptyWaTable,
    &CFL_2x3x8::gtSystemInfo,
    CFL::capabilityTable,
};
GT_SYSTEM_INFO CFL_2x3x8::gtSystemInfo = {0};
void CFL_2x3x8::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 47;
    gtSysInfo->ThreadCount = 47 * CFL::threadsPerEu;
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
    gtSysInfo->MaxEuPerSubSlice = CFL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = CFL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = CFL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo CFL_3x3x8::hwInfo = {
    &CFL::platform,
    &emptySkuTable,
    &emptyWaTable,
    &CFL_3x3x8::gtSystemInfo,
    CFL::capabilityTable,
};
GT_SYSTEM_INFO CFL_3x3x8::gtSystemInfo = {0};
void CFL_3x3x8::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 71;
    gtSysInfo->ThreadCount = 71 * CFL::threadsPerEu;
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
    gtSysInfo->MaxEuPerSubSlice = CFL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = CFL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = CFL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo CFL::hwInfo = CFL_1x3x6::hwInfo;

void setupCFLHardwareInfoImpl(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable, const std::string &hwInfoConfig) {
    if (hwInfoConfig == "1x3x8") {
        CFL_1x3x8::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "2x3x8") {
        CFL_2x3x8::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "3x3x8") {
        CFL_3x3x8::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "1x2x6") {
        CFL_1x2x6::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "1x3x6") {
        CFL_1x3x6::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "default") {
        // Default config
        CFL_1x3x6::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*CFL::setupHardwareInfo)(GT_SYSTEM_INFO *, FeatureTable *, bool, const std::string &) = setupCFLHardwareInfoImpl;
} // namespace OCLRT
