/*
 * Copyright (C) 2018-2022 Intel Corporation
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
    uint64_t sharedSystemMemCapabilities;
    double defaultProfilingTimerResolution;
    size_t requiredPreemptionSurfaceSize;
    const char *platformType;
    const char *deviceName;
    PreemptionMode defaultPreemptionMode;
    aub_stream::EngineType defaultEngineType;
    uint32_t maxRenderFrequency;
    unsigned int clVersionSupport;
    uint32_t aubDeviceId;
    uint32_t extraQuantityThreadsPerEU;
    uint32_t slmSize;
    uint32_t grfSize;
    uint32_t timestampValidBits;
    uint32_t kernelTimestampValidBits;
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
    bool debuggerSupported;
    bool supportsVme;
    bool supportCacheFlushAfterWalker;
    bool supportsImages;
    bool supportsDeviceEnqueue;
    bool supportsPipes;
    bool supportsOcl21Features;
    bool supportsOnDemandPageFaults;
    bool supportsIndependentForwardProgress;
    bool hostPtrTrackingEnabled;
    bool levelZeroSupported;
    bool isIntegratedDevice;
    bool supportsMediaBlock;
    bool p2pAccessSupported;
    bool p2pAtomicAccessSupported;
    bool fusedEuEnabled;
    bool l0DebuggerSupported;
};

inline bool operator==(const RuntimeCapabilityTable &lhs, const RuntimeCapabilityTable &rhs) {
    bool result = 1;

    for (size_t i = 0; i < (sizeof(lhs.directSubmissionEngines.data) / sizeof(*lhs.directSubmissionEngines.data)); ++i) {
        result &= (lhs.directSubmissionEngines.data[i].engineSupported == rhs.directSubmissionEngines.data[i].engineSupported);
        result &= (lhs.directSubmissionEngines.data[i].submitOnInit == rhs.directSubmissionEngines.data[i].submitOnInit);
        result &= (lhs.directSubmissionEngines.data[i].useNonDefault == rhs.directSubmissionEngines.data[i].useNonDefault);
        result &= (lhs.directSubmissionEngines.data[i].useRootDevice == rhs.directSubmissionEngines.data[i].useRootDevice);
        result &= (lhs.directSubmissionEngines.data[i].useInternal == rhs.directSubmissionEngines.data[i].useInternal);
        result &= (lhs.directSubmissionEngines.data[i].useLowPriority == rhs.directSubmissionEngines.data[i].useLowPriority);
    }

    result &= (lhs.kmdNotifyProperties.delayKmdNotifyMicroseconds == rhs.kmdNotifyProperties.delayKmdNotifyMicroseconds);
    result &= (lhs.kmdNotifyProperties.delayQuickKmdSleepMicroseconds == rhs.kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    result &= (lhs.kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds == rhs.kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
    result &= (lhs.kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission == rhs.kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    result &= (lhs.kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds == rhs.kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);
    result &= (lhs.kmdNotifyProperties.enableKmdNotify == rhs.kmdNotifyProperties.enableKmdNotify);
    result &= (lhs.kmdNotifyProperties.enableQuickKmdSleep == rhs.kmdNotifyProperties.enableQuickKmdSleep);
    result &= (lhs.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits == rhs.kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    result &= (lhs.gpuAddressSpace == rhs.gpuAddressSpace);
    result &= (lhs.sharedSystemMemCapabilities == rhs.sharedSystemMemCapabilities);
    result &= (lhs.defaultProfilingTimerResolution == rhs.defaultProfilingTimerResolution);
    result &= (lhs.requiredPreemptionSurfaceSize == rhs.requiredPreemptionSurfaceSize);
    result &= (lhs.defaultPreemptionMode == rhs.defaultPreemptionMode);
    result &= (lhs.defaultEngineType == rhs.defaultEngineType);
    result &= (lhs.maxRenderFrequency == rhs.maxRenderFrequency);
    result &= (lhs.clVersionSupport == rhs.clVersionSupport);
    result &= (lhs.aubDeviceId == rhs.aubDeviceId);
    result &= (lhs.extraQuantityThreadsPerEU == rhs.extraQuantityThreadsPerEU);
    result &= (lhs.slmSize == rhs.slmSize);
    result &= (lhs.grfSize == rhs.grfSize);
    result &= (lhs.timestampValidBits == rhs.timestampValidBits);
    result &= (lhs.kernelTimestampValidBits == rhs.kernelTimestampValidBits);
    result &= (lhs.blitterOperationsSupported == rhs.blitterOperationsSupported);
    result &= (lhs.ftrSupportsInteger64BitAtomics == rhs.ftrSupportsInteger64BitAtomics);
    result &= (lhs.ftrSupportsFP64 == rhs.ftrSupportsFP64);
    result &= (lhs.ftrSupports64BitMath == rhs.ftrSupports64BitMath);
    result &= (lhs.ftrSvm == rhs.ftrSvm);
    result &= (lhs.ftrSupportsCoherency == rhs.ftrSupportsCoherency);
    result &= (lhs.ftrSupportsVmeAvcTextureSampler == rhs.ftrSupportsVmeAvcTextureSampler);
    result &= (lhs.ftrSupportsVmeAvcPreemption == rhs.ftrSupportsVmeAvcPreemption);
    result &= (lhs.ftrRenderCompressedBuffers == rhs.ftrRenderCompressedBuffers);
    result &= (lhs.ftrRenderCompressedImages == rhs.ftrRenderCompressedImages);
    result &= (lhs.ftr64KBpages == rhs.ftr64KBpages);
    result &= (lhs.instrumentationEnabled == rhs.instrumentationEnabled);
    result &= (lhs.platformType == rhs.platformType);
    result &= (lhs.deviceName == rhs.deviceName);
    result &= (lhs.debuggerSupported == rhs.debuggerSupported);
    result &= (lhs.supportsVme == rhs.supportsVme);
    result &= (lhs.supportCacheFlushAfterWalker == rhs.supportCacheFlushAfterWalker);
    result &= (lhs.supportsImages == rhs.supportsImages);
    result &= (lhs.supportsDeviceEnqueue == rhs.supportsDeviceEnqueue);
    result &= (lhs.supportsPipes == rhs.supportsPipes);
    result &= (lhs.supportsOcl21Features == rhs.supportsOcl21Features);
    result &= (lhs.supportsOnDemandPageFaults == rhs.supportsOnDemandPageFaults);
    result &= (lhs.supportsIndependentForwardProgress == rhs.supportsIndependentForwardProgress);
    result &= (lhs.hostPtrTrackingEnabled == rhs.hostPtrTrackingEnabled);
    result &= (lhs.levelZeroSupported == rhs.levelZeroSupported);
    result &= (lhs.isIntegratedDevice == rhs.isIntegratedDevice);
    result &= (lhs.supportsMediaBlock == rhs.supportsMediaBlock);
    result &= (lhs.fusedEuEnabled == rhs.fusedEuEnabled);
    result &= (lhs.l0DebuggerSupported == rhs.l0DebuggerSupported);

    return result;
}

struct HardwareInfo {
    HardwareInfo() = default;
    HardwareInfo(const PLATFORM *platform, const FeatureTable *featureTable, const WorkaroundTable *workaroundTable,
                 const GT_SYSTEM_INFO *gtSystemInfo, const RuntimeCapabilityTable &capabilityTable);

    PLATFORM platform = {};
    FeatureTable featureTable = {};
    WorkaroundTable workaroundTable = {};
    alignas(4) GT_SYSTEM_INFO gtSystemInfo = {};

    alignas(8) RuntimeCapabilityTable capabilityTable = {};
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
extern void (*hardwareInfoBaseSetup[IGFX_MAX_PRODUCT])(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable);

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
void overridePlatformName(std::string &name);
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
