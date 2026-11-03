/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/zes_os_sysman_driver_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
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
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"
#include "level_zero/sysman/source/shared/linux/sysman_hw_device_id_linux.h"
#include "level_zero/sysman/source/shared/linux/sysman_sys_calls_wrapper.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <algorithm>

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

ze_result_t LinuxSysmanDriverImp::enumInfoLogs(uint32_t *pCount, zes_info_log_handle_t *phInfoLogs) {

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

ze_result_t LinuxSysmanDriverImp::getPciBdfForHwDevice(NEO::HwDeviceId *hwDeviceId, std::string &pciBdf) {
    if (hwDeviceId->getDriverModelType() != NEO::DriverModelType::drm) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): driver model is not DRM and returning error:0x%x \n", NEO_FUNCTION_NAME, ZE_RESULT_ERROR_UNKNOWN);
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    pciBdf = hwDeviceId->as<NEO::HwDeviceIdDrm>()->getPciPath();
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxSysmanDriverImp::getPciUuidForHwDevice(NEO::HwDeviceId *hwDeviceId, std::string &pciUuid) {
    std::string pciBdf;
    ze_result_t result = getPciBdfForHwDevice(hwDeviceId, pciBdf);
    if (result != ZE_RESULT_SUCCESS) {
        pciUuid = "";
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): getPciBdfForHwDevice() failed and returning error:0x%x \n", NEO_FUNCTION_NAME, result);
        return result;
    }

    std::string uuidPath = "/sys/bus/pci/devices/" + pciBdf + "/device_uuid";
    result = FsAccessInterface::create()->read(uuidPath, pciUuid);
    if (result != ZE_RESULT_SUCCESS) {
        pciUuid.clear();
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): failed to read %s and returning error:0x%x \n", NEO_FUNCTION_NAME, uuidPath.c_str(), result);
    }
    return result;
}

SysmanDevice *LinuxSysmanDriverImp::getSysmanDeviceByPciUuid(const SysmanDriverHandleImp *driverHandle, const std::string &pciUuid) {
    if (pciUuid.empty()) {
        return nullptr;
    }
    for (auto sysmanDevice : driverHandle->sysmanDevices) {
        if (!sysmanDevice->isDeviceInSurvivabilityMode &&
            static_cast<LinuxSysmanImp *>(static_cast<SysmanDeviceImp *>(sysmanDevice)->deviceGetOsInterface())->getPciUuid() == pciUuid) {
            return sysmanDevice;
        }
    }
    return nullptr;
}

SysmanDevice *LinuxSysmanDriverImp::getSysmanDeviceByPciBusInfo(const SysmanDriverHandleImp *driverHandle, const NEO::PhysicalDevicePciBusInfo &pciBusInfo) {
    for (auto sysmanDevice : driverHandle->sysmanDevices) {
        if (sysmanDevice->isDeviceInSurvivabilityMode) {
            continue;
        }
        auto deviceBusInfo = static_cast<SysmanDeviceImp *>(sysmanDevice)->deviceGetOsInterface()->getPciBdfInfo();
        if (deviceBusInfo != nullptr &&
            deviceBusInfo->pciDomain == pciBusInfo.pciDomain && deviceBusInfo->pciBus == pciBusInfo.pciBus &&
            deviceBusInfo->pciDevice == pciBusInfo.pciDevice && deviceBusInfo->pciFunction == pciBusInfo.pciFunction) {
            return sysmanDevice;
        }
    }
    return nullptr;
}

void LinuxSysmanDriverImp::removeLostSysmanDevices(SysmanDriverHandleImp *driverHandle, const std::vector<SysmanDevice *> &foundDevices) {
    auto knownDevices = driverHandle->sysmanDevices;
    for (auto sysmanDevice : knownDevices) {
        if (sysmanDevice->isDeviceInSurvivabilityMode ||
            std::find(foundDevices.begin(), foundDevices.end(), sysmanDevice) != foundDevices.end()) {
            continue;
        }
        driverHandle->moveSysmanDeviceToLost(sysmanDevice);
    }
}

