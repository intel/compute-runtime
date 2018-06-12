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

#include "runtime/command_stream/preemption.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/hw_helper.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "runtime/os_interface/hw_info_config.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/utilities/cpu_info.h"
#include "runtime/memory_manager/memory_constants.h"
#include "runtime/gen_common/hw_cmds.h"

#include <cstring>

namespace OCLRT {

HwInfoConfig *hwInfoConfigFactory[IGFX_MAX_PRODUCT] = {};

uint32_t bitExact(uint32_t value, uint32_t highBit, uint32_t lowBit) {
    uint32_t bitVal = ((value >> lowBit) & ((1 << (highBit - lowBit + 1)) - 1));
    return bitVal;
}

int configureCacheInfo(HardwareInfo *hwInfo) {
    GT_SYSTEM_INFO *pSysInfo = const_cast<GT_SYSTEM_INFO *>(hwInfo->pSysInfo);

    uint32_t type = 0;
    uint32_t subleaf = 0;
    uint32_t eax, ebx, ecx;
    uint32_t cachelevel, linesize, partitions, ways;
    uint64_t sets, size;

    const CpuInfo &cpuInfo = CpuInfo::getInstance();

    do {
        uint32_t cpuRegsInfo[4];

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
                pSysInfo->LLCCacheSizeInKb = size;
            }
            subleaf++;
        }
    } while (type);

    return 0;
}

