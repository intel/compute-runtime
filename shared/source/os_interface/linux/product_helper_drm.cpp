/*
 * Copyright (C) 2018-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/constants.h"
#include "shared/source/helpers/hw_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/product_helper.h"
#include "shared/source/release_helper/release_helper.h"
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

int ProductHelper::configureHwInfoDrm(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, const RootDeviceEnvironment &rootDeviceEnvironment) {
    int ret = 0;
    auto osInterface = rootDeviceEnvironment.osInterface.get();
    Drm *drm = osInterface->getDriverModel()->as<Drm>();

    *outHwInfo = *inHwInfo;
    auto gtSystemInfo = &outHwInfo->gtSystemInfo;
    auto featureTable = &outHwInfo->featureTable;

    DrmQueryTopologyData topologyData = {};

    bool status = drm->queryTopology(*outHwInfo, topologyData);

    if (!status) {
        PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "WARNING: Topology query failed!\n");

        topologyData.sliceCount = gtSystemInfo->SliceCount;

        ret = drm->getEuTotal(topologyData.euCount);
        if (ret != 0) {
            PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query EU total parameter!\n");
            *outHwInfo = {};
            return ret;
        }

        ret = drm->getSubsliceTotal(topologyData.subSliceCount);
        if (ret != 0) {
            PRINT_DEBUG_STRING(debugManager.flags.PrintDebugMessages.get(), stderr, "%s", "FATAL: Cannot query subslice total parameter!\n");
            *outHwInfo = {};
            return ret;
        }

        topologyData.maxEusPerSubSlice = topologyData.subSliceCount > 0 ? topologyData.euCount / topologyData.subSliceCount : 0;
        topologyData.maxSlices = topologyData.sliceCount;
        topologyData.maxSubSlicesPerSlice = topologyData.sliceCount > 0 ? topologyData.subSliceCount / topologyData.sliceCount : 0;
    }

    auto releaseHelper = rootDeviceEnvironment.getReleaseHelper();

    auto numThreadsPerEu = releaseHelper ? releaseHelper->getNumThreadsPerEu() : 7u;

    gtSystemInfo->SliceCount = static_cast<uint32_t>(topologyData.sliceCount);
    gtSystemInfo->SubSliceCount = static_cast<uint32_t>(topologyData.subSliceCount);
    gtSystemInfo->DualSubSliceCount = static_cast<uint32_t>(topologyData.subSliceCount);
    if (topologyData.euCount) {
        gtSystemInfo->EUCount = static_cast<uint32_t>(topologyData.euCount);
    }
    gtSystemInfo->ThreadCount = numThreadsPerEu * gtSystemInfo->EUCount;

    gtSystemInfo->MaxEuPerSubSlice = gtSystemInfo->MaxEuPerSubSlice != 0 ? gtSystemInfo->MaxEuPerSubSlice : topologyData.maxEusPerSubSlice;
    gtSystemInfo->MaxSubSlicesSupported = std::max(static_cast<uint32_t>(topologyData.maxSubSlicesPerSlice * topologyData.maxSlices), gtSystemInfo->MaxSubSlicesSupported);
    gtSystemInfo->MaxSlicesSupported = topologyData.maxSlices;
    gtSystemInfo->MaxDualSubSlicesSupported = gtSystemInfo->MaxSubSlicesSupported;

    gtSystemInfo->IsDynamicallyPopulated = true;
    for (uint32_t slice = 0; slice < GT_MAX_SLICE; slice++) {
        gtSystemInfo->SliceInfo[slice].Enabled = slice < gtSystemInfo->SliceCount;
    }

    uint64_t gttSizeQuery = 0;
    featureTable->flags.ftrSVM = true;

    ret = drm->queryGttSize(gttSizeQuery, true);

    if (ret == 0) {
        featureTable->flags.ftrSVM = (gttSizeQuery > MemoryConstants::max64BitAppAddress);
        outHwInfo->capabilityTable.gpuAddressSpace = gttSizeQuery - 1; // gttSizeQuery = (1 << bits)
    }

    int maxGpuFreq = 0;
    drm->getMaxGpuFrequency(*outHwInfo, maxGpuFreq);

    ret = setupProductSpecificConfig(*outHwInfo, rootDeviceEnvironment);

    configureCacheInfo(outHwInfo);
    featureTable->flags.ftrEDram = (gtSystemInfo->EdramSizeInKb != 0) ? 1 : 0;

    outHwInfo->capabilityTable.maxRenderFrequency = maxGpuFreq;
    outHwInfo->capabilityTable.ftrSvm = featureTable->flags.ftrSVM;
    outHwInfo->capabilityTable.ftrSupportsCoherency = false;

    setupDefaultEngineType(*outHwInfo, rootDeviceEnvironment);

    drm->checkQueueSliceSupport();
    drm->checkNonPersistentContextsSupport();
    drm->checkPreemptionSupport();
    setupPreemptionMode(*outHwInfo, rootDeviceEnvironment, drm->isPreemptionSupported());
    setupPreemptionSurfaceSize(*outHwInfo, rootDeviceEnvironment);
    setupKmdNotifyProperties(outHwInfo->capabilityTable.kmdNotifyProperties);
    setupImageSupport(*outHwInfo);

    return ret;
}

} // namespace NEO
