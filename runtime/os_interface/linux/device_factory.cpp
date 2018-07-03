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

#include "runtime/helpers/hw_info.h"
#include "runtime/helpers/options.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/os_interface/linux/drm_neo.h"
#include "runtime/os_interface/hw_info_config.h"
#include "runtime/os_interface/linux/os_interface.h"
#include "drm/i915_drm.h"
#include "runtime/gmm_helper/gmm_helper.h"
#include "runtime/device/device.h"

#include <vector>
#include <cstring>

namespace OCLRT {

size_t DeviceFactory::numDevices = 0;
HardwareInfo *DeviceFactory::hwInfos = nullptr;

bool DeviceFactory::getDevices(HardwareInfo **pHWInfos, size_t &numDevices) {
    std::vector<HardwareInfo> tHwInfos;
    unsigned int devNum = 0;
    size_t requiredDeviceCount = 1;

    if (DebugManager.flags.CreateMultipleDevices.get()) {
        requiredDeviceCount = DebugManager.flags.CreateMultipleDevices.get();
    }

    Drm *drm = Drm::create(devNum);
    if (!drm) {
        return false;
    }
    std::unique_ptr<OSInterface> osInterface = std::unique_ptr<OSInterface>(new OSInterface());
    osInterface.get()->get()->setDrm(drm);
    const HardwareInfo *pCurrDevice = platformDevices[devNum];

    while (devNum < requiredDeviceCount) {
        HardwareInfo tmpHwInfo;
        HwInfoConfig *hwConfig = HwInfoConfig::get(pCurrDevice->pPlatform->eProductFamily);
        if (hwConfig->configureHwInfo(pCurrDevice, &tmpHwInfo, osInterface.get())) {
            return false;
        }
        tHwInfos.push_back(tmpHwInfo);
        devNum++;
    }

    HardwareInfo *ptr = new HardwareInfo[devNum];
    for (size_t i = 0; i < tHwInfos.size(); i++)
        ptr[i] = tHwInfos[i];

    numDevices = devNum;
    *pHWInfos = ptr;

    DeviceFactory::numDevices = devNum;
    DeviceFactory::hwInfos = ptr;

    return true;
}

void DeviceFactory::releaseDevices() {
    if (DeviceFactory::numDevices > 0) {
        for (unsigned int i = 0; i < DeviceFactory::numDevices; ++i) {
            Drm::closeDevice(i);
            delete hwInfos[i].pSysInfo;
            delete hwInfos[i].pSkuTable;
            delete hwInfos[i].pWaTable;
            delete hwInfos[i].pPlatform;
        }
        delete[] hwInfos;
    }
    DeviceFactory::hwInfos = nullptr;
    DeviceFactory::numDevices = 0;
}

void Device::appendOSExtensions(std::string &deviceExtensions) {
}
} // namespace OCLRT
