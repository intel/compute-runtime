/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/direct_submission/direct_submission_properties.h"
#include "shared/source/helpers/hw_ip_version.h"
#include "shared/source/helpers/kmd_notify_properties.h"

#include "gtsysinfo.h"
#include "neo_igfxfmid.h"
#include "sku_info.h"

namespace NEO {

enum PreemptionMode : uint32_t;
class ReleaseHelper;

struct RuntimeCapabilityTable {
    DirectSubmissionProperyEngines directSubmissionEngines;
    KmdNotifyProperties kmdNotifyProperties;
    uint64_t gpuAddressSpace;
    uint64_t sharedSystemMemCapabilities;
    size_t requiredPreemptionSurfaceSize;
    const char *deviceName;
    const char *preferredPlatformName;
    PreemptionMode defaultPreemptionMode;
    aub_stream::EngineType defaultEngineType;
    uint32_t maxRenderFrequency;
    unsigned int clVersionSupport;
    uint32_t aubDeviceId;
    uint32_t extraQuantityThreadsPerEU;
    uint32_t maxProgrammableSlmSize;
    uint32_t grfSize;
    uint32_t timestampValidBits;
    uint32_t kernelTimestampValidBits;
    bool blitterOperationsSupported;
    bool ftrSupportsInteger64BitAtomics;
    bool ftrSupportsFP64;
    bool ftrSupportsFP64Emulation;
    bool ftrSupports64BitMath;
    bool ftrSupportsCoherency;
    bool ftrRenderCompressedBuffers;
    bool ftrRenderCompressedImages;
    bool instrumentationEnabled;
    bool supportCacheFlushAfterWalker;
    bool supportsImages;
    bool supportsOcl21Features;
    bool supportsOnDemandPageFaults;
    bool supportsIndependentForwardProgress;
    bool isIntegratedDevice;
    bool supportsMediaBlock;
    bool p2pAccessSupported;
    bool p2pAtomicAccessSupported;
    bool fusedEuEnabled;
    bool l0DebuggerSupported;
    bool supportsFloatAtomics;
    uint32_t cxlType;

    bool operator==(const RuntimeCapabilityTable &) const = default;
};

struct HardwareInfo { // NOLINT(clang-analyzer-optin.performance.Padding)
    HardwareInfo() = default;
    HardwareInfo(const PLATFORM *platform, const FeatureTable *featureTable, const WorkaroundTable *workaroundTable,
                 const GT_SYSTEM_INFO *gtSystemInfo, const RuntimeCapabilityTable &capabilityTable);

    PLATFORM platform{};
    FeatureTable featureTable{};
    WorkaroundTable workaroundTable{};
    alignas(4) GT_SYSTEM_INFO gtSystemInfo{};
    alignas(8) RuntimeCapabilityTable capabilityTable{};
    alignas(8) HardwareIpVersion ipVersion{};
    alignas(8) HardwareIpVersion ipVersionOverrideExposedToTheApplication{};
};

// Global table of hardware prefixes
extern bool familyEnabled[IGFX_MAX_CORE];
extern const char *hardwarePrefix[IGFX_MAX_PRODUCT];
extern const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT];
extern void (*hardwareInfoSetup[IGFX_MAX_PRODUCT])(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, uint64_t hwInfoConfig, const ReleaseHelper *releaseHelper);
extern void (*hardwareInfoBaseSetup[IGFX_MAX_PRODUCT])(HardwareInfo *hwInfo, bool setupFeatureTableAndWorkaroundTable, const ReleaseHelper *releaseHelper);

template <GFXCORE_FAMILY gfxFamily>
struct EnableGfxFamilyHw {
    EnableGfxFamilyHw() {
        familyEnabled[gfxFamily] = true;
    }
};

bool getHwInfoForPlatformString(std::string &platform, const HardwareInfo *&hwInfoIn);
void setHwInfoValuesFromConfig(const uint64_t hwInfoConfig, HardwareInfo &hwInfoIn);
bool parseHwInfoConfigString(const std::string &hwInfoConfigStr, uint64_t &hwInfoConfig);
aub_stream::EngineType getChosenEngineType(const HardwareInfo &hwInfo);
void setupDefaultGtSysInfo(HardwareInfo *hwInfo, const ReleaseHelper *releaseHelper);
void setupDefaultFeatureTableAndWorkaroundTable(HardwareInfo *hwInfo, const ReleaseHelper &releaseHelper);
uint32_t getNumSubSlicesPerSlice(const HardwareInfo &hwInfo);

} // namespace NEO