ze_result_t LinuxSysmanDriverImp::updateHwDeviceId(SysmanDevice *sysmanDevice, NEO::HwDeviceId *hwDeviceId) {
    auto sysmanDeviceImp = static_cast<SysmanDeviceImp *>(sysmanDevice);
    auto &rootDeviceEnv = sysmanDeviceImp->getRootDeviceEnvironmentRef();

    auto driverModel = rootDeviceEnv.osInterface->getDriverModel();
    if (driverModel->getDriverModelType() != NEO::DriverModelType::drm || hwDeviceId->getDriverModelType() != NEO::DriverModelType::drm) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): driver model is not DRM and returning error:0x%x \n", NEO_FUNCTION_NAME, ZE_RESULT_ERROR_UNSUPPORTED_FEATURE);
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    auto drm = driverModel->as<NEO::Drm>();

    auto neoHwDeviceIdDrm = hwDeviceId->as<NEO::HwDeviceIdDrm>();
    auto sysmanHwDeviceIdDrm = std::make_unique<SysmanHwDeviceIdDrm>(-1, neoHwDeviceIdDrm->getPciPath(), neoHwDeviceIdDrm->getDeviceNode());

    // Replace Drm's hwDeviceId without destroying Drm object
    drm->getHwDeviceId() = std::move(sysmanHwDeviceIdDrm);

    // Re-query BDF from new hwDeviceId (updates adapterBDF and pciDomain)
    if (drm->queryAdapterBDF() != 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): queryAdapterBDF() failed for BDF %s and returning error:0x%x \n", NEO_FUNCTION_NAME, neoHwDeviceIdDrm->getPciPath(), ZE_RESULT_ERROR_UNINITIALIZED);
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxSysmanDriverImp::reinitDeviceCachedInfo(SysmanDevice *sysmanDevice, NEO::HwDeviceId *hwDeviceId) {
    auto sysmanDeviceImp = static_cast<SysmanDeviceImp *>(sysmanDevice);
    ze_result_t result = updateHwDeviceId(sysmanDeviceImp, hwDeviceId);
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): updateHwDeviceId() failed and returning error:0x%x \n", NEO_FUNCTION_NAME, result);
        return result;
    }

    result = static_cast<LinuxSysmanImp *>(sysmanDeviceImp->pOsSysman)->updateBdfDependentData();
    if (result != ZE_RESULT_SUCCESS) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): updateBdfDependentData() failed and returning error:0x%x \n", NEO_FUNCTION_NAME, result);
        return result;
    }
    return ZE_RESULT_SUCCESS;
}

