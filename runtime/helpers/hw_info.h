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

namespace NEO {

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
    aub_stream::EngineType defaultEngineType;
    uint32_t maxRenderFrequency;
    unsigned int clVersionSupport;
    uint32_t aubDeviceId;
    uint32_t extraQuantityThreadsPerEU;
    uint32_t slmSize;
    bool blitterOperationsSupported;
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
    bool supportsImages;
};

struct HardwareCapabilities {
    size_t image3DMaxWidth;
    size_t image3DMaxHeight;
    uint64_t maxMemAllocSize;
    bool isStatelesToStatefullWithOffsetSupported;
};

struct HardwareInfo {
    HardwareInfo() = default;
    HardwareInfo(const PLATFORM *platform, const FeatureTable *featureTable, const WorkaroundTable *workaroundTable,
                 const GT_SYSTEM_INFO *gtSystemInfo, const RuntimeCapabilityTable &capabilityTable);

    PLATFORM platform = {};
    FeatureTable featureTable = {};
    WorkaroundTable workaroundTable = {};
    alignas(4) GT_SYSTEM_INFO gtSystemInfo = {};

    RuntimeCapabilityTable capabilityTable = {};
};

template <PRODUCT_FAMILY product>
struct HwMapper {};

template <GFXCORE_FAMILY gfxFamily>
struct GfxFamilyMapper {};

// Global table of hardware prefixes
extern bool familyEnabled[IGFX_MAX_CORE];
extern const char *familyName[IGFX_MAX_CORE];
extern const char *hardwarePrefix[];
extern const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT];
extern void (*hardwareInfoSetup[IGFX_MAX_PRODUCT])(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const std::string &hwInfoConfig);

template <GFXCORE_FAMILY gfxFamily>
struct EnableGfxFamilyHw {
    EnableGfxFamilyHw() {
        familyEnabled[gfxFamily] = true;
        familyName[gfxFamily] = GfxFamilyMapper<gfxFamily>::name;
    }
};

const char *getPlatformType(const HardwareInfo &hwInfo);
bool getHwInfoForPlatformString(const char *str, const HardwareInfo *&hwInfoIn);
aub_stream::EngineType getChosenEngineType(const HardwareInfo &hwInfo);
} // namespace NEO
