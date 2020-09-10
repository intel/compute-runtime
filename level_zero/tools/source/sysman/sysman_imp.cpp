/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/sysman_imp.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/tools/source/sysman/global_operations/global_operations_imp.h"
#include "level_zero/tools/source/sysman/pci/pci_imp.h"
#include "level_zero/tools/source/sysman/sysman.h"

#include <vector>

namespace L0 {

SysmanDeviceImp::SysmanDeviceImp(ze_device_handle_t hDevice) {
    hCoreDevice = hDevice;
    pOsSysman = OsSysman::create(this);
    UNRECOVERABLE_IF(nullptr == pOsSysman);
    pPci = new PciImp(pOsSysman);
    pPowerHandleContext = new PowerHandleContext(pOsSysman);
    pFrequencyHandleContext = new FrequencyHandleContext(pOsSysman);
    pFabricPortHandleContext = new FabricPortHandleContext(pOsSysman);
    pTempHandleContext = new TemperatureHandleContext(pOsSysman);
    pStandbyHandleContext = new StandbyHandleContext(pOsSysman);
    pEngineHandleContext = new EngineHandleContext(pOsSysman);
    pSchedulerHandleContext = new SchedulerHandleContext(pOsSysman);
    pRasHandleContext = new RasHandleContext(pOsSysman);
    pMemoryHandleContext = new MemoryHandleContext(pOsSysman);
    pGlobalOperations = new GlobalOperationsImp(pOsSysman);
    pFanHandleContext = new FanHandleContext(pOsSysman);
    pFirmwareHandleContext = new FirmwareHandleContext(pOsSysman);
}

SysmanDeviceImp::~SysmanDeviceImp() {
    freeResource(pFanHandleContext);
    freeResource(pFirmwareHandleContext);
    freeResource(pGlobalOperations);
    freeResource(pMemoryHandleContext);
    freeResource(pRasHandleContext);
    freeResource(pSchedulerHandleContext);
    freeResource(pEngineHandleContext);
    freeResource(pStandbyHandleContext);
    freeResource(pTempHandleContext);
    freeResource(pFabricPortHandleContext);
    freeResource(pPci);
    freeResource(pFrequencyHandleContext);
    freeResource(pPowerHandleContext);
    freeResource(pOsSysman);
}

void SysmanDeviceImp::init() {
    uint32_t subDeviceCount = 0;
    std::vector<ze_device_handle_t> deviceHandles;
    // We received a device handle. Check for subdevices in this device
    Device::fromHandle(hCoreDevice)->getSubDevices(&subDeviceCount, nullptr);
    if (subDeviceCount == 0) {
        deviceHandles.resize(1, hCoreDevice);
    } else {
        deviceHandles.resize(subDeviceCount, nullptr);
        Device::fromHandle(hCoreDevice)->getSubDevices(&subDeviceCount, deviceHandles.data());
    }

    pOsSysman->init();
    if (pPowerHandleContext) {
        pPowerHandleContext->init();
    }
    if (pFrequencyHandleContext) {
        pFrequencyHandleContext->init(deviceHandles);
    }
    if (pFabricPortHandleContext) {
        pFabricPortHandleContext->init();
    }
    if (pTempHandleContext) {
        pTempHandleContext->init();
    }
    if (pPci) {
        pPci->init();
    }
    if (pStandbyHandleContext) {
        pStandbyHandleContext->init();
    }
    if (pEngineHandleContext) {
        pEngineHandleContext->init();
    }
    if (pSchedulerHandleContext) {
        pSchedulerHandleContext->init();
    }
    if (pRasHandleContext) {
        pRasHandleContext->init();
    }
    if (pMemoryHandleContext) {
        pMemoryHandleContext->init();
    }
    if (pGlobalOperations) {
        pGlobalOperations->init();
    }
    if (pFanHandleContext) {
        pFanHandleContext->init();
    }
    if (pFirmwareHandleContext) {
        pFirmwareHandleContext->init();
    }
}

ze_result_t SysmanDeviceImp::frequencyGet(uint32_t *pCount, zes_freq_handle_t *phFrequency) {
    return pFrequencyHandleContext->frequencyGet(pCount, phFrequency);
}

ze_result_t SysmanDeviceImp::deviceGetProperties(zes_device_properties_t *pProperties) {
    return pGlobalOperations->deviceGetProperties(pProperties);
}

ze_result_t SysmanDeviceImp::processesGetState(uint32_t *pCount, zes_process_state_t *pProcesses) {
    return pGlobalOperations->processesGetState(pCount, pProcesses);
}

ze_result_t SysmanDeviceImp::deviceReset(ze_bool_t force) {
    return pGlobalOperations->reset(force);
}

ze_result_t SysmanDeviceImp::deviceGetState(zes_device_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanDeviceImp::pciGetProperties(zes_pci_properties_t *pProperties) {
    return pPci->pciStaticProperties(pProperties);
}

ze_result_t SysmanDeviceImp::pciGetState(zes_pci_state_t *pState) {
    return pPci->pciGetState(pState);
}

ze_result_t SysmanDeviceImp::pciGetBars(uint32_t *pCount, zes_pci_bar_properties_t *pProperties) {
    return pPci->pciGetInitializedBars(pCount, pProperties);
}

ze_result_t SysmanDeviceImp::pciGetStats(zes_pci_stats_t *pStats) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t SysmanDeviceImp::powerGet(uint32_t *pCount, zes_pwr_handle_t *phPower) {
    return pPowerHandleContext->powerGet(pCount, phPower);
}

ze_result_t SysmanDeviceImp::engineGet(uint32_t *pCount, zes_engine_handle_t *phEngine) {
    return pEngineHandleContext->engineGet(pCount, phEngine);
}

ze_result_t SysmanDeviceImp::standbyGet(uint32_t *pCount, zes_standby_handle_t *phStandby) {
    return pStandbyHandleContext->standbyGet(pCount, phStandby);
}

ze_result_t SysmanDeviceImp::fabricPortGet(uint32_t *pCount, zes_fabric_port_handle_t *phPort) {
    return pFabricPortHandleContext->fabricPortGet(pCount, phPort);
}

ze_result_t SysmanDeviceImp::temperatureGet(uint32_t *pCount, zes_temp_handle_t *phTemperature) {
    return pTempHandleContext->temperatureGet(pCount, phTemperature);
}

ze_result_t SysmanDeviceImp::schedulerGet(uint32_t *pCount, zes_sched_handle_t *phScheduler) {
    return pSchedulerHandleContext->schedulerGet(pCount, phScheduler);
}

ze_result_t SysmanDeviceImp::rasGet(uint32_t *pCount, zes_ras_handle_t *phRas) {
    return pRasHandleContext->rasGet(pCount, phRas);
}

ze_result_t SysmanDeviceImp::firmwareGet(uint32_t *pCount, zes_firmware_handle_t *phFirmware) {
    return pFirmwareHandleContext->firmwareGet(pCount, phFirmware);
}

ze_result_t SysmanDeviceImp::memoryGet(uint32_t *pCount, zes_mem_handle_t *phMemory) {
    return pMemoryHandleContext->memoryGet(pCount, phMemory);
}

ze_result_t SysmanDeviceImp::fanGet(uint32_t *pCount, zes_fan_handle_t *phFan) {
    return pFanHandleContext->fanGet(pCount, phFan);
}
} // namespace L0
