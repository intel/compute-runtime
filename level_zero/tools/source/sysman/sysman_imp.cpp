/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/sysman_imp.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/tools/source/sysman/events/events_imp.h"
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
    pEvents = new EventsImp(pOsSysman);
    pFanHandleContext = new FanHandleContext(pOsSysman);
    pFirmwareHandleContext = new FirmwareHandleContext(pOsSysman);
    pDiagnosticsHandleContext = new DiagnosticsHandleContext(pOsSysman);
    pPerformanceHandleContext = new PerformanceHandleContext(pOsSysman);
}

SysmanDeviceImp::~SysmanDeviceImp() {
    freeResource(pPerformanceHandleContext);
    freeResource(pDiagnosticsHandleContext);
    freeResource(pFirmwareHandleContext);
    freeResource(pFanHandleContext);
    freeResource(pEvents);
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

void SysmanDeviceImp::updateSubDeviceHandlesLocally() {
    uint32_t subDeviceCount = 0;
    deviceHandles.clear();
    Device::fromHandle(hCoreDevice)->getSubDevices(&subDeviceCount, nullptr);
    if (subDeviceCount == 0) {
        deviceHandles.resize(1, hCoreDevice);
    } else {
        deviceHandles.resize(subDeviceCount, nullptr);
        Device::fromHandle(hCoreDevice)->getSubDevices(&subDeviceCount, deviceHandles.data());
    }
}

void SysmanDeviceImp::getSysmanDeviceInfo(zes_device_handle_t hDevice, uint32_t &subdeviceId, ze_bool_t &onSubdevice) {
    NEO::Device *neoDevice = Device::fromHandle(hDevice)->getNEODevice();
    onSubdevice = static_cast<ze_bool_t>(false);
    if (NEO::HwHelper::getSubDevicesCount(&neoDevice->getHardwareInfo()) > 1) {
        onSubdevice = static_cast<ze_bool_t>(true);
    }
    if (!neoDevice->isSubDevice()) {                                  // To get physical device or subdeviceIndex Index in case when the device does not support tile architecture is single tile device
        UNRECOVERABLE_IF(neoDevice->getDeviceBitfield().count() != 1) // or the device is single tile device or AFFINITY_MASK only exposes single tile
        subdeviceId = Math::log2(static_cast<uint32_t>(neoDevice->getDeviceBitfield().to_ulong()));
    } else {
        subdeviceId = static_cast<NEO::SubDevice *>(neoDevice)->getSubDeviceIndex();
    }
}

ze_result_t SysmanDeviceImp::init() {
    // We received a device handle. Check for subdevices in this device
    updateSubDeviceHandlesLocally();

    auto result = pOsSysman->init();
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    if (pPowerHandleContext) {
        pPowerHandleContext->init(deviceHandles, hCoreDevice);
    }
    if (pFrequencyHandleContext) {
        pFrequencyHandleContext->init(deviceHandles);
    }
    if (pFabricPortHandleContext) {
        pFabricPortHandleContext->init();
    }
    if (pTempHandleContext) {
        pTempHandleContext->init(deviceHandles);
    }
    if (pPci) {
        pPci->init();
    }
    if (pStandbyHandleContext) {
        pStandbyHandleContext->init(deviceHandles);
    }
    if (pEngineHandleContext) {
        pEngineHandleContext->init();
    }
    if (pSchedulerHandleContext) {
        pSchedulerHandleContext->init(deviceHandles);
    }
    if (pRasHandleContext) {
        pRasHandleContext->init(deviceHandles);
    }
    if (pMemoryHandleContext) {
        pMemoryHandleContext->init(deviceHandles);
    }
    if (pGlobalOperations) {
        pGlobalOperations->init();
    }
    if (pEvents) {
        pEvents->init();
    }
    if (pFanHandleContext) {
        pFanHandleContext->init();
    }
    if (pFirmwareHandleContext) {
        pFirmwareHandleContext->init();
    }
    if (pDiagnosticsHandleContext) {
        pDiagnosticsHandleContext->init(deviceHandles);
    }
    if (pPerformanceHandleContext) {
        pPerformanceHandleContext->init(deviceHandles, hCoreDevice);
    }
    return result;
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

ze_result_t SysmanDeviceImp::deviceEventRegister(zes_event_type_flags_t events) {
    return pEvents->eventRegister(events);
}

bool SysmanDeviceImp::deviceEventListen(zes_event_type_flags_t &pEvent, uint64_t timeout) {
    return pEvents->eventListen(pEvent, timeout);
}

ze_result_t SysmanDeviceImp::deviceGetState(zes_device_state_t *pState) {
    return pGlobalOperations->deviceGetState(pState);
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

ze_result_t SysmanDeviceImp::diagnosticsGet(uint32_t *pCount, zes_diag_handle_t *phDiagnostics) {
    return pDiagnosticsHandleContext->diagnosticsGet(pCount, phDiagnostics);
}

ze_result_t SysmanDeviceImp::memoryGet(uint32_t *pCount, zes_mem_handle_t *phMemory) {
    return pMemoryHandleContext->memoryGet(pCount, phMemory);
}

ze_result_t SysmanDeviceImp::fanGet(uint32_t *pCount, zes_fan_handle_t *phFan) {
    return pFanHandleContext->fanGet(pCount, phFan);
}

ze_result_t SysmanDeviceImp::performanceGet(uint32_t *pCount, zes_perf_handle_t *phPerformance) {
    return pPerformanceHandleContext->performanceGet(pCount, phPerformance);
}

} // namespace L0
