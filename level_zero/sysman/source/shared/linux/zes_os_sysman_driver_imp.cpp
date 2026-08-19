/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/zes_os_sysman_driver_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/debug_helpers.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/hw_device_id.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/directory.h"

#include "level_zero/sysman/source/api/events/linux/sysman_os_events_imp.h"
#include "level_zero/sysman/source/api/info_log/sysman_info_log.h"
#include "level_zero/sysman/source/device/sysman_device.h"
#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/driver/sysman_driver_handle_imp.h"
#include "level_zero/sysman/source/shared/linux/sysman_hw_device_id_linux.h"
#include "level_zero/sysman/source/shared/linux/sysman_sys_calls_wrapper.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

ze_result_t LinuxSysmanDriverImp::eventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) {
    return driverEventsListen(timeout, count, phDevices, pNumDeviceEvents, pEvents, nullptr);
}

ze_result_t LinuxSysmanDriverImp::driverEventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents, zes_event_type_flags_t *pDriverEvents) {
    ze_result_t res = pLinuxEventsUtil->eventsListen(timeout, count, phDevices, pNumDeviceEvents, pEvents, pDriverEvents);
    if (ZE_RESULT_SUCCESS != res) {
        return res;
    }

    // handle runtime survivability event
    for (uint32_t index = 0; index < count; index++) {
        if (pEvents[index] & ZES_EVENT_TYPE_FLAG_SURVIVABILITY_MODE_DETECTED) {
            auto pSysmanDevice = L0::Sysman::SysmanDevice::fromHandle(phDevices[index]);
            pSysmanDevice->isDeviceInSurvivabilityMode = true;
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Device %d got Survivability event\n", index);
        }
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxSysmanDriverImp::enumInfoLogs(uint32_t *pCount, zes_intel_info_log_handle_t *phInfoLogs) {

    if (pInfoLogHandleContext == nullptr) {
        pInfoLogHandleContext = new InfoLogHandleContext();
    }

    return pInfoLogHandleContext->infoLogGet(pCount, phInfoLogs);
}

void LinuxSysmanDriverImp::eventRegister(zes_event_type_flags_t events, SysmanDeviceImp *pSysmanDevice) {
    pLinuxEventsUtil->eventRegister(events, pSysmanDevice);
}

ze_result_t LinuxSysmanDriverImp::driverEventRegister(zes_event_type_flags_t events) {
    return pLinuxEventsUtil->driverEventRegister(events);
}

ze_result_t LinuxSysmanDriverImp::getPciBdfAndUuidForHwDevice(NEO::HwDeviceId *hwDeviceId, std::string &pciBdf, std::string &pciUuid) {
    if (hwDeviceId->getDriverModelType() != NEO::DriverModelType::drm) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): driver model is not DRM and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNKNOWN);
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    auto hwDeviceIdDrm = hwDeviceId->as<NEO::HwDeviceIdDrm>();
    pciBdf = hwDeviceIdDrm->getPciPath();

    pciUuid.assign(64, '\0');
    std::string uuidPath = "/sys/bus/pci/devices/" + pciBdf + "/device_uuid";

    int errorNum = 0;
    int fd = SysmanSysCallsWrapper::open(uuidPath.c_str(), O_RDONLY, errorNum);
    if (fd < 0) {
        pciUuid = "";
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): open() failed to open %s (errno:%d) and returning error:0x%x \n", __FUNCTION__, uuidPath.c_str(), errorNum, LinuxSysmanImp::getResult(errorNum));
        return LinuxSysmanImp::getResult(errorNum);
    }

    ssize_t bytesRead = SysmanSysCallsWrapper::read(fd, pciUuid.data(), pciUuid.size() - 1, errorNum);
    SysmanSysCallsWrapper::close(fd, errorNum);

    if (bytesRead <= 0) {
        pciUuid = "";
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): read() failed to read %s (errno:%d) and returning error:0x%x \n", __FUNCTION__, uuidPath.c_str(), errorNum, LinuxSysmanImp::getResult(errorNum));
        return LinuxSysmanImp::getResult(errorNum);
    }

    std::replace(pciUuid.begin(), pciUuid.end(), '\n', '\0');
    pciUuid.resize(bytesRead);
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxSysmanDriverImp::updateHwDeviceId(SysmanDevice *sysmanDevice, const std::string &newBdf) {
    auto sysmanDeviceImp = static_cast<SysmanDeviceImp *>(sysmanDevice);
    auto &rootDeviceEnv = sysmanDeviceImp->getRootDeviceEnvironmentRef();
    auto executionEnvironment = sysmanDeviceImp->getExecutionEnvironment();

    // Get the Drm object
    auto driverModel = rootDeviceEnv.osInterface->getDriverModel();
    if (driverModel->getDriverModelType() != NEO::DriverModelType::drm) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): driver model is not DRM and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    auto drm = driverModel->as<NEO::Drm>();

    // Discover device at new BDF location
    std::string newPciPath = newBdf;
    auto hwDeviceIds = NEO::Drm::discoverDevice(*executionEnvironment, newPciPath);

    if (hwDeviceIds.empty()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): discoverDevice() found no device at BDF %s and returning error:0x%x \n", __FUNCTION__, newBdf.c_str(), ZE_RESULT_ERROR_DEVICE_LOST);
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }

    auto neoHwDeviceId = std::move(hwDeviceIds[0]);
    auto neoHwDeviceIdDrm = static_cast<NEO::HwDeviceIdDrm *>(neoHwDeviceId.get());
    auto sysmanHwDeviceIdDrm = std::make_unique<SysmanHwDeviceIdDrm>(-1, neoHwDeviceIdDrm->getPciPath(), neoHwDeviceIdDrm->getDeviceNode());

    // Replace Drm's hwDeviceId without destroying Drm object
    drm->getHwDeviceId() = std::move(sysmanHwDeviceIdDrm);

    // Re-query BDF from new hwDeviceId (updates adapterBDF and pciDomain)
    if (drm->queryAdapterBDF() != 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): queryAdapterBDF() failed for BDF %s and returning error:0x%x \n", __FUNCTION__, newBdf.c_str(), ZE_RESULT_ERROR_UNINITIALIZED);
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxSysmanDriverImp::rescanDevices(SysmanDriverHandleImp *driverHandle, uint32_t *pCount, zes_device_handle_t *phDevices) {
    if (driverHandle->sysmanDevices.empty()) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): no sysman devices available and returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_UNINITIALIZED);
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    // Get execution environment from first device
    auto sysmanDevice = static_cast<SysmanDeviceImp *>(driverHandle->sysmanDevices[0]);
    auto executionEnvironment = sysmanDevice->getExecutionEnvironment();

    // Discover all devices currently on PCI bus
    auto discoveredDevices = NEO::OSInterface::discoverDevices(*executionEnvironment);

    if (*pCount == 0 && phDevices == nullptr) {
        *pCount = static_cast<uint32_t>(discoveredDevices.size());
        return ZE_RESULT_SUCCESS;
    }

    // Check each discovered device for BDF changes
    for (const auto &hwDeviceId : discoveredDevices) {
        std::string pciBdf;
        std::string pciUuid;
        ze_result_t result = getPciBdfAndUuidForHwDevice(hwDeviceId.get(), pciBdf, pciUuid);
        if (result != ZE_RESULT_SUCCESS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): getPciBdfAndUuidForHwDevice() failed and returning error:0x%x \n", __FUNCTION__, result);
            return result;
        }

        // Extract BDF components for comparison
        uint16_t domain = 0;
        uint8_t bus = 0, device = 0, function = 0;
        constexpr int bdfTokensNum = 4;
        if (NEO::parseBdfString(pciBdf, domain, bus, device, function) != bdfTokensNum) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): parseBdfString() failed to parse BDF %s and returning error:0x%x \n", __FUNCTION__, pciBdf.c_str(), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
            return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
        }

        auto it = driverHandle->pciUuidToPciBusInfoMap.find(pciUuid);
        if (it == driverHandle->pciUuidToPciBusInfoMap.end()) {
            continue;
        }

        const auto &cachedBusInfo = it->second;
        if (cachedBusInfo == nullptr) {
            continue;
        }

        if (domain == cachedBusInfo->pciDomain && bus == cachedBusInfo->pciBus &&
            device == cachedBusInfo->pciDevice &&
            function == cachedBusInfo->pciFunction) {
            continue;
        }

        int32_t deviceIndex = findDeviceIndexByPciUuid(driverHandle, pciUuid);
        if (deviceIndex < 0) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): findDeviceIndexByPciUuid() found no matching device for relocated BDF %s and returning error:0x%x \n", __FUNCTION__, pciBdf.c_str(), ZE_RESULT_ERROR_UNKNOWN);
            return ZE_RESULT_ERROR_UNKNOWN;
        }

        auto relocatedSysmanDevice = static_cast<SysmanDeviceImp *>(driverHandle->sysmanDevices[deviceIndex]);
        result = updateHwDeviceId(relocatedSysmanDevice, pciBdf);
        if (result != ZE_RESULT_SUCCESS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): updateHwDeviceId() failed for BDF %s and returning error:0x%x \n", __FUNCTION__, pciBdf.c_str(), result);
            return result;
        }

        result = static_cast<LinuxSysmanImp *>(relocatedSysmanDevice->pOsSysman)->updateBdfDependentData();
        if (result != ZE_RESULT_SUCCESS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): updateBdfDependentData() failed for BDF %s and returning error:0x%x \n", __FUNCTION__, pciBdf.c_str(), result);
            return result;
        }
        driverHandle->updatePciUuidMap(relocatedSysmanDevice);
        driverHandle->updateUuidMap(relocatedSysmanDevice);
    }
    return driverHandle->getDevice(pCount, phDevices);
}

