/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/device.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/hw_info_config.h"

namespace NEO {

bool DeviceFactory::getDevicesForProductFamilyOverride(HardwareInfo **pHWInfos, size_t &numDevices, ExecutionEnvironment &executionEnvironment) {
    auto totalDeviceCount = 1u;
    if (DebugManager.flags.CreateMultipleDevices.get()) {
        totalDeviceCount = DebugManager.flags.CreateMultipleDevices.get();
    }
    auto productFamily = DebugManager.flags.ProductFamilyOverride.get();
    auto hwInfoConst = *platformDevices;
    getHwInfoForPlatformString(productFamily.c_str(), hwInfoConst);
    numDevices = 0;
    std::string hwInfoConfig;
    DebugManager.getHardwareInfoOverride(hwInfoConfig);

    auto hardwareInfo = std::make_unique<HardwareInfo>();
    hardwareInfo->pPlatform = new PLATFORM(*hwInfoConst->pPlatform);
    hardwareInfo->pSkuTable = new FeatureTable(*hwInfoConst->pSkuTable);
    hardwareInfo->pWaTable = new WorkaroundTable(*hwInfoConst->pWaTable);
    hardwareInfo->pSysInfo = new GT_SYSTEM_INFO(*hwInfoConst->pSysInfo);
    hardwareInfo->capabilityTable = hwInfoConst->capabilityTable;
    hardwareInfoSetup[hwInfoConst->pPlatform->eProductFamily](hardwareInfo.get(), true, hwInfoConfig);

    HwInfoConfig *hwConfig = HwInfoConfig::get(hardwareInfo->pPlatform->eProductFamily);
    hwConfig->configureHardwareCustom(hardwareInfo.get(), nullptr);

    *pHWInfos = hardwareInfo.release();

    executionEnvironment.setHwInfo(*pHWInfos);
    numDevices = totalDeviceCount;
    DeviceFactory::numDevices = numDevices;
    DeviceFactory::hwInfo = *pHWInfos;

    return true;
}

} // namespace NEO
