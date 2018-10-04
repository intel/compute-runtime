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

const char *HwMapper<IGFX_BROADWELL>::abbreviation = "bdw";

bool isSimulationBDW(unsigned short deviceId) {
    switch (deviceId) {
    case IBDW_GT0_DESK_DEVICE_F0_ID:
    case IBDW_GT1_DESK_DEVICE_F0_ID:
    case IBDW_GT2_DESK_DEVICE_F0_ID:
    case IBDW_GT3_DESK_DEVICE_F0_ID:
    case IBDW_GT4_DESK_DEVICE_F0_ID:
        return true;
    }
    return false;
};

const PLATFORM BDW::platform = {
    IGFX_BROADWELL,
    PCH_UNKNOWN,
    IGFX_GEN8_CORE,
    IGFX_GEN8_CORE,
    PLATFORM_NONE, // default init
    0,             // usDeviceID
    0,             // usRevId. 0 sets the stepping to A0
    0,             // usDeviceID_PCH
    0,             // usRevId_PCH
    GTTYPE_UNDEFINED};

const RuntimeCapabilityTable BDW::capabilityTable{0,
                                                  80,
                                                  21,
                                                  true,
                                                  true,
                                                  true,
                                                  true,
                                                  false, // ftrSupportsVmeAvcTextureSampler
                                                  false, // ftrSupportsVmeAvcPreemption
                                                  false, // ftrRenderCompressedBuffers
                                                  false, // ftrRenderCompressedImages
                                                  PreemptionMode::Disabled,
                                                  {false, false},
                                                  &isSimulationBDW,
                                                  true,
                                                  true,                                    // forceStatelessCompilationFor32Bit
                                                  {true, 50000, true, 5000, true, 200000}, // KmdNotifyProperties
                                                  false,                                   // ftr64KBpages
                                                  EngineType::ENGINE_RCS,                  // defaultEngineType
                                                  MemoryConstants::pageSize,               // requiredPreemptionSurfaceSize
                                                  true,                                    // isCore
                                                  false,                                   // sourceLevelDebuggerSupported
                                                  CmdServicesMemTraceVersion::DeviceValues::Bdw,
                                                  0,                                 // extraQuantityThreadsPerEU
                                                  MemoryConstants::max48BitAddress}; // gpuAddressSpace

const HardwareInfo BDW_1x2x6::hwInfo = {
    &BDW::platform,
    &emptySkuTable,
    &emptyWaTable,
    &BDW_1x2x6::gtSystemInfo,
    BDW::capabilityTable,
};
GT_SYSTEM_INFO BDW_1x2x6::gtSystemInfo = {0};
void BDW_1x2x6::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 12;
    gtSysInfo->ThreadCount = 12 * BDW::threadsPerEu;
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
    gtSysInfo->MaxEuPerSubSlice = BDW::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = BDW::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = BDW::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo BDW_1x3x6::hwInfo = {
    &BDW::platform,
    &emptySkuTable,
    &emptyWaTable,
    &BDW_1x3x6::gtSystemInfo,
    BDW::capabilityTable,
};
GT_SYSTEM_INFO BDW_1x3x6::gtSystemInfo = {0};
void BDW_1x3x6::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 18;
    gtSysInfo->ThreadCount = 18 * BDW::threadsPerEu;
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
    gtSysInfo->MaxEuPerSubSlice = BDW::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = BDW::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = BDW::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo BDW_1x3x8::hwInfo = {
    &BDW::platform,
    &emptySkuTable,
    &emptyWaTable,
    &BDW_1x3x8::gtSystemInfo,
    BDW::capabilityTable,
};
GT_SYSTEM_INFO BDW_1x3x8::gtSystemInfo = {0};
void BDW_1x3x8::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 23;
    gtSysInfo->ThreadCount = 23 * BDW::threadsPerEu;
    gtSysInfo->SliceCount = 1;
    gtSysInfo->SubSliceCount = 3;
    gtSysInfo->L3CacheSizeInKb = 384;
    gtSysInfo->L3BankCount = 2;
    gtSysInfo->MaxFillRate = 8;
    gtSysInfo->TotalVsThreads = 336;
    gtSysInfo->TotalHsThreads = 336;
    gtSysInfo->TotalDsThreads = 336;
    gtSysInfo->TotalGsThreads = 336;
    gtSysInfo->TotalPsThreadsWindowerRange = 64;
    gtSysInfo->CsrSizeInMb = 8;
    gtSysInfo->MaxEuPerSubSlice = BDW::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = BDW::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = BDW::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo BDW_2x3x8::hwInfo = {
    &BDW::platform,
    &emptySkuTable,
    &emptyWaTable,
    &BDW_2x3x8::gtSystemInfo,
    BDW::capabilityTable,
};
GT_SYSTEM_INFO BDW_2x3x8::gtSystemInfo = {0};
void BDW_2x3x8::setupHardwareInfo(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable) {
    gtSysInfo->EUCount = 47;
    gtSysInfo->ThreadCount = 47 * BDW::threadsPerEu;
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
    gtSysInfo->MaxEuPerSubSlice = BDW::maxEuPerSubslice;
    gtSysInfo->MaxSlicesSupported = BDW::maxSlicesSupported;
    gtSysInfo->MaxSubSlicesSupported = BDW::maxSubslicesSupported;
    gtSysInfo->IsL3HashModeEnabled = false;
    gtSysInfo->IsDynamicallyPopulated = false;
};

const HardwareInfo BDW::hwInfo = BDW_1x3x8::hwInfo;

void setupBDWHardwareInfoImpl(GT_SYSTEM_INFO *gtSysInfo, FeatureTable *featureTable, bool setupFeatureTable, const std::string &hwInfoConfig) {
    if (hwInfoConfig == "2x3x8") {
        BDW_2x3x8::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "1x3x8") {
        BDW_1x3x8::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "1x3x6") {
        BDW_1x3x6::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "1x2x6") {
        BDW_1x2x6::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else if (hwInfoConfig == "default") {
        // Default config
        BDW_1x3x8::setupHardwareInfo(gtSysInfo, featureTable, setupFeatureTable);
    } else {
        UNRECOVERABLE_IF(true);
    }
}

void (*BDW::setupHardwareInfo)(GT_SYSTEM_INFO *, FeatureTable *, bool, const std::string &) = setupBDWHardwareInfoImpl;
} // namespace OCLRT
