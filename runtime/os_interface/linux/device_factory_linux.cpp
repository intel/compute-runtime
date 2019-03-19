/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/device/device.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/os_interface/hw_info_config.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "runtime/os_interface/linux/os_interface.h"

#include "drm/i915_drm.h"

#include <cstring>
#include <vector>

namespace OCLRT {
size_t DeviceFactory::numDevices = 0;
HardwareInfo *DeviceFactory::hwInfo = nullptr;

bool DeviceFactory::getDevices(HardwareInfo **pHWInfos, size_t &numDevices, ExecutionEnvironment &executionEnvironment) {
    unsigned int devNum = 0;
    size_t requiredDeviceCount = 1;

    if (DebugManager.flags.CreateMultipleDevices.get()) {
        requiredDeviceCount = DebugManager.flags.CreateMultipleDevices.get();
    }

    Drm *drm = Drm::create(devNum);
    if (!drm) {
        return false;
    }

    executionEnvironment.osInterface.reset(new OSInterface());
    executionEnvironment.osInterface->get()->setDrm(drm);

    auto hardwareInfo = std::make_unique<HardwareInfo>();
    const HardwareInfo *pCurrDevice = platformDevices[devNum];
    HwInfoConfig *hwConfig = HwInfoConfig::get(pCurrDevice->pPlatform->eProductFamily);
    if (hwConfig->configureHwInfo(pCurrDevice, hardwareInfo.get(), executionEnvironment.osInterface.get())) {
        return false;
    }

    numDevices = requiredDeviceCount;
    *pHWInfos = hardwareInfo.release();
    executionEnvironment.setHwInfo(*pHWInfos);

    DeviceFactory::numDevices = numDevices;
    DeviceFactory::hwInfo = *pHWInfos;

    return true;
}

void DeviceFactory::releaseDevices() {
    if (DeviceFactory::numDevices > 0) {
        for (unsigned int i = 0; i < DeviceFactory::numDevices; ++i) {
            Drm::closeDevice(i);
        }
        delete hwInfo->pSysInfo;
        delete hwInfo->pSkuTable;
        delete hwInfo->pWaTable;
        delete hwInfo->pPlatform;
        delete hwInfo;
    }
    DeviceFactory::hwInfo = nullptr;
    DeviceFactory::numDevices = 0;
}

void Device::appendOSExtensions(std::string &deviceExtensions) {
}
} // namespace OCLRT