ze_result_t LinuxSysmanDriverImp::rescanDevices(SysmanDriverHandleImp *driverHandle, uint32_t *pCount, zes_device_handle_t *phDevices) {
    auto executionEnvironment = driverHandle->getExecutionEnvironment();
    if (executionEnvironment == nullptr) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): execution environment not available and returning error:0x%x \n", NEO_FUNCTION_NAME, ZE_RESULT_ERROR_UNINITIALIZED);
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }

    // Discover all devices currently on PCI bus
    auto discoveredDevices = NEO::OSInterface::discoverDevices(*executionEnvironment);

    if (*pCount == 0 && phDevices == nullptr) {
        // Survivability devices can't serve ioctls so discovery never sees them, but rescan keeps them in the device list.
        auto survivabilityDeviceCount = std::count_if(driverHandle->sysmanDevices.begin(), driverHandle->sysmanDevices.end(),
                                                      [](const auto sysmanDevice) -> bool { return sysmanDevice->isDeviceInSurvivabilityMode; });
        *pCount = static_cast<uint32_t>(discoveredDevices.size() + survivabilityDeviceCount);
        return ZE_RESULT_SUCCESS;
    }

    std::vector<SysmanDevice *> foundDevices;
    for (const auto &hwDeviceId : discoveredDevices) {
        std::string pciBdf;
        ze_result_t result = getPciBdfForHwDevice(hwDeviceId.get(), pciBdf);
        if (result != ZE_RESULT_SUCCESS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): getPciBdfForHwDevice() failed and returning error:0x%x \n", NEO_FUNCTION_NAME, result);
            return result;
        }

        SysmanDevice *pSysmanDevice = nullptr;
        std::string pciUuid;
        result = getPciUuidForHwDevice(hwDeviceId.get(), pciUuid);
        if (result == ZE_RESULT_SUCCESS) {
            pSysmanDevice = getSysmanDeviceByPciUuid(driverHandle, pciUuid);
            if (pSysmanDevice == nullptr) {
                continue;
            }
        } else {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): getPciUuidForHwDevice() failed for BDF %s with error:0x%x, falling back to BDF lookup \n", NEO_FUNCTION_NAME, pciBdf.c_str(), result);
            uint16_t domain = 0;
            uint8_t bus = 0, device = 0, function = 0;
            constexpr int bdfTokensNum = 4;
            if (NEO::parseBdfString(pciBdf, domain, bus, device, function) != bdfTokensNum) {
                PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): parseBdfString() failed to parse BDF %s and returning error:0x%x \n", NEO_FUNCTION_NAME, pciBdf.c_str(), ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE);
                return ZE_RESULT_ERROR_DEPENDENCY_UNAVAILABLE;
            }

            pSysmanDevice = getSysmanDeviceByPciBusInfo(driverHandle, NEO::PhysicalDevicePciBusInfo(domain, bus, device, function));
            if (pSysmanDevice == nullptr) {
                continue;
            }
        }

        result = this->reinitDeviceCachedInfo(pSysmanDevice, hwDeviceId.get());
        if (result != ZE_RESULT_SUCCESS) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): reinitDeviceCachedInfo() failed for BDF %s and returning error:0x%x \n", NEO_FUNCTION_NAME, pciBdf.c_str(), result);
            return result;
        }
        foundDevices.push_back(pSysmanDevice);
    }

    removeLostSysmanDevices(driverHandle, foundDevices);
    return driverHandle->getDevice(pCount, phDevices);
}

template <typename Type>
static ze_result_t readPciDeviceAttribute(const std::string &devicePath, const char *attribute, Type &value) {
    std::string attributePath = devicePath + "/" + attribute;
    int errorNum = 0;
    int fd = SysmanSysCallsWrapper::open(attributePath.c_str(), O_RDONLY, errorNum);
    if (fd < 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): open() failed to open %s (errno:%d) \n", NEO_FUNCTION_NAME, attributePath.c_str(), errorNum);
        return LinuxSysmanImp::getResult(errorNum);
    }

    char buffer[16] = {};
    ssize_t bytesRead = SysmanSysCallsWrapper::read(fd, buffer, sizeof(buffer) - 1, errorNum);
    int savedErrorNum = errorNum;
    SysmanSysCallsWrapper::close(fd, errorNum);

    if (bytesRead <= 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): read() failed to read %s (errno:%d) \n", NEO_FUNCTION_NAME, attributePath.c_str(), savedErrorNum);
        return LinuxSysmanImp::getResult(savedErrorNum);
    }

    buffer[bytesRead] = '\0';
    value = static_cast<Type>(std::strtoul(buffer, nullptr, 16));
    return ZE_RESULT_SUCCESS;
}

static bool isDeviceVirtual(const std::string &devicePath) {
    int errorNum = 0;
    return SysmanSysCallsWrapper::access(devicePath + "/physfn", F_OK, errorNum) == 0;
}

static uint32_t getPciConfigDword(const uint8_t *configHeader, uint32_t offset) {
    uint32_t value = 0;
    memcpy_s(&value, sizeof(value), configHeader + offset, sizeof(value));
    return value;
}

