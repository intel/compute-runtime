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

#include "hw_info_glk.h"
#include "hw_cmds.h"
#include "runtime/aub_mem_dump/aub_services.h"
#include "runtime/helpers/engine_node.h"
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
    0,
    52.083,
    12,
    true,
    true,
    false, // ftrSvm
    true,
    true,  // ftrSupportsVmeAvcTextureSampler
    false, // ftrSupportsVmeAvcPreemption
    false,
    PreemptionMode::MidThread,
    {true, false},
    &isSimulationGLK,
    true,
    false,                             // forceStatelessCompilationFor32Bit
    {true, 30000, false, 0, false, 0}, // KmdNotifyProperties
    false,                             // ftr64KBpages
    EngineType::ENGINE_RCS,            // defaultEngineType
    MemoryConstants::pageSize,         //requiredPreemptionSurfaceSize
    false,                             // isCore
    true,                              // sourceLevelDebuggerSupported
    CmdServicesMemTraceVersion::DeviceValues::Glk};

const HardwareInfo GLK_1x3x6::hwInfo = {
    &GLK::platform,
    &emptySkuTable,
    &emptyWaTable,
    &GLK_1x3x6::gtSystemInfo,
    GLK::capabilityTable,
};
GT_SYSTEM_INFO GLK_1x3x6::gtSystemInfo = {0};
void GLK_1x3x6::setupGtSystemInfo(GT_SYSTEM_INFO *gtSysInfo) {
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
void GLK_1x2x6::setupGtSystemInfo(GT_SYSTEM_INFO *gtSysInfo) {
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
void (*GLK::setupGtSystemInfo)(GT_SYSTEM_INFO *) = GLK_1x3x6::setupGtSystemInfo;
} // namespace OCLRT