int32_t LinuxSysmanDriverImp::findDeviceIndexByPciUuid(SysmanDriverHandleImp *driverHandle, const std::string &pciUuid) {
    for (uint32_t i = 0; i < driverHandle->sysmanDevices.size(); i++) {
        auto sysmanDevice = static_cast<SysmanDeviceImp *>(driverHandle->sysmanDevices[i]);
        auto osSysman = sysmanDevice->pOsSysman;

        if (!osSysman) {
            continue;
        }

        if (osSysman->getPciUuid() == pciUuid) {
            return static_cast<int32_t>(i);
        }
    }

    return -1;
}

L0::Sysman::UdevLib *LinuxSysmanDriverImp::getUdevLibHandle() {
    if (pUdevLib == nullptr) {
        pUdevLib = UdevLib::create();
    }
    return pUdevLib;
}

LinuxSysmanDriverImp::LinuxSysmanDriverImp() {
    pLinuxEventsUtil = new LinuxEventsUtil(this);
}

LinuxSysmanDriverImp::~LinuxSysmanDriverImp() {
    // Clean up netlink resources (eventSocket) if initialized
    netlinkCleanup();

    if (nullptr != pUdevLib) {
        delete pUdevLib;
        pUdevLib = nullptr;
    }

    if (nullptr != pLinuxEventsUtil) {
        delete pLinuxEventsUtil;
        pLinuxEventsUtil = nullptr;
    }

    if (nullptr != pInfoLogHandleContext) {
        if (cperTracePipeFd >= 0) {
            pInfoLogHandleContext->disableInfoLogCollection();
        }

        delete pInfoLogHandleContext;
        pInfoLogHandleContext = nullptr;
    }

    if (cperTracePipeFd >= 0) {
        int errorNum = 0;
        SysmanSysCallsWrapper::close(cperTracePipeFd, errorNum);
        cperTracePipeFd = -1;
    }
}

OsSysmanDriver *OsSysmanDriver::create() {
    LinuxSysmanDriverImp *pLinuxSysmanDriverImp = new LinuxSysmanDriverImp();
    DEBUG_BREAK_IF(nullptr == pLinuxSysmanDriverImp);
    return static_cast<OsSysmanDriver *>(pLinuxSysmanDriverImp);
}

DrmNlApi *LinuxSysmanDriverImp::getDrmNlApiHandle() {
    if (pDrmNl == nullptr) {
        pDrmNl = LinuxSysmanDriverImp::createDrmNlApi();
    }
    return pDrmNl;
}

void LinuxSysmanDriverImp::netlinkCleanup() {
    LinuxSysmanDriverImp::destroyDrmNlApi(pDrmNl);
    pDrmNl = nullptr;
}

} // namespace Sysman
} // namespace L0
