/*
 * Copyright (C) 2018-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_info.h"

#include "shared/source/command_stream/linear_stream.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/release_helper/release_helper.h"

#include <algorithm>

namespace NEO {
HardwareInfo::HardwareInfo(const PLATFORM *platform, const FeatureTable *featureTable, const WorkaroundTable *workaroundTable,
                           const GT_SYSTEM_INFO *gtSystemInfo, const RuntimeCapabilityTable &capabilityTable)
    : platform(*platform), featureTable(*featureTable), workaroundTable(*workaroundTable), gtSystemInfo(*gtSystemInfo), capabilityTable(capabilityTable) {
}

// Global table of hardware prefixes
const char *hardwarePrefix[IGFX_MAX_PRODUCT] = {
    nullptr,
};

// Global table of family names
bool familyEnabled[IGFX_MAX_CORE] = {
    false,
};

const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT] = {};
void (*hardwareInfoSetup[IGFX_MAX_PRODUCT])(HardwareInfo *, bool, uint64_t, const ReleaseHelper *) = {
    0x0,
};

void (*hardwareInfoBaseSetup[IGFX_MAX_PRODUCT])(HardwareInfo *, bool, const ReleaseHelper *) = {
    0x0,
};

bool getHwInfoForPlatformString(std::string &platform, const HardwareInfo *&hwInfoIn) {
    std::transform(platform.begin(), platform.end(), platform.begin(), ::tolower);

    if (platform == "unk")
        return true;
    bool ret = false;
    for (int j = 0; j < IGFX_MAX_PRODUCT; j++) {
        if (hardwarePrefix[j] == nullptr)
            continue;
        if (hardwarePrefix[j] == platform) {
            hwInfoIn = hardwareInfoTable[j];
            ret = true;
            break;
        }
    }

    return ret;
}

void setHwInfoValuesFromConfig(const uint64_t hwInfoConfig, HardwareInfo &hwInfoIn) {
    uint32_t sliceCount = static_cast<uint16_t>(hwInfoConfig >> 32);
    uint32_t subSlicePerSliceCount = static_cast<uint16_t>(hwInfoConfig >> 16);
    uint32_t euPerSubSliceCount = static_cast<uint16_t>(hwInfoConfig);

    hwInfoIn.gtSystemInfo.SliceCount = sliceCount;
    hwInfoIn.gtSystemInfo.SubSliceCount = subSlicePerSliceCount * sliceCount;
    hwInfoIn.gtSystemInfo.DualSubSliceCount = subSlicePerSliceCount * sliceCount;
    hwInfoIn.gtSystemInfo.EUCount = euPerSubSliceCount * subSlicePerSliceCount * sliceCount;
    hwInfoIn.gtSystemInfo.IsDynamicallyPopulated = true;
    for (uint32_t slice = 0; slice < hwInfoIn.gtSystemInfo.SliceCount; slice++) {
        hwInfoIn.gtSystemInfo.SliceInfo[slice].Enabled = true;
    }

    if (hwInfoIn.gtSystemInfo.MaxSlicesSupported == 0) {
        hwInfoIn.gtSystemInfo.MaxSlicesSupported = sliceCount;
    }
    if (hwInfoIn.gtSystemInfo.MaxSubSlicesSupported == 0) {
        hwInfoIn.gtSystemInfo.MaxSubSlicesSupported = hwInfoIn.gtSystemInfo.SubSliceCount;
    }
    if (hwInfoIn.gtSystemInfo.MaxDualSubSlicesSupported == 0) {
        hwInfoIn.gtSystemInfo.MaxDualSubSlicesSupported = hwInfoIn.gtSystemInfo.SubSliceCount;
    }
    if (hwInfoIn.gtSystemInfo.MaxEuPerSubSlice == 0) {
        hwInfoIn.gtSystemInfo.MaxEuPerSubSlice = euPerSubSliceCount;
    }
}

bool parseHwInfoConfigString(const std::string &hwInfoConfigStr, uint64_t &hwInfoConfig) {
    hwInfoConfig = 0u;

    size_t currPos = hwInfoConfigStr.find('x', 0);
    if (currPos == std::string::npos) {
        return false;
    }
    uint32_t sliceCount = static_cast<uint32_t>(std::stoul(hwInfoConfigStr.substr(0, currPos)));
    if (sliceCount > std::numeric_limits<uint16_t>::max()) {
        return false;
    }
    size_t prevPos = currPos + 1;

    currPos = hwInfoConfigStr.find('x', prevPos);
    if (currPos == std::string::npos) {
        return false;
    }
    uint32_t subSlicePerSliceCount = static_cast<uint32_t>(std::stoul(hwInfoConfigStr.substr(prevPos, currPos)));
    if (subSlicePerSliceCount > std::numeric_limits<uint16_t>::max()) {
        return false;
    }
    uint32_t subSliceCount = subSlicePerSliceCount * sliceCount;
    if (subSliceCount > std::numeric_limits<uint16_t>::max()) {
        return false;
    }
    prevPos = currPos + 1;

    uint32_t euPerSubSliceCount = static_cast<uint32_t>(std::stoul(hwInfoConfigStr.substr(prevPos, std::string::npos)));
    if (euPerSubSliceCount > std::numeric_limits<uint16_t>::max()) {
        return false;
    }
    uint32_t euCount = euPerSubSliceCount * subSliceCount;
    if (euCount > std::numeric_limits<uint16_t>::max()) {
        return false;
    }

    hwInfoConfig = static_cast<uint64_t>(sliceCount & 0xffff) << 32 | static_cast<uint64_t>(subSlicePerSliceCount & 0xffff) << 16 | static_cast<uint64_t>(euPerSubSliceCount & 0xffff);
    return true;
}

aub_stream::EngineType getChosenEngineType(const HardwareInfo &hwInfo) {
    return debugManager.flags.NodeOrdinal.get() == -1
               ? hwInfo.capabilityTable.defaultEngineType
               : static_cast<aub_stream::EngineType>(debugManager.flags.NodeOrdinal.get());
}
void setupDefaultGtSysInfo(HardwareInfo *hwInfo) {
    GT_SYSTEM_INFO *gtSysInfo = &hwInfo->gtSystemInfo;
    gtSysInfo->L3CacheSizeInKb = 1;
    gtSysInfo->CCSInfo.IsValid = 1;
    gtSysInfo->CCSInfo.NumberOfCCSEnabled = 1;
    gtSysInfo->CCSInfo.Instances.CCSEnableMask = 0b1;
    // non-zero values for unit tests
    if (gtSysInfo->SliceCount == 0) {
        gtSysInfo->SliceCount = 2;
        gtSysInfo->SubSliceCount = 8;
        gtSysInfo->DualSubSliceCount = gtSysInfo->SubSliceCount;
        gtSysInfo->EUCount = 64;
        gtSysInfo->NumThreadsPerEu = 8u;
        gtSysInfo->ThreadCount = gtSysInfo->EUCount * gtSysInfo->NumThreadsPerEu;

        gtSysInfo->MaxEuPerSubSlice = gtSysInfo->EUCount / gtSysInfo->SubSliceCount;
        gtSysInfo->MaxSlicesSupported = gtSysInfo->SliceCount;
        gtSysInfo->MaxSubSlicesSupported = gtSysInfo->SubSliceCount;
        gtSysInfo->MaxDualSubSlicesSupported = gtSysInfo->DualSubSliceCount;
    }
}

void setupDefaultFeatureTableAndWorkaroundTable(HardwareInfo *hwInfo, const ReleaseHelper &releaseHelper) {
    FeatureTable *featureTable = &hwInfo->featureTable;

    featureTable->flags.ftrAstcHdr2D = true;
    featureTable->flags.ftrAstcLdr2D = true;
    featureTable->flags.ftrCCSNode = true;
    featureTable->flags.ftrCCSRing = true;
    featureTable->flags.ftrFbc = true;
    featureTable->flags.ftrGpGpuMidBatchPreempt = true;
    featureTable->flags.ftrGpGpuThreadGroupLevelPreempt = true;
    featureTable->flags.ftrIA32eGfxPTEs = true;
    featureTable->flags.ftrL3IACoherency = true;
    featureTable->flags.ftrLinearCCS = true;
    featureTable->flags.ftrPPGTT = true;
    featureTable->flags.ftrSVM = true;
    featureTable->flags.ftrStandardMipTailFormat = true;
    featureTable->flags.ftrTileMappedResource = true;
    featureTable->flags.ftrTranslationTable = true;
    featureTable->flags.ftrUserModeTranslationTable = true;

    featureTable->flags.ftrXe2Compression = releaseHelper.getFtrXe2Compression();

    WorkaroundTable *workaroundTable = &hwInfo->workaroundTable;

    workaroundTable->flags.wa4kAlignUVOffsetNV12LinearSurface = true;
}

void applyDebugOverrides(HardwareInfo &hwInfo) {
    if (debugManager.flags.OverrideNumThreadsPerEu.get() != -1) {
        hwInfo.gtSystemInfo.NumThreadsPerEu = debugManager.flags.OverrideNumThreadsPerEu.get();
        hwInfo.gtSystemInfo.ThreadCount = hwInfo.gtSystemInfo.EUCount * hwInfo.gtSystemInfo.NumThreadsPerEu;
    }
}

uint32_t getNumSubSlicesPerSlice(const HardwareInfo &hwInfo) {
    return static_cast<uint32_t>(Math::divideAndRoundUp(hwInfo.gtSystemInfo.SubSliceCount, hwInfo.gtSystemInfo.SliceCount));
}

} // namespace NEO
