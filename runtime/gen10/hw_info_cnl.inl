/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "hw_cmds.h"
#include "hw_info.h"
#include "runtime/aub_mem_dump/aub_services.h"
#include "runtime/helpers/engine_node.h"
#include "runtime/memory_manager/memory_constants.h"

namespace OCLRT {

const char *HwMapper<IGFX_CANNONLAKE>::abbreviation = "cnl";

bool isSimulationCNL(unsigned short deviceId) {
    switch (deviceId) {
    case ICNL_3x8_DESK_DEVICE_F0_ID:
    case ICNL_5x8_DESK_DEVICE_F0_ID:
    case ICNL_9x8_DESK_DEVICE_F0_ID:
    case ICNL_13x8_DESK_DEVICE_F0_ID:
        return true;
    }
    return false;
};

const PLATFORM CNL::platform = {
    IGFX_CANNONLAKE,
    PCH_UNKNOWN,
    IGFX_GEN10_CORE,
    IGFX_GEN10_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable CNL::capabilityTable{0,
                                                  83.333,
                                                  21,
                                                  true,
                                                  true,
                                                  true,
                                                  true,
                                                  true,  // ftrSupportsVmeAvcTextureSampler
                                                  true,  // ftrSupportsVmeAvcPreemption
                                                  false, // ftrRenderCompressedBuffers
                                                  false, // ftrRenderCompressedImages
                                                  PreemptionMode::MidThread,
                                                  {true, true},
                                                  &isSimulationCNL,
                                                  true,
                                                  true,                           // forceStatelessCompilationFor32Bit
                                                  {false, 0, false, 0, false, 0}, // KmdNotifyProperties
                                                  true,                           // ftr64KBpages
                                                  EngineType::ENGINE_RCS,         // defaultEngineType
                                                  MemoryConstants::pageSize,      // requiredPreemptionSurfaceSize
                                                  true,
                                                  true, // sourceLevelDebuggerSupported
                                                  CmdServicesMemTraceVersion::DeviceValues::Cnl,
                                                  0,                                 // extraQuantityThreadsPerEU
                                                  MemoryConstants::max48BitAddress}; // gpuAddressSpace

const HardwareInfo CNL_2x5x8::hwInfo = {
    &CNL::platform,
    &emptySkuTable,
    &emptyWaTable,
    &CNL_2x5x8::gtSystemInfo,
    CNL::capabilityTable,
};
GT_SYSTEM_INFO CNL_2x5x8::gtSystemInfo = {0};
void CNL_2x5x8::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 39;
    gtSysInfo->ThreadCount = 39 * CNL::threadsPerEu;
    gtSysInfo->SliceCount = 2;
    gtSysInfo->SubSliceCount = 5;
    gtSysInfo->L3CacheSizeInKb = 1536;
    gtSysInfo->L3BankCount = 6;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = CNL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = CNL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = CNL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};
const HardwareInfo CNL_2x4x8::hwInfo = {
    &CNL::platform,
    &emptySkuTable,
    &emptyWaTable,
    &CNL_2x4x8::gtSystemInfo,
    CNL::capabilityTable,
};
GT_SYSTEM_INFO CNL_2x4x8::gtSystemInfo = {0};
void CNL_2x4x8::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 31;
    gtSysInfo->ThreadCount = 31 * CNL::threadsPerEu;
    gtSysInfo->SliceCount = 2;
    gtSysInfo->SubSliceCount = 4;
    gtSysInfo->L3CacheSizeInKb = 1536;
    gtSysInfo->L3BankCount = 6;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = CNL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = CNL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = CNL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};
const HardwareInfo CNL_1x3x8::hwInfo = {
    &CNL::platform,
    &emptySkuTable,
    &emptyWaTable,
    &CNL_1x3x8::gtSystemInfo,
    CNL::capabilityTable,
};
GT_SYSTEM_INFO CNL_1x3x8::gtSystemInfo = {0};
void CNL_1x3x8::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 23;
    gtSysInfo->ThreadCount = 23 * CNL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 3;
    gtSysInfo->L3CacheSizeInKb = 1536;
    gtSysInfo->L3BankCount = 6;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = CNL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = CNL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = CNL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};
const HardwareInfo CNL_1x2x8::hwInfo = {
    &CNL::platform,
    &emptySkuTable,
    &emptyWaTable,
    &CNL_1x2x8::gtSystemInfo,
    CNL::capabilityTable,
};
GT_SYSTEM_INFO CNL_1x2x8::gtSystemInfo = {0};
void CNL_1x2x8::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 15;
    gtSysInfo->ThreadCount = 15 * CNL::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 2;
    gtSysInfo->L3CacheSizeInKb = 1536;
    gtSysInfo->L3BankCount = 6;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = CNL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = CNL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = CNL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};
const HardwareInfo CNL_4x9x8::hwInfo = {
    &CNL::platform,
    &emptySkuTable,
    &emptyWaTable,
    &CNL_4x9x8::gtSystemInfo,
    CNL::capabilityTable,
};
GT_SYSTEM_INFO CNL_4x9x8::gtSystemInfo = {0};
void CNL_4x9x8::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 71;
    gtSysInfo->ThreadCount = 71 * CNL::threadsPerEu;
    gtSysInfo->SliceCount = 4;
    gtSysInfo->SubSliceCount = 9;
    gtSysInfo->L3CacheSizeInKb = 1536;
    gtSysInfo->L3BankCount = 6;
    gtSysInfo->MaxFillRate = 16;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = CNL::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = CNL::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = CNL::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};
const HardwareInfo CNL::hwInfo = CNL_2x5x8::hwInfo;

void setupCNLHardwareInfoImpl(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable, const std::string &hwInfoConfig) {
    if (hwInfoConfig == "1x2x8") {
        CNL_1x2x8::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "1x3x8") {
        CNL_1x3x8::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "2x5x8") {
        CNL_2x5x8::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "2x4x8") {
        CNL_2x4x8::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "4x9x8") {
        CNL_4x9x8::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "default") {
        // Default config
        CNL_2x5x8::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*CNL::setupHardwareInfo)(GT_SYSTEM_INFO *, FeatureTable *, bool, const std::string &) = setupCNLHardwareInfoImpl;
} // namespace OCLRT
