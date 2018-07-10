/*
 * Copyright (c) 2018, Intel Corporation
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

#pragma once
#include "igfxfmid.h"
#include "gtsysinfo.h"
#include "sku_info.h"

#include "runtime/helpers/engine_node.h"
#include "runtime/helpers/kmd_notify_properties.h"
#include <cstddef>

namespace OCLRT {

enum class PreemptionMode : uint32_t {
    // Keep in sync with ForcePreemptionMode debug variable
    Initial = 0,
    Disabled = 1,
    MidBatch,
    ThreadGroup,
    MidThread,
};

struct WhitelistedRegisters {
    bool csChicken1_0x2580;
    bool chicken0hdc_0xE5F0;
};

struct RuntimeCapabilityTable {
    uint32_t maxRenderFrequency;
    double defaultProfilingTimerResolution;

    unsigned int clVersionSupport;
    bool ftrSupportsFP64;
    bool ftrSupports64BitMath;
    bool ftrSvm;
    bool ftrSupportsCoherency;
    bool ftrSupportsVmeAvcTextureSampler;
    bool ftrSupportsVmeAvcPreemption;
    bool ftrCompression;
    PreemptionMode defaultPreemptionMode;
    WhitelistedRegisters whitelistedRegisters;

    bool (*isSimulation)(unsigned short);
    bool instrumentationEnabled;

    bool forceStatelessCompilationFor32Bit;

    KmdNotifyProperties kmdNotifyProperties;

    bool ftr64KBpages;

    EngineType defaultEngineType;

    size_t requiredPreemptionSurfaceSize;
    bool isCore;
    bool sourceLevelDebuggerSupported;
    uint32_t aubDeviceId;
};

struct HardwareCapabilities {
    size_t image3DMaxWidth;
    size_t image3DMaxHeight;
    uint64_t maxMemAllocSize;
    bool isStatelesToStatefullWithOffsetSupported;
};

struct HardwareInfo {
    HardwareInfo() = default;
    HardwareInfo(const PLATFORM *platform, const FeatureTable *skuTable, const WorkaroundTable *waTable,
                 const GT_SYSTEM_INFO *sysInfo, const RuntimeCapabilityTable &capabilityTable);

    const PLATFORM *pPlatform = nullptr;
    const FeatureTable *pSkuTable = nullptr;
    const WorkaroundTable *pWaTable = nullptr;
    const GT_SYSTEM_INFO *pSysInfo = nullptr;

    RuntimeCapabilityTable capabilityTable = {};
};

extern const WorkaroundTable emptyWaTable;
extern const FeatureTable emptySkuTable;

template <PRODUCT_FAMILY product>
struct HwMapper {};

template <GFXCORE_FAMILY gfxFamily>
struct GfxFamilyMapper {};

// Global table of hardware prefixes
extern bool familyEnabled[IGFX_MAX_CORE];
extern const char *familyName[IGFX_MAX_CORE];
extern const char *hardwarePrefix[];
extern const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT];
extern void (*hardwareInfoSetupGt[IGFX_MAX_PRODUCT])(GT_SYSTEM_INFO *);

template <GFXCORE_FAMILY gfxFamily>
struct EnableGfxFamilyHw {
    EnableGfxFamilyHw() {
        familyEnabled[gfxFamily] = true;
        familyName[gfxFamily] = GfxFamilyMapper<gfxFamily>::name;
    }
};

const char *getPlatformType(const HardwareInfo &hwInfo);
bool getHwInfoForPlatformString(const char *str, const HardwareInfo *&hwInfoIn);
} // namespace OCLRT
