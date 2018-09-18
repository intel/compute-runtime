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
    while (numDevices < totalDeviceCount) {
        tempHwInfos[numDevices].pPlatform = new PLATFORM(*hwInfoConst->pPlatform);
        tempHwInfos[numDevices].pSkuTable = new FeatureTable(*hwInfoConst->pSkuTable);
        tempHwInfos[numDevices].pWaTable = new WorkaroundTable(*hwInfoConst->pWaTable);
        tempHwInfos[numDevices].pSysInfo = new GT_SYSTEM_INFO(*hwInfoConst->pSysInfo);
        tempHwInfos[numDevices].capabilityTable = hwInfoConst->capabilityTable;
        hardwareInfoSetup[hwInfoConst->pPlatform->eProductFamily](const_cast<GT_SYSTEM_INFO *>(tempHwInfos[numDevices].pSysInfo),
                                                                  const_cast<FeatureTable *>(tempHwInfos[numDevices].pSkuTable), true);
        numDevices++;
    }
    *pHWInfos = tempHwInfos.get();
    DeviceFactory::numDevices = numDevices;
    DeviceFactory::hwInfos = tempHwInfos.get();
    tempHwInfos.release();
    return true;
}

} // namespace OCLRT
