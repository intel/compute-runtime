/*
 * Copyright (C) 2020-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/sysman.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/sleep.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/sysman/source/driver/sysman_driver.h"
#include "level_zero/tools/source/sysman/os_sysman_driver.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

#include <cstring>
#include <vector>

namespace L0 {
bool sysmanInitFromCore = false;

struct OsSysmanDriver *globalOsSysmanDriver = nullptr;

void DeviceImp::createSysmanHandle(bool isSubDevice) {
    if (static_cast<DriverHandleImp *>(driverHandle)->enableSysman && !isSubDevice) {
        if (this->getSysmanHandle() == nullptr) {
            // Sysman handles are created only during zeInit time device creation. And destroyed during L0::device destroy.
            this->setSysmanHandle(L0::SysmanDeviceHandleContext::init(this->toHandle()));
        }
    }
}

SysmanDevice *SysmanDeviceHandleContext::init(ze_device_handle_t coreDevice) {
    if (L0::Sysman::sysmanOnlyInit) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "%s", "Sysman Initialization already happened via zesInit\n");
        return nullptr;
    }

    SysmanDeviceImp *sysmanDevice = new SysmanDeviceImp(coreDevice);
    DEBUG_BREAK_IF(!sysmanDevice);
    if (ZE_RESULT_SUCCESS != sysmanDevice->init()) {
        delete sysmanDevice;
        sysmanDevice = nullptr;
        sysmanInitFromCore = false;
    } else {
        sysmanInitFromCore = true;
    }

    L0::DeviceImp *device = static_cast<DeviceImp *>(Device::fromHandle(coreDevice));
    for (auto &subDevice : device->subDevices) {
        static_cast<DeviceImp *>(subDevice)->setSysmanHandle(sysmanDevice);
    }

    if (globalOsSysmanDriver == nullptr) {
        globalOsSysmanDriver = L0::OsSysmanDriver::create();
    }
    return sysmanDevice;
}

void DeviceImp::setSysmanHandle(SysmanDevice *pSysmanDev) {
    pSysmanDevice = pSysmanDev;
}

SysmanDevice *DeviceImp::getSysmanHandle() {
    return pSysmanDevice;
}

ze_result_t DriverHandleImp::sysmanEventsListen(
    uint32_t timeout,
    uint32_t count,
    zes_device_handle_t *phDevices,
    uint32_t *pNumDeviceEvents,
    zes_event_type_flags_t *pEvents) {

    if (globalOsSysmanDriver == nullptr) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "%s", "Os Sysman Driver Not initialized\n");
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    return globalOsSysmanDriver->eventsListen(timeout, count, phDevices, pNumDeviceEvents, pEvents);
}

ze_result_t DriverHandleImp::sysmanEventsListenEx(
    uint64_t timeout,
    uint32_t count,
    zes_device_handle_t *phDevices,
    uint32_t *pNumDeviceEvents,
    zes_event_type_flags_t *pEvents) {

    if (globalOsSysmanDriver == nullptr) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "%s", "Os Sysman Driver Not initialized\n");
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return globalOsSysmanDriver->eventsListen(timeout, count, phDevices, pNumDeviceEvents, pEvents);
}

ze_result_t SysmanDevice::performanceGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_perf_handle_t *phPerformance) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->performanceGet(pCount, phPerformance);
}

ze_result_t SysmanDevice::powerGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_pwr_handle_t *phPower) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->powerGet(pCount, phPower);
}

ze_result_t SysmanDevice::powerGetCardDomain(zes_device_handle_t hDevice, zes_pwr_handle_t *phPower) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->powerGetCardDomain(phPower);
}

ze_result_t SysmanDevice::frequencyGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_freq_handle_t *phFrequency) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->frequencyGet(pCount, phFrequency);
}

ze_result_t SysmanDevice::fabricPortGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_fabric_port_handle_t *phPort) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->fabricPortGet(pCount, phPort);
}

ze_result_t SysmanDevice::temperatureGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_temp_handle_t *phTemperature) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->temperatureGet(pCount, phTemperature);
}

ze_result_t SysmanDevice::standbyGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_standby_handle_t *phStandby) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->standbyGet(pCount, phStandby);
}

ze_result_t SysmanDevice::deviceGetProperties(zes_device_handle_t hDevice, zes_device_properties_t *pProperties) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->deviceGetProperties(pProperties);
}

ze_result_t SysmanDevice::processesGetState(zes_device_handle_t hDevice, uint32_t *pCount, zes_process_state_t *pProcesses) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->processesGetState(pCount, pProcesses);
}

ze_result_t SysmanDevice::deviceReset(zes_device_handle_t hDevice, ze_bool_t force) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->deviceReset(force);
}
ze_result_t SysmanDevice::deviceGetState(zes_device_handle_t hDevice, zes_device_state_t *pState) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->deviceGetState(pState);
}

ze_result_t SysmanDevice::engineGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_engine_handle_t *phEngine) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->engineGet(pCount, phEngine);
}

ze_result_t SysmanDevice::pciGetProperties(zes_device_handle_t hDevice, zes_pci_properties_t *pProperties) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->pciGetProperties(pProperties);
}

ze_result_t SysmanDevice::pciGetState(zes_device_handle_t hDevice, zes_pci_state_t *pState) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->pciGetState(pState);
}

ze_result_t SysmanDevice::pciGetBars(zes_device_handle_t hDevice, uint32_t *pCount, zes_pci_bar_properties_t *pProperties) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->pciGetBars(pCount, pProperties);
}

ze_result_t SysmanDevice::pciGetStats(zes_device_handle_t hDevice, zes_pci_stats_t *pStats) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->pciGetStats(pStats);
}

ze_result_t SysmanDevice::schedulerGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_sched_handle_t *phScheduler) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->schedulerGet(pCount, phScheduler);
}

ze_result_t SysmanDevice::rasGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_ras_handle_t *phRas) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->rasGet(pCount, phRas);
}

ze_result_t SysmanDevice::memoryGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_mem_handle_t *phMemory) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->memoryGet(pCount, phMemory);
}

ze_result_t SysmanDevice::fanGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_fan_handle_t *phFan) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->fanGet(pCount, phFan);
}

ze_result_t SysmanDevice::diagnosticsGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_diag_handle_t *phDiagnostics) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->diagnosticsGet(pCount, phDiagnostics);
}

ze_result_t SysmanDevice::firmwareGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_firmware_handle_t *phFirmware) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->firmwareGet(pCount, phFirmware);
}

ze_result_t SysmanDevice::deviceEventRegister(zes_device_handle_t hDevice, zes_event_type_flags_t events) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->deviceEventRegister(events);
}

ze_result_t SysmanDevice::deviceEccAvailable(zes_device_handle_t hDevice, ze_bool_t *pAvailable) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->deviceEccAvailable(pAvailable);
}

ze_result_t SysmanDevice::deviceEccConfigurable(zes_device_handle_t hDevice, ze_bool_t *pConfigurable) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->deviceEccConfigurable(pConfigurable);
}

ze_result_t SysmanDevice::deviceGetEccState(zes_device_handle_t hDevice, zes_device_ecc_properties_t *pState) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->deviceGetEccState(pState);
}

ze_result_t SysmanDevice::deviceSetEccState(zes_device_handle_t hDevice, const zes_device_ecc_desc_t *newState, zes_device_ecc_properties_t *pState) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    if (pSysmanDevice == nullptr) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return pSysmanDevice->deviceSetEccState(newState, pState);
}

uint64_t SysmanDevice::getSysmanTimestamp() {
    std::chrono::time_point<std::chrono::steady_clock> ts = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(ts.time_since_epoch()).count();
}

ze_result_t SysmanDevice::deviceResetExt(zes_device_handle_t hDevice, zes_reset_properties_t *pProperties) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->deviceResetExt(pProperties);
}

ze_result_t SysmanDevice::fabricPortGetMultiPortThroughput(zes_device_handle_t hDevice, uint32_t numPorts, zes_fabric_port_handle_t *phPort, zes_fabric_port_throughput_t **pThroughput) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->fabricPortGetMultiPortThroughput(numPorts, phPort, pThroughput);
}

ze_result_t SysmanDevice::deviceEnumEnabledVF(zes_device_handle_t hDevice, uint32_t *pCount, zes_vf_handle_t *phVFhandle) {
    auto pSysmanDevice = L0::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->deviceEnumEnabledVF(pCount, phVFhandle);
}

} // namespace L0
