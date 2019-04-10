/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifdef _WIN32

#include "runtime/command_stream/preemption.h"
#include "runtime/device/device.h"
#include "runtime/helpers/device_helpers.h"
#include "runtime/os_interface/debug_settings_manager.h"
#include "runtime/os_interface/device_factory.h"
#include "runtime/os_interface/hw_info_config.h"
#include "runtime/os_interface/windows/os_interface.h"
#include "runtime/os_interface/windows/wddm/wddm.h"

namespace NEO {

extern const HardwareInfo *hardwareInfoTable[IGFX_MAX_PRODUCT];

size_t DeviceFactory::numDevices = 0;
HardwareInfo *DeviceFactory::hwInfo = nullptr;

bool DeviceFactory::getDevices(HardwareInfo **pHWInfos, size_t &numDevices, ExecutionEnvironment &executionEnvironment) {
    numDevices = 0;

    auto hardwareInfo = std::make_unique<HardwareInfo>();
    std::unique_ptr<Wddm> wddm(Wddm::createWddm());
    if (!wddm->enumAdapters(*hardwareInfo)) {
        return false;
    }

    auto totalDeviceCount = DeviceHelper::getDevicesCount(hardwareInfo.get());

    executionEnvironment.osInterface.reset(new OSInterface());
    executionEnvironment.osInterface->get()->setWddm(wddm.release());

    HwInfoConfig *hwConfig = HwInfoConfig::get(hardwareInfo->pPlatform->eProductFamily);
    if (hwConfig->configureHwInfo(hardwareInfo.get(), hardwareInfo.get(), nullptr)) {
        return false;
    }

    *pHWInfos = hardwareInfo.release();
    numDevices = totalDeviceCount;
    DeviceFactory::numDevices = numDevices;
    DeviceFactory::hwInfo = *pHWInfos;

    executionEnvironment.setHwInfo(*pHWInfos);
    executionEnvironment.initGmm();
    auto preemptionMode = PreemptionHelper::getDefaultPreemptionMode(**pHWInfos);
    bool success = executionEnvironment.osInterface->get()->getWddm()->init(preemptionMode);
    DEBUG_BREAK_IF(!success);

    return true;
}

void DeviceFactory::releaseDevices() {
    if (DeviceFactory::numDevices > 0) {
        delete hwInfo->pPlatform;
        delete hwInfo->pSkuTable;
        delete hwInfo->pWaTable;
        delete hwInfo->pSysInfo;
        delete hwInfo;
    }
    DeviceFactory::hwInfo = nullptr;
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
} // namespace NEO

#endif
