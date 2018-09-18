/*
 * Copyright (C) 2017-2018 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef _WIN32

#include "runtime/device/device.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/os_interface/hw_info_config.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/windows/wddm/wddm.h"

namespace OCLRT {

extern const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT];

size_t DeviceFactory::numDevices = 0;
HardwareInfo *DeviceFactory::hwInfos = nullptr;

bool DeviceFactory::getDevices(HardwareInfo **pHWInfos, size_t &numDevices, ExecutionEnvironment &executionEnvironment) {
    auto totalDeviceCount = 1u;
    if (DebugManager.flags.CreateMultipleDevices.get()) {
        totalDeviceCount = DebugManager.flags.CreateMultipleDevices.get();
    }
    std::unique_ptr<HardwareInfo[]> tempHwInfos(new HardwareInfo[totalDeviceCount]);
    numDevices = 0;

    while (numDevices < totalDeviceCount) {
        std::unique_ptr<Wddm> wddm(Wddm ::createWddm());
        if (!wddm->enumAdapters(tempHwInfos[numDevices])) {
            return false;
        }

        executionEnvironment.osInterface.reset(new OSInterface());
        executionEnvironment.osInterface->get()->setWddm(wddm.release());

        HwInfoConfig *hwConfig = HwInfoConfig::get(tempHwInfos[numDevices].pPlatform->eProductFamily);
        if (hwConfig->configureHwInfo(&tempHwInfos[numDevices], &tempHwInfos[numDevices], nullptr)) {
            return false;
        }
        numDevices++;
    }

    *pHWInfos = tempHwInfos.get();
    DeviceFactory::numDevices = numDevices;
    DeviceFactory::hwInfos = tempHwInfos.get();
    tempHwInfos.release();
    return true;
}

void DeviceFactory::releaseDevices() {
    if (DeviceFactory::numDevices > 0) {
        for (unsigned int i = 0; i < DeviceFactory::numDevices; ++i) {
            delete hwInfos[i].pPlatform;
            delete hwInfos[i].pSkuTable;
            delete hwInfos[i].pWaTable;
            delete hwInfos[i].pSysInfo;
        }
        delete[] hwInfos;
    }
    DeviceFactory::hwInfos = nullptr;
    DeviceFactory::numDevices = 0;
}

void Device::appendOSExtensions(std::string &deviceExtensions) {
    deviceExtensions += "cl_intel_simultaneous_sharing ";

    simultaneousInterops = {CL_GL_CONTEXT_KHR,
                            CL_WGL_HDC_KHR,
                            CL_CONTEXT_ADAPTER_D3D9_KHR,
                            CL_CONTEXT_D3D9_DEVICE_INTEL,
                            CL_CONTEXT_ADAPTER_D3D9EX_KHR,
                            CL_CONTEXT_D3D9EX_DEVICE_INTEL,
                            CL_CONTEXT_ADAPTER_DXVA_KHR,
                            CL_CONTEXT_DXVA_DEVICE_INTEL,
                            CL_CONTEXT_D3D10_DEVICE_KHR,
                            CL_CONTEXT_D3D11_DEVICE_KHR,
                            0};
}
} // namespace OCLRT

#endif
