/*
 * Copyright (c) 2017 - 2018, Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "hw_info.h"
#include "hw_cmds.h"
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

const RuntimeCapabilityTable BDW::capabilityTable{
    0,
    80,
    21,
    true,
    true,
    true,
    true,
    false, // ftrSupportsVmeAvcTextureSampler
    false, // ftrSupportsVmeAvcPreemption
    false,
    PreemptionMode::Disabled,
    {false, false},
    &isSimulationBDW,
    true,
    true,                                    // forceStatelessCompilationFor32Bit
    {true, 50000, true, 5000, true, 200000}, // KmdNotifyProperties
    false,                                   // ftr64KBpages
    EngineType::ENGINE_RCS,                  // defaultEngineType
    MemoryConstants::pageSize,               //requiredPreemptionSurfaceSize
    true                                     // isCore
};

const HardwareInfo BDW_1x2x6::hwInfo = {
    &BDW::platform,
    &emptySkuTable,
    &emptyWaTable,
    &BDW_1x2x6::gtSystemInfo,
    BDW::capabilityTable,
};
GT_SYSTEM_INFO BDW_1x2x6::gtSystemInfo = {0};
void BDW_1x2x6::setupGtSystemInfo(GT_SYSTEM_INFO *gtSysInfo) {
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
void BDW_1x3x6::setupGtSystemInfo(GT_SYSTEM_INFO *gtSysInfo) {
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
void BDW_1x3x8::setupGtSystemInfo(GT_SYSTEM_INFO *gtSysInfo) {
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
void BDW_2x3x8::setupGtSystemInfo(GT_SYSTEM_INFO *gtSysInfo) {
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
void (*BDW::setupGtSystemInfo)(GT_SYSTEM_INFO *) = BDW_1x3x8::setupGtSystemInfo;
} // namespace OCLRT
