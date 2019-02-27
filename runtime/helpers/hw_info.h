/*
 * Copyright (C) 2018-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "runtime/command_stream/preemption_mode.h"
#include "runtime/helpers/kmd_notify_properties.h"

#include "engine_node.h"
#include "gtsysinfo.h"
#include "igfxfmid.h"
#include "sku_info.h"

#include <cstddef>
#include <string>

namespace OCLRT {

struct WhitelistedRegisters {
    bool csChicken1_0x2580;
    bool chicken0hdc_0xE5F0;
};

struct RuntimeCapabilityTable {
    KmdNotifyProperties kmdNotifyProperties;
    WhitelistedRegisters whitelistedRegisters;
    uint64_t gpuAddressSpace;
    double defaultProfilingTimerResolution;
    size_t requiredPreemptionSurfaceSize;
    bool (*isSimulation)(unsigned short);
    PreemptionMode defaultPreemptionMode;
    EngineType defaultEngineType;
    uint32_t maxRenderFrequency;
    unsigned int clVersionSupport;
    uint32_t aubDeviceId;
    uint32_t extraQuantityThreadsPerEU;
    uint32_t slmSize;
    bool ftrSupportsFP64;
    bool ftrSupports64BitMath;
    bool ftrSvm;
    bool ftrSupportsCoherency;
    bool ftrSupportsVmeAvcTextureSampler;
    bool ftrSupportsVmeAvcPreemption;
    bool ftrRenderCompressedBuffers;
    bool ftrRenderCompressedImages;
    bool ftr64KBpages;
    bool instrumentationEnabled;
    bool forceStatelessCompilationFor32Bit;
    bool isCore;
    bool sourceLevelDebuggerSupported;
    bool supportsVme;
    bool supportCacheFlushAfterWalker;
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
extern void (*hardwareInfoSetup[IGFX_MAX_PRODUCT])(GT_SYSTEM_INFO *gtSystemInfo, FeatureTable *featureTable, bool setupFeatureTable, const std::string &hwInfoConfig);

template <GFXCORE_FAMILY gfxFamily>
struct EnableGfxFamilyHw {
    EnableGfxFamilyHw() {
        familyEnabled[gfxFamily] = true;
        familyName[gfxFamily] = GfxFamilyMapper<gfxFamily>::name;
    }
};

const char *getPlatformType(const HardwareInfo &hwInfo);
bool getHwInfoForPlatformString(const char *str, const HardwareInfo *&hwInfoIn);
EngineType getChosenEngineType(const HardwareInfo &hwInfo);
} // namespace OCLRT
