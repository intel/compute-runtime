/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/execution_environment/execution_environment.h"

#include "level_zero/sysman/source/sysman_device_imp.h"

#include <cstring>
#include <vector>

namespace L0 {
namespace Sysman {

SysmanDevice *SysmanDevice::create(NEO::ExecutionEnvironment &executionEnvironment, const uint32_t rootDeviceIndex) {

    SysmanDeviceImp *pSysmanDevice = new SysmanDeviceImp(&executionEnvironment, rootDeviceIndex);
    DEBUG_BREAK_IF(!pSysmanDevice);
    if (pSysmanDevice->init() != ZE_RESULT_SUCCESS) {
        delete pSysmanDevice;
        pSysmanDevice = nullptr;
    }
    return pSysmanDevice;
}

ze_result_t SysmanDevice::fabricPortGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_fabric_port_handle_t *phPort) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->fabricPortGet(pCount, phPort);
}
ze_result_t SysmanDevice::memoryGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_mem_handle_t *phMemory) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->memoryGet(pCount, phMemory);
}

ze_result_t SysmanDevice::powerGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_pwr_handle_t *phPower) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->powerGet(pCount, phPower);
}

ze_result_t SysmanDevice::powerGetCardDomain(zes_device_handle_t hDevice, zes_pwr_handle_t *phPower) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->powerGetCardDomain(phPower);
}

ze_result_t SysmanDevice::engineGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_engine_handle_t *phEngine) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->engineGet(pCount, phEngine);
}

ze_result_t SysmanDevice::frequencyGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_freq_handle_t *phFrequency) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->frequencyGet(pCount, phFrequency);
}

ze_result_t SysmanDevice::schedulerGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_sched_handle_t *phScheduler) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->schedulerGet(pCount, phScheduler);
}

ze_result_t SysmanDevice::rasGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_ras_handle_t *phRas) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->rasGet(pCount, phRas);
}

ze_result_t SysmanDevice::diagnosticsGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_diag_handle_t *phDiagnostics) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->diagnosticsGet(pCount, phDiagnostics);
}

ze_result_t SysmanDevice::firmwareGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_firmware_handle_t *phFirmware) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->firmwareGet(pCount, phFirmware);
}

ze_result_t SysmanDevice::deviceGetProperties(zes_device_handle_t hDevice, zes_device_properties_t *pProperties) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->deviceGetProperties(pProperties);
}

ze_result_t SysmanDevice::processesGetState(zes_device_handle_t hDevice, uint32_t *pCount, zes_process_state_t *pProcesses) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->processesGetState(pCount, pProcesses);
}

ze_result_t SysmanDevice::deviceReset(zes_device_handle_t hDevice, ze_bool_t force) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->deviceReset(force);
}
ze_result_t SysmanDevice::deviceGetState(zes_device_handle_t hDevice, zes_device_state_t *pState) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->deviceGetState(pState);
}

ze_result_t SysmanDevice::standbyGet(zes_device_handle_t hDevice, uint32_t *pCount, zes_standby_handle_t *phStandby) {
    auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(hDevice);
    return pSysmanDevice->standbyGet(pCount, phStandby);
}

} // namespace Sysman
} // namespace L0
