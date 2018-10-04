/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/device.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/debug_settings_manager.h"

namespace OCLRT {

bool DeviceFactory::getDevicesForProductFamilyOverride(HardwareInfo **pHWInfos, size_t &numDevices, ExecutionEnvironment &executionEnvironment) {
    auto totalDeviceCount = 1u;
    if (DebugManager.flags.CreateMultipleDevices.get()) {
        totalDeviceCount = DebugManager.flags.CreateMultipleDevices.get();
    }
    auto productFamily = DebugManager.flags.ProductFamilyOverride.get();
    auto hwInfoConst = *platformDevices;
    getHwInfoForPlatformString(productFamily.c_str(), hwInfoConst);
    std::unique_ptr<HardwareInfo[]> tempHwInfos(new HardwareInfo[totalDeviceCount]);
    numDevices = 0;
    std::string hwInfoConfig;
    DebugManager.getHardwareInfoOverride(hwInfoConfig);
    while (numDevices < totalDeviceCount) {
        tempHwInfos[numDevices].pPlatform = new PLATFORM(*hwInfoConst->pPlatform);
        tempHwInfos[numDevices].pSkuTable = new FeatureTable(*hwInfoConst->pSkuTable);
        tempHwInfos[numDevices].pWaTable = new WorkaroundTable(*hwInfoConst->pWaTable);
        tempHwInfos[numDevices].pSysInfo = new GT_SYSTEM_INFO(*hwInfoConst->pSysInfo);
        tempHwInfos[numDevices].capabilityTable = hwInfoConst->capabilityTable;
        hardwareInfoSetup[hwInfoConst->pPlatform->eProductFamily](const_cast<GT_SYSTEM_INFO *>(tempHwInfos[numDevices].pSysInfo),
                                                                  const_cast<FeatureTable *>(tempHwInfos[numDevices].pSkuTable),
                                                                  true, hwInfoConfig);
        numDevices++;
    }
    *pHWInfos = tempHwInfos.get();
    DeviceFactory::numDevices = numDevices;
    DeviceFactory::hwInfos = tempHwInfos.get();
    tempHwInfos.release();
    return true;
}

} // namespace OCLRT
