/*
 * Copyright (C) 2018-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/command_stream/preemption.h"
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/compiler_hw_info_config.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/hw_info_config.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/cpu_info.h"

#include <cstring>

namespace NEO {

uint32_t bitExact(uint32_t value, uint32_t highBit, uint32_t lowBit) {
    uint32_t bitVal = static_cast<uint32_t>((value >> lowBit) & maxNBitValue(highBit - lowBit + 1));
    return bitVal;
}

int configureCacheInfo(HardwareInfo *hwInfo) {
    GT_SYSTEM_INFO *gtSystemInfo = &hwInfo->gtSystemInfo;

    uint32_t type = 0;
    uint32_t subleaf = 0;
    uint32_t eax, ebx, ecx;
    uint32_t cachelevel, linesize, partitions, ways;
    uint64_t sets, size;

    const CpuInfo &cpuInfo = CpuInfo::getInstance();

    do {
        uint32_t cpuRegsInfo[4] = {};

        cpuInfo.cpuidex(cpuRegsInfo, 4, subleaf);
        eax = cpuRegsInfo[0];
        ebx = cpuRegsInfo[1];
        ecx = cpuRegsInfo[2];

        type = bitExact(eax, 4, 0);
        if (type != 0) {
            cachelevel = bitExact(eax, 7, 5);
            linesize = bitExact(ebx, 11, 0) + 1;
            partitions = bitExact(ebx, 21, 12) + 1;
            ways = bitExact(ebx, 31, 22) + 1;
            sets = (uint64_t)ecx + 1;

            size = sets * ways * partitions * linesize / 1024;
            if (cachelevel == 3) {
                gtSystemInfo->LLCCacheSizeInKb = size;
            }
            subleaf++;
        }
    } while (type);

    return 0;
}

int HwInfoConfig::configureHwInfoDrm(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, OSInterface *osIface) {
    int ret = 0;
    Drm *drm = osIface->getDriverModel()->as<Drm>();

    *outHwInfo = *inHwInfo;
    auto platform = &outHwInfo->platform;
    auto gtSystemInfo = &outHwInfo->gtSystemInfo;
    auto featureTable = &outHwInfo->featureTable;

    Drm::QueryTopologyData topologyData = {};

    bool status = drm->queryTopology(*outHwInfo, topologyData);

    if (!status) {
        PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Topology query failed!\n");

        topologyData.sliceCount = gtSystemInfo->SliceCount;

        ret = drm->getEuTotal(topologyData.euCount);
        if (ret != 0) {
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query EU total parameter!\n");
            *outHwInfo = {};
            return ret;
        }

        ret = drm->getSubsliceTotal(topologyData.subSliceCount);
        if (ret != 0) {
            PRINT_DEBUG_STRING(DebugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query subslice total parameter!\n");
            *outHwInfo = {};
            return ret;
        }

        topologyData.maxEuCount = topologyData.subSliceCount > 0 ? topologyData.euCount / topologyData.subSliceCount : 0;
        topologyData.maxSliceCount = topologyData.sliceCount;
        topologyData.maxSubSliceCount = topologyData.sliceCount > 0 ? topologyData.subSliceCount / topologyData.sliceCount : 0;
    }

    gtSystemInfo->SliceCount = static_cast<uint32_t>(topologyData.sliceCount);
    gtSystemInfo->SubSliceCount = static_cast<uint32_t>(topologyData.subSliceCount);
    gtSystemInfo->DualSubSliceCount = static_cast<uint32_t>(topologyData.subSliceCount);
    gtSystemInfo->EUCount = static_cast<uint32_t>(topologyData.euCount);
    gtSystemInfo->ThreadCount = this->threadsPerEu * gtSystemInfo->EUCount;

    gtSystemInfo->MaxEuPerSubSlice = gtSystemInfo->MaxEuPerSubSlice != 0 ? gtSystemInfo->MaxEuPerSubSlice : topologyData.maxEuCount;
    gtSystemInfo->MaxSubSlicesSupported = std::max(static_cast<uint32_t>(topologyData.maxSubSliceCount * topologyData.maxSliceCount), gtSystemInfo->MaxSubSlicesSupported);
    gtSystemInfo->MaxSlicesSupported = topologyData.maxSliceCount;

    for (uint32_t slice = 0; slice < gtSystemInfo->SliceCount; slice++) {
        gtSystemInfo->SliceInfo[slice].Enabled = true;
    }

    uint64_t gttSizeQuery = 0;
    featureTable->flags.ftrSVM = true;

    ret = drm->queryGttSize(gttSizeQuery);

    if (ret == 0) {
        featureTable->flags.ftrSVM = (gttSizeQuery > MemoryConstants::max64BitAppAddress);
        outHwInfo->capabilityTable.gpuAddressSpace = gttSizeQuery - 1; // gttSizeQuery = (1 << bits)
    }

    int maxGpuFreq = 0;
    drm->getMaxGpuFrequency(*outHwInfo, maxGpuFreq);

    ret = configureHardwareCustom(outHwInfo, osIface);
    if (ret != 0) {
        *outHwInfo = {};
        return ret;
    }
    configureCacheInfo(outHwInfo);
    featureTable->flags.ftrEDram = (gtSystemInfo->EdramSizeInKb != 0) ? 1 : 0;

    outHwInfo->capabilityTable.maxRenderFrequency = maxGpuFreq;
    outHwInfo->capabilityTable.ftrSvm = featureTable->flags.ftrSVM;

    HwHelper &hwHelper = HwHelper::get(platform->eRenderCoreFamily);
    outHwInfo->capabilityTable.ftrSupportsCoherency = false;

    hwHelper.adjustDefaultEngineType(outHwInfo);
    outHwInfo->capabilityTable.defaultEngineType = getChosenEngineType(*outHwInfo);

    drm->checkQueueSliceSupport();
    drm->checkNonPersistentContextsSupport();
    drm->checkPreemptionSupport();
    bool preemption = drm->isPreemptionSupported();
    auto compilerHwInfoConfig = CompilerHwInfoConfig::get(outHwInfo->platform.eProductFamily);
    PreemptionHelper::adjustDefaultPreemptionMode(outHwInfo->capabilityTable,
                                                  compilerHwInfoConfig->isMidThreadPreemptionSupported(*outHwInfo) && preemption,
                                                  static_cast<bool>(outHwInfo->featureTable.flags.ftrGpGpuThreadGroupLevelPreempt) && preemption,
                                                  static_cast<bool>(outHwInfo->featureTable.flags.ftrGpGpuMidBatchPreempt) && preemption);

    outHwInfo->capabilityTable.requiredPreemptionSurfaceSize = outHwInfo->gtSystemInfo.CsrSizeInMb * MemoryConstants::megaByte;
    hwHelper.adjustPreemptionSurfaceSize(outHwInfo->capabilityTable.requiredPreemptionSurfaceSize);

    auto &kmdNotifyProperties = outHwInfo->capabilityTable.kmdNotifyProperties;
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableKmdNotify.get(), kmdNotifyProperties.enableKmdNotify);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideKmdNotifyDelayMicroseconds.get(), kmdNotifyProperties.delayKmdNotifyMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableQuickKmdSleep.get(), kmdNotifyProperties.enableQuickKmdSleep);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideQuickKmdSleepDelayMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableQuickKmdSleepForSporadicWaits.get(), kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideDelayQuickKmdSleepForSporadicWaitsMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableQuickKmdSleepForDirectSubmission.get(), kmdNotifyProperties.enableQuickKmdSleepForDirectSubmission);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideDelayQuickKmdSleepForDirectSubmissionMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepForDirectSubmissionMicroseconds);

    return 0;
}

} // namespace NEO