int HwInfoConfig::configureHwInfo(const HardwareInfo *inHwInfo, HardwareInfo *outHwInfo, OSInterface *osIface) {
    int ret = 0;
    Drm *drm = osIface->get()->getDrm();

    auto pPlatform = std::unique_ptr<PLATFORM>(new PLATFORM);
    *pPlatform = *(inHwInfo->pPlatform);
    auto pSysInfo = std::unique_ptr<GT_SYSTEM_INFO>(new GT_SYSTEM_INFO);
    *(pSysInfo) = *(inHwInfo->pSysInfo);
    auto pSkuTable = std::unique_ptr<FeatureTable>(new FeatureTable);
    *pSkuTable = *(inHwInfo->pSkuTable);
    auto pWaTable = std::unique_ptr<WorkaroundTable>(new WorkaroundTable);
    *pWaTable = *(inHwInfo->pWaTable);

    outHwInfo->pPlatform = const_cast<const PLATFORM *>(pPlatform.get());
    outHwInfo->pSysInfo = const_cast<const GT_SYSTEM_INFO *>(pSysInfo.get());
    outHwInfo->pSkuTable = const_cast<const FeatureTable *>(pSkuTable.get());
    outHwInfo->pWaTable = const_cast<const WorkaroundTable *>(pWaTable.get());
    outHwInfo->capabilityTable = inHwInfo->capabilityTable;

    int val = 0;
    ret = drm->getDeviceID(val);
    if (ret != 0 || val == 0) {
        *outHwInfo = {};
        return (ret == 0) ? -1 : ret;
    }
    pPlatform->usDeviceID = static_cast<unsigned short>(val);
    ret = drm->getDeviceRevID(val);
    if (ret != 0) {
        *outHwInfo = {};
        return ret;
    }
    pPlatform->usRevId = static_cast<unsigned short>(val);

    int euCount;
    ret = drm->getEuTotal(euCount);
    if (ret != 0) {
        *outHwInfo = {};
        return ret;
    }
    pSysInfo->EUCount = static_cast<uint32_t>(euCount);

    pSysInfo->ThreadCount = this->threadsPerEu * pSysInfo->EUCount;

    int subSliceCount;
    ret = drm->getSubsliceTotal(subSliceCount);
    if (ret != 0) {
        *outHwInfo = {};
        return ret;
    }
    pSysInfo->SubSliceCount = static_cast<uint32_t>(subSliceCount);

    pSkuTable->ftrSVM = drm->is48BitAddressRangeSupported();

    int maxGpuFreq = 0;
    drm->getMaxGpuFrequency(maxGpuFreq);

    GTTYPE gtType = drm->getGtType();
    if (gtType == GTTYPE_UNDEFINED) {
        *outHwInfo = {};
        return -1;
    }
    pPlatform->eGTType = gtType;
    pSkuTable->ftrGTA = (gtType == GTTYPE_GTA) ? 1 : 0;
    pSkuTable->ftrGTC = (gtType == GTTYPE_GTC) ? 1 : 0;
    pSkuTable->ftrGTX = (gtType == GTTYPE_GTX) ? 1 : 0;
    pSkuTable->ftrGT1 = (gtType == GTTYPE_GT1) ? 1 : 0;
    pSkuTable->ftrGT1_5 = (gtType == GTTYPE_GT1_5) ? 1 : 0;
    pSkuTable->ftrGT2 = (gtType == GTTYPE_GT2) ? 1 : 0;
    pSkuTable->ftrGT2_5 = (gtType == GTTYPE_GT2_5) ? 1 : 0;
    pSkuTable->ftrGT3 = (gtType == GTTYPE_GT3) ? 1 : 0;
    pSkuTable->ftrGT4 = (gtType == GTTYPE_GT4) ? 1 : 0;

    ret = configureHardwareCustom(outHwInfo, osIface);
    if (ret != 0) {
        *outHwInfo = {};
        return ret;
    }
    configureCacheInfo(outHwInfo);
    pSkuTable->ftrEDram = (pSysInfo->EdramSizeInKb != 0) ? 1 : 0;

    outHwInfo->capabilityTable.maxRenderFrequency = maxGpuFreq;
    outHwInfo->capabilityTable.ftrSvm = pSkuTable->ftrSVM;

    HwHelper &hwHelper = HwHelper::get(pPlatform->eRenderCoreFamily);
    outHwInfo->capabilityTable.ftrSupportsCoherency = false;

    hwHelper.adjustDefaultEngineType(outHwInfo);
    outHwInfo->capabilityTable.defaultEngineType = DebugManager.flags.NodeOrdinal.get() == -1
                                                       ? outHwInfo->capabilityTable.defaultEngineType
                                                       : static_cast<EngineType>(DebugManager.flags.NodeOrdinal.get());

    outHwInfo->capabilityTable.instrumentationEnabled = false;
    outHwInfo->capabilityTable.ftrCompression = false;

    bool preemption = drm->hasPreemption();
    preemption = hwHelper.setupPreemptionRegisters(outHwInfo, preemption);
    PreemptionHelper::adjustDefaultPreemptionMode(outHwInfo->capabilityTable,
                                                  static_cast<bool>(outHwInfo->pSkuTable->ftrGpGpuMidThreadLevelPreempt) && preemption,
                                                  static_cast<bool>(outHwInfo->pSkuTable->ftrGpGpuThreadGroupLevelPreempt) && preemption,
                                                  static_cast<bool>(outHwInfo->pSkuTable->ftrGpGpuMidBatchPreempt) && preemption);
    outHwInfo->capabilityTable.requiredPreemptionSurfaceSize = outHwInfo->pSysInfo->CsrSizeInMb * MemoryConstants::megaByte;

    auto &kmdNotifyProperties = outHwInfo->capabilityTable.kmdNotifyProperties;
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableKmdNotify.get(), kmdNotifyProperties.enableKmdNotify);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideKmdNotifyDelayMicroseconds.get(), kmdNotifyProperties.delayKmdNotifyMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableQuickKmdSleep.get(), kmdNotifyProperties.enableQuickKmdSleep);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideQuickKmdSleepDelayMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepMicroseconds);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideEnableQuickKmdSleepForSporadicWaits.get(), kmdNotifyProperties.enableQuickKmdSleepForSporadicWaits);
    KmdNotifyHelper::overrideFromDebugVariable(DebugManager.flags.OverrideDelayQuickKmdSleepForSporadicWaitsMicroseconds.get(), kmdNotifyProperties.delayQuickKmdSleepForSporadicWaitsMicroseconds);

    pPlatform.release();
    pSkuTable.release();
    pWaTable.release();
    pSysInfo.release();

    return 0;
}

} // namespace OCLRT
