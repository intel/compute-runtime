/*
 * Copyright (C) 2018-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/command_stream/preemption_mode.h"
#include "shared/source/direct_submission/direct_submission_properties.h"
#include "shared/source/helpers/kmd_notify_properties.h"

#include "engine_node.h"
#include "gtsysinfo.h"
#include "igfxfmid.h"
#include "sku_info.h"

#include <cstddef>
#include <string>

namespace NEO {

struct RuntimeCapabilityTable {
    DirectSubmissionProperyEngines directSubmissionEngines;
    KmdNotifyProperties kmdNotifyProperties;
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
    uint32_t grfSize;
    bool blitterOperationsSupported;
    bool ftrSupportsInteger64BitAtomics;
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
    const char *platformType;
    bool debuggerSupported;
    bool supportsVme;
    bool supportCacheFlushAfterWalker;
    bool supportsImages;
    bool supportsDeviceEnqueue;
    bool hostPtrTrackingEnabled;
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
extern const char *hardwarePrefix[IGFX_MAX_PRODUCT];
extern uint64_t defaultHardwareInfoConfigTable[IGFX_MAX_PRODUCT];
extern const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT];
extern void (*hardwareInfoSetup[IGFX_MAX_PRODUCT])(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig);

template <GFXCORE_FAMILY gfxFamily>
struct EnableGfxFamilyHw {
    EnableGfxFamilyHw() {
        familyEnabled[gfxFamily] = true;
        familyName[gfxFamily] = GfxFamilyMapper<gfxFamily>::name;
    }
};

bool getHwInfoForPlatformString(std::string &platform, const HardwareInfo *&hwInfoIn);
void setHwInfoValuesFromConfig(const uint64_t hwInfoConfig, HardwareInfo &hwInfoIn);
bool parseHwInfoConfigString(const std::string &hwInfoConfigStr, uint64_t &hwInfoConfig);
aub_stream::EngineType getChosenEngineType(const HardwareInfo &hwInfo);
const std::string getFamilyNameWithType(const HardwareInfo &hwInfo);

// Utility conversion
template <PRODUCT_FAMILY productFamily>
struct ToGfxCoreFamily {
    static const GFXCORE_FAMILY gfxCoreFamily =
        static_cast<GFXCORE_FAMILY>(NEO::HwMapper<productFamily>::gfxFamily);
    static constexpr GFXCORE_FAMILY get() { return gfxCoreFamily; }
};
} // namespace NEO