static zes_pci_link_status_t getLinkStatusFromConfigHeader(const uint8_t *configHeader) {
    constexpr uint32_t pciConfigVendorAndDeviceIdOffset = 0x00u;
    constexpr uint32_t pciConfigRevisionIdOffset = 0x08u;
    constexpr uint32_t pciConfigHeaderTypeOffset = 0x0eu;
    constexpr uint32_t pciConfigBar0Offset = 0x10u;
    constexpr uint32_t pciMaxStandardBars = 6u; // BAR0 to BAR5
    constexpr uint8_t pciHeaderTypeLayoutMask = 0x7fu;
    constexpr uint8_t pciHeaderTypeEndpoint = 0x00u;
    constexpr uint32_t pciAllOnesDword = 0xffffffffu;
    constexpr uint8_t pciAllOnesByte = 0xffu;

    if (getPciConfigDword(configHeader, pciConfigVendorAndDeviceIdOffset) == pciAllOnesDword) {
        return ZES_PCI_LINK_STATUS_LINK_ERROR;
    }

    if (configHeader[pciConfigRevisionIdOffset] == pciAllOnesByte) {
        return ZES_PCI_LINK_STATUS_LINK_ERROR;
    }

    if ((configHeader[pciConfigHeaderTypeOffset] & pciHeaderTypeLayoutMask) != pciHeaderTypeEndpoint) {
        return ZES_PCI_LINK_STATUS_LINK_ERROR;
    }

    for (uint32_t barIndex = 0; barIndex < pciMaxStandardBars; barIndex++) {
        uint32_t bar = getPciConfigDword(configHeader, pciConfigBar0Offset + (barIndex * sizeof(uint32_t)));
        if (bar == pciAllOnesDword) {
            return ZES_PCI_LINK_STATUS_LINK_ERROR;
        }
    }

    return ZES_PCI_LINK_STATUS_GOOD;
}

static zes_pci_link_status_t getPciDeviceLinkStatus(const std::string &devicePath, bool &deviceRemoved) {
    constexpr size_t pciStandardHeaderSize = 64u;

    std::string configPath = devicePath + "/config";
    int errorNum = 0;
    int fd = SysmanSysCallsWrapper::open(configPath.c_str(), O_RDONLY, errorNum);
    if (fd < 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): open() failed to open %s (errno:%d) \n", NEO_FUNCTION_NAME, configPath.c_str(), errorNum);
        if (errorNum == ENOENT) {
            deviceRemoved = true;
            return ZES_PCI_LINK_STATUS_UNKNOWN;
        }
        if ((errorNum == EACCES) || (errorNum == EPERM)) {
            return ZES_PCI_LINK_STATUS_UNKNOWN;
        }
        return ZES_PCI_LINK_STATUS_LINK_ERROR;
    }

    uint8_t configHeader[pciStandardHeaderSize] = {};
    ssize_t bytesRead = SysmanSysCallsWrapper::read(fd, configHeader, sizeof(configHeader), errorNum);
    int savedErrorNum = errorNum;
    SysmanSysCallsWrapper::close(fd, errorNum);

    if (bytesRead < static_cast<ssize_t>(sizeof(configHeader))) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): read() returned %zd of %zu bytes of %s (errno:%d) \n", NEO_FUNCTION_NAME, bytesRead, sizeof(configHeader), configPath.c_str(), savedErrorNum);
        if ((savedErrorNum == EACCES) || (savedErrorNum == EPERM)) {
            return ZES_PCI_LINK_STATUS_UNKNOWN;
        }
        return ZES_PCI_LINK_STATUS_LINK_ERROR;
    }

    return getLinkStatusFromConfigHeader(configHeader);
}

struct PciDeviceCandidate {
    std::string devicePath;
    zes_pci_address_t address = {};
    uint16_t vendorId = 0;
    uint16_t deviceId = 0;
    uint16_t subsystemVendorId = 0;
    uint16_t subsystemDeviceId = 0;
    uint8_t revision = 0;
    zes_pci_link_status_t status = ZES_PCI_LINK_STATUS_UNKNOWN;
};

static bool isKnownGpuDeviceId(uint16_t deviceId) {
    for (size_t i = 0; NEO::deviceDescriptorTable[i].deviceId != 0; i++) {
        if (deviceId == NEO::deviceDescriptorTable[i].deviceId) {
            return true;
        }
    }
    return false;
}

static bool getPciDeviceCandidate(const std::string &devicePath, PciDeviceCandidate &candidate) {
    constexpr uint32_t intelPciVendorId = 0x8086u;
    constexpr uint32_t pciClassCodeBaseClassShift = 16u;
    constexpr uint32_t pciBaseClassDisplayController = 0x03u;
    constexpr uint32_t pciBaseClassProcessingAccelerator = 0x12u;

    std::string bdfString = devicePath.substr(devicePath.find_last_of('/') + 1);
    uint16_t domain = 0;
    uint8_t bus = 0, device = 0, function = 0;
    constexpr int bdfTokensNum = 4;
    if (NEO::parseBdfString(bdfString, domain, bus, device, function) != bdfTokensNum) {
        return false;
    }

    uint16_t vendorId = 0;
    if (readPciDeviceAttribute(devicePath, "vendor", vendorId) != ZE_RESULT_SUCCESS) {
        return false;
    }
    if (vendorId != intelPciVendorId) {
        return false;
    }

    uint32_t classCode = 0;
    if (readPciDeviceAttribute(devicePath, "class", classCode) != ZE_RESULT_SUCCESS) {
        return false;
    }
    uint32_t baseClass = (classCode >> pciClassCodeBaseClassShift) & 0xffu;
    if ((baseClass != pciBaseClassDisplayController) && (baseClass != pciBaseClassProcessingAccelerator)) {
        return false;
    }

    if (readPciDeviceAttribute(devicePath, "device", candidate.deviceId) != ZE_RESULT_SUCCESS) {
        return false;
    }
    if (!isKnownGpuDeviceId(candidate.deviceId)) {
        return false;
    }

    if (isDeviceVirtual(devicePath)) {
        return false;
    }

    readPciDeviceAttribute(devicePath, "subsystem_vendor", candidate.subsystemVendorId);
    readPciDeviceAttribute(devicePath, "subsystem_device", candidate.subsystemDeviceId);
    readPciDeviceAttribute(devicePath, "revision", candidate.revision);

    candidate.devicePath = devicePath;
    candidate.address.domain = static_cast<uint32_t>(domain);
    candidate.address.bus = static_cast<uint32_t>(bus);
    candidate.address.device = static_cast<uint32_t>(device);
    candidate.address.function = static_cast<uint32_t>(function);
    candidate.vendorId = vendorId;
    return true;
}

static std::vector<PciDeviceCandidate> getIntelGpuDeviceCandidates() {
    const std::string pciSysfsDevicesDirectory = "/sys/bus/pci/devices";
    std::vector<PciDeviceCandidate> candidates;

    // Only the sysfs attributes cached by the kernel are read, the config space is not accessed so that
    // the devices are not woken up
    for (const auto &devicePath : NEO::Directory::getFiles(pciSysfsDevicesDirectory)) {
        PciDeviceCandidate candidate = {};
        if (getPciDeviceCandidate(devicePath, candidate)) {
            candidates.push_back(std::move(candidate));
        }
    }

    return candidates;
}

static std::vector<PciDeviceCandidate> getIntelGpuDevices() {
    std::vector<PciDeviceCandidate> devices;

    for (auto &candidate : getIntelGpuDeviceCandidates()) {
        bool deviceRemoved = false;
        candidate.status = getPciDeviceLinkStatus(candidate.devicePath, deviceRemoved);
        if (deviceRemoved) {
            continue;
        }

        devices.push_back(std::move(candidate));
    }

    return devices;
}

ze_result_t LinuxSysmanDriverImp::getPciDeviceProperties(uint32_t *pCount, zes_driver_pci_device_properties_t *pProperties) {
    if (pCount == nullptr) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): pCount is nullptr and returning error:0x%x \n", NEO_FUNCTION_NAME, ZE_RESULT_ERROR_INVALID_NULL_POINTER);
        return ZE_RESULT_ERROR_INVALID_NULL_POINTER;
    }

    auto devices = getIntelGpuDevices();
    auto availableCount = static_cast<uint32_t>(devices.size());

    if ((*pCount == 0) || (pProperties == nullptr)) {
        *pCount = availableCount;
        return ZE_RESULT_SUCCESS;
    }

    auto count = std::min(*pCount, availableCount);
    for (uint32_t index = 0; index < count; index++) {
        if (pProperties[index].pNext != nullptr) {
            PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): pProperties[%u].pNext is not nullptr and returning error:0x%x \n", NEO_FUNCTION_NAME, index, ZE_RESULT_ERROR_INVALID_ARGUMENT);
            return ZE_RESULT_ERROR_INVALID_ARGUMENT;
        }
    }

    *pCount = count;
    for (uint32_t index = 0; index < *pCount; index++) {
        const auto &device = devices[index];
        pProperties[index].address = device.address;
        pProperties[index].maxSpeed = {-1, -1, -1};
        pProperties[index].status = device.status;

        auto &configProperties = pProperties[index].configProperties;
        configProperties.stype = ZES_STRUCTURE_TYPE_PCI_CONFIG_PROPERTIES;
        configProperties.pNext = nullptr;
        configProperties.vendorId = device.vendorId;
        configProperties.deviceId = device.deviceId;
        configProperties.subsystemVendorId = device.subsystemVendorId;
        configProperties.subsystemDeviceId = device.subsystemDeviceId;
        configProperties.revision = device.revision;
        configProperties.pcieCapabilityVersion = 0;
        configProperties.supportedLinkSpeeds = 0;
    }

    return ZE_RESULT_SUCCESS;
}

std::vector<zes_pci_address_t> LinuxSysmanDriverImp::getPciDeviceAddresses() {
    // The link status is not read unlike in getPciDeviceProperties(), so this can be called frequently
    // without waking up the devices
    std::vector<zes_pci_address_t> addresses;
    for (const auto &candidate : getIntelGpuDeviceCandidates()) {
        addresses.push_back(candidate.address);
    }
    return addresses;
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
        // Tearing the instances down closes and unregisters their trace_pipe descriptors.
        pInfoLogHandleContext->destroyAllInstances();

        delete pInfoLogHandleContext;
        pInfoLogHandleContext = nullptr;
    }

    clearCperTracePipeFds();
}

// The info log collection instances own their trace_pipe descriptors; the driver only keeps a
// registry of them so that the events path can poll every open stream.
void LinuxSysmanDriverImp::registerCperTracePipeFd(int fd) {
    if (fd < 0) {
        PRINT_STRING(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Refusing to register an invalid trace_pipe descriptor %d, it will not be polled for CPER records\n", NEO_FUNCTION_NAME, fd);
        return;
    }
    std::lock_guard<std::mutex> lock(cperFdsMutex);
    if (std::find(cperTracePipeFds.begin(), cperTracePipeFds.end(), fd) == cperTracePipeFds.end()) {
        cperTracePipeFds.push_back(fd);
    }
}

void LinuxSysmanDriverImp::unregisterCperTracePipeFd(int fd) {
    std::lock_guard<std::mutex> lock(cperFdsMutex);
    auto it = std::find(cperTracePipeFds.begin(), cperTracePipeFds.end(), fd);
    if (it != cperTracePipeFds.end()) {
        cperTracePipeFds.erase(it);
    }
}

std::vector<int> LinuxSysmanDriverImp::getCperTracePipeFds() const {
    std::lock_guard<std::mutex> lock(cperFdsMutex);
    return cperTracePipeFds;
}

int LinuxSysmanDriverImp::getCperTracePipeFd() const {
    std::lock_guard<std::mutex> lock(cperFdsMutex);
    return cperTracePipeFds.empty() ? -1 : cperTracePipeFds.front();
}

// Closes any descriptor still registered after the owning instances are gone, which can only
// happen if an instance leaked its registration.
void LinuxSysmanDriverImp::clearCperTracePipeFds() {
    std::lock_guard<std::mutex> lock(cperFdsMutex);
    for (auto fd : cperTracePipeFds) {
        int errorNum = 0;
        SysmanSysCallsWrapper::close(fd, errorNum);
    }
    cperTracePipeFds.clear();
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
