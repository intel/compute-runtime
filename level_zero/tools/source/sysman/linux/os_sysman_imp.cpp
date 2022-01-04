/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

#include "shared/source/os_interface/device_factory.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/sysman/linux/fs_access.h"

#include "sysman/linux/firmware_util/firmware_util.h"

#include <linux/pci_regs.h>

namespace L0 {

const std::string LinuxSysmanImp::deviceDir("device");

ze_result_t LinuxSysmanImp::init() {
    pFsAccess = FsAccess::create();
    DEBUG_BREAK_IF(nullptr == pFsAccess);

    if (pProcfsAccess == nullptr) {
        pProcfsAccess = ProcfsAccess::create();
    }
    DEBUG_BREAK_IF(nullptr == pProcfsAccess);

    auto result = initLocalDeviceAndDrmHandles();
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    int myDeviceFd = pDrm->getFileDescriptor();
    std::string myDeviceName;
    result = pProcfsAccess->getFileName(pProcfsAccess->myProcessId(), myDeviceFd, myDeviceName);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    if (pSysfsAccess == nullptr) {
        pSysfsAccess = SysfsAccess::create(myDeviceName);
    }
    DEBUG_BREAK_IF(nullptr == pSysfsAccess);

    pPmuInterface = PmuInterface::create(this);

    DEBUG_BREAK_IF(nullptr == pPmuInterface);

    return createPmtHandles();
}

void LinuxSysmanImp::createFwUtilInterface() {
    std::string realRootPath;
    auto result = pSysfsAccess->getRealPath("device", realRootPath);
    if (ZE_RESULT_SUCCESS != result) {
        return;
    }
    auto rootPciPathOfGpuDevice = getPciRootPortDirectoryPath(realRootPath);
    auto loc = realRootPath.find_last_of('/');
    pFwUtilInterface = FirmwareUtil::create(realRootPath.substr(loc + 1, std::string::npos));
}

ze_result_t LinuxSysmanImp::createPmtHandles() {
    std::string realRootPath;
    auto result = pSysfsAccess->getRealPath("device", realRootPath);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    auto rootPciPathOfGpuDevice = getPciRootPortDirectoryPath(realRootPath);
    PlatformMonitoringTech::create(pParentSysmanDeviceImp->deviceHandles, pFsAccess, rootPciPathOfGpuDevice, mapOfSubDeviceIdToPmtObject);
    return result;
}

PmuInterface *LinuxSysmanImp::getPmuInterface() {
    return pPmuInterface;
}

FirmwareUtil *LinuxSysmanImp::getFwUtilInterface() {
    if (pFwUtilInterface == nullptr) {
        createFwUtilInterface();
    }
    return pFwUtilInterface;
}

PRODUCT_FAMILY LinuxSysmanImp::getProductFamily() {
    return pDevice->getNEODevice()->getHardwareInfo().platform.eProductFamily;
}

FsAccess &LinuxSysmanImp::getFsAccess() {
    UNRECOVERABLE_IF(nullptr == pFsAccess);
    return *pFsAccess;
}

ProcfsAccess &LinuxSysmanImp::getProcfsAccess() {
    UNRECOVERABLE_IF(nullptr == pProcfsAccess);
    return *pProcfsAccess;
}

SysfsAccess &LinuxSysmanImp::getSysfsAccess() {
    UNRECOVERABLE_IF(nullptr == pSysfsAccess);
    return *pSysfsAccess;
}

ze_result_t LinuxSysmanImp::initLocalDeviceAndDrmHandles() {
    pDevice = Device::fromHandle(pParentSysmanDeviceImp->hCoreDevice);
    DEBUG_BREAK_IF(nullptr == pDevice);
    NEO::OSInterface &OsInterface = pDevice->getOsInterface();
    if (OsInterface.getDriverModel()->getDriverModelType() != NEO::DriverModelType::DRM) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    pDrm = OsInterface.getDriverModel()->as<NEO::Drm>();
    return ZE_RESULT_SUCCESS;
}

NEO::Drm &LinuxSysmanImp::getDrm() {
    if (pDrm == nullptr) {
        initLocalDeviceAndDrmHandles();
    }
    UNRECOVERABLE_IF(nullptr == pDrm);
    return *pDrm;
}

void LinuxSysmanImp::releaseLocalDrmHandle() {
    pDrm = nullptr;
}

Device *LinuxSysmanImp::getDeviceHandle() {
    return pDevice;
}

SysmanDeviceImp *LinuxSysmanImp::getSysmanDeviceImp() {
    return pParentSysmanDeviceImp;
}

std::string LinuxSysmanImp::getPciRootPortDirectoryPath(std::string realPciPath) {
    size_t loc;
    // we need to change the absolute path to two levels up to get
    // the Discrete card's root port.
    // the root port is always at a fixed distance as defined in HW
    uint8_t nLevel = 2;
    while (nLevel > 0) {
        loc = realPciPath.find_last_of('/');
        if (loc == std::string::npos) {
            break;
        }
        realPciPath = realPciPath.substr(0, loc);
        nLevel--;
    }
    return realPciPath;
}

static std::string modifyPathOnLevel(std::string path, uint8_t level) {
    size_t loc = 0;
    size_t count = 0;
    std::string modifiedPath(path);
    uint8_t nLevel = level;
    do {
        loc = path.find_first_of('/');
        count = count + loc;
        if (loc == std::string::npos) {
            break;
        }
        path = path.substr(loc + 1, path.size());
        nLevel--;
    } while (nLevel > 0);
    if (nLevel == 0) {
        modifiedPath = modifiedPath.substr(0, (count + level - 1)); // need to adjust for  last '/' that the code encounters
    }
    return modifiedPath;
}
std::string LinuxSysmanImp::getPciRootPortDirectoryPathForReset(std::string realPciPath) {
    // the rootport is always the first pci folder after the pcie slot.
    // /sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0
    // '/sys/devices/pci0000:89/0000:89:02.0/' will always be the same distance.
    return modifyPathOnLevel(realPciPath, 5);
}

std::string LinuxSysmanImp::getPciCardBusDirectoryPath(std::string realPciPath) {
    // the cardbus is always the second pci folder after the pcie slot.
    // /sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0
    // '/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/' will always be the same distance.
    return modifyPathOnLevel(realPciPath, 6);
}

PlatformMonitoringTech *LinuxSysmanImp::getPlatformMonitoringTechAccess(uint32_t subDeviceId) {
    auto subDeviceIdToPmtEntry = mapOfSubDeviceIdToPmtObject.find(subDeviceId);
    if (subDeviceIdToPmtEntry == mapOfSubDeviceIdToPmtObject.end()) {
        return nullptr;
    }
    return subDeviceIdToPmtEntry->second;
}

LinuxSysmanImp::LinuxSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp) {
    this->pParentSysmanDeviceImp = pParentSysmanDeviceImp;
}

void LinuxSysmanImp::releasePmtObject() {
    for (auto &subDeviceIdToPmtEntry : mapOfSubDeviceIdToPmtObject) {
        if (subDeviceIdToPmtEntry.second) {
            delete subDeviceIdToPmtEntry.second;
            subDeviceIdToPmtEntry.second = nullptr;
        }
    }
    mapOfSubDeviceIdToPmtObject.clear();
}
void LinuxSysmanImp::releaseFwUtilInterface() {
    if (nullptr != pFwUtilInterface) {
        delete pFwUtilInterface;
        pFwUtilInterface = nullptr;
    }
}

LinuxSysmanImp::~LinuxSysmanImp() {
    if (nullptr != pSysfsAccess) {
        delete pSysfsAccess;
        pSysfsAccess = nullptr;
    }
    if (nullptr != pProcfsAccess) {
        delete pProcfsAccess;
        pProcfsAccess = nullptr;
    }
    if (nullptr != pFsAccess) {
        delete pFsAccess;
        pFsAccess = nullptr;
    }
    if (nullptr != pPmuInterface) {
        delete pPmuInterface;
        pPmuInterface = nullptr;
    }
    releaseFwUtilInterface();
    releasePmtObject();
}

void LinuxSysmanImp::getPidFdsForOpenDevice(ProcfsAccess *pProcfsAccess, SysfsAccess *pSysfsAccess, const ::pid_t pid, std::vector<int> &deviceFds) {
    // Return a list of all the file descriptors of this process that point to this device
    std::vector<int> fds;
    deviceFds.clear();
    if (ZE_RESULT_SUCCESS != pProcfsAccess->getFileDescriptors(pid, fds)) {
        // Process exited. Not an error. Just ignore.
        return;
    }
    for (auto &&fd : fds) {
        std::string file;
        if (pProcfsAccess->getFileName(pid, fd, file) != ZE_RESULT_SUCCESS) {
            // Process closed this file. Not an error. Just ignore.
            continue;
        }
        if (pSysfsAccess->isMyDeviceFile(file)) {
            deviceFds.push_back(fd);
        }
    }
}

void LinuxSysmanImp::releaseSysmanDeviceResources() {
    getSysmanDeviceImp()->pEngineHandleContext->releaseEngines();
    getSysmanDeviceImp()->pRasHandleContext->releaseRasHandles();
    if (!diagnosticsReset) {
        getSysmanDeviceImp()->pDiagnosticsHandleContext->releaseDiagnosticsHandles();
    }
    getSysmanDeviceImp()->pFirmwareHandleContext->releaseFwHandles();
    releasePmtObject();
    if (!diagnosticsReset) {
        releaseFwUtilInterface();
    }
    releaseLocalDrmHandle();
}

void LinuxSysmanImp::releaseDeviceResources() {
    releaseSysmanDeviceResources();
    auto device = static_cast<DeviceImp *>(getDeviceHandle());
    executionEnvironment = device->getNEODevice()->getExecutionEnvironment();
    device->releaseResources();
    executionEnvironment->memoryManager->releaseDeviceSpecificMemResources(rootDeviceIndex);
    executionEnvironment->releaseRootDeviceEnvironmentResources(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get());
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].reset();
}

void LinuxSysmanImp::reInitSysmanDeviceResources() {
    getSysmanDeviceImp()->updateSubDeviceHandlesLocally();
    createPmtHandles();
    createFwUtilInterface();
    getSysmanDeviceImp()->pRasHandleContext->init(getSysmanDeviceImp()->deviceHandles);
    getSysmanDeviceImp()->pEngineHandleContext->init();
    if (!diagnosticsReset) {
        getSysmanDeviceImp()->pDiagnosticsHandleContext->init(getSysmanDeviceImp()->deviceHandles);
    }
    getSysmanDeviceImp()->pFirmwareHandleContext->init();
}

ze_result_t LinuxSysmanImp::initDevice() {
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto device = static_cast<DeviceImp *>(getDeviceHandle());

    auto neoDevice = NEO::DeviceFactory::createDevice(*executionEnvironment, devicePciBdf, rootDeviceIndex);
    if (neoDevice == nullptr) {
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }
    static_cast<L0::DriverHandleImp *>(device->getDriverHandle())->updateRootDeviceBitFields(neoDevice);
    static_cast<L0::DriverHandleImp *>(device->getDriverHandle())->enableRootDeviceDebugger(neoDevice);
    Device::deviceReinit(device->getDriverHandle(), device, neoDevice, &result);
    reInitSysmanDeviceResources();
    return ZE_RESULT_SUCCESS;
}

// A 'warm reset' is a conventional reset that is triggered across a PCI express link.
// A warm reset is triggered either when a link is forced into electrical idle or
// by sending TS1 and TS2 ordered sets with the hot reset bit set.
// Software can initiate a warm reset by setting and then clearing the secondary bus reset bit
// in the bridge control register in the PCI configuration space of the bridge port upstream of the device.
ze_result_t LinuxSysmanImp::osWarmReset() {
    std::string rootPortPath;
    std::string realRootPath;
    ze_result_t result = pSysfsAccess->getRealPath(deviceDir, realRootPath);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    auto device = static_cast<DeviceImp *>(pDevice);
    executionEnvironment = device->getNEODevice()->getExecutionEnvironment();
    devicePciBdf = device->getNEODevice()->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>()->getPciPath();
    rootDeviceIndex = device->getNEODevice()->getRootDeviceIndex();

    ExecutionEnvironmentRefCountRestore restorer(executionEnvironment);
    releaseDeviceResources();

    rootPortPath = getPciRootPortDirectoryPathForReset(realRootPath);

    int fd, ret = 0;
    unsigned int offset = PCI_BRIDGE_CONTROL; // Bridge control offset in Header of PCI config space
    unsigned int value = 0x00;
    unsigned int resetValue = 0x00;
    std::string configFilePath = rootPortPath + '/' + "config";
    fd = this->openFunction(configFilePath.c_str(), O_RDWR);
    if (fd < 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    this->preadFunction(fd, &value, 0x01, offset);
    resetValue = value | PCI_BRIDGE_CTL_BUS_RESET;
    this->pwriteFunction(fd, &resetValue, 0x01, offset);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Sleep for 100 milliseconds just to make sure the change is propagated.
    this->pwriteFunction(fd, &value, 0x01, offset);
    std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Sleep for 500 milliseconds
    ret = this->closeFunction(fd);
    if (ret < 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    std::string cardBusPath;
    cardBusPath = getPciCardBusDirectoryPath(realRootPath);
    // write 1 to remove
    result = pFsAccess->write(cardBusPath + '/' + "remove", "1");
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    result = pFsAccess->write(realRootPath + '/' + "rescan", "1");
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    return initDevice();
}

std::string LinuxSysmanImp::getAddressFromPath(std::string &rootPortPath) {
    size_t loc;
    loc = rootPortPath.find_last_of('/'); // we get the pci address of the root port  from rootPortPath
    return rootPortPath.substr(loc + 1, std::string::npos);
}

ze_result_t LinuxSysmanImp::osColdReset() {
    const std::string slotPath("/sys/bus/pci/slots/");                       // holds the directories matching to the number of slots in the PC
    std::string cardBusPath;                                                 // will hold the PCIe Root port directory path (the address of the PCIe slot).
    std::string realRootPath;                                                // will hold the absolute real path (not symlink) to the selected Device
    ze_result_t result = pSysfsAccess->getRealPath(deviceDir, realRootPath); // e.g realRootPath=/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    auto device = static_cast<DeviceImp *>(pDevice);
    executionEnvironment = device->getNEODevice()->getExecutionEnvironment();
    devicePciBdf = device->getNEODevice()->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>()->getPciPath();
    rootDeviceIndex = device->getNEODevice()->getRootDeviceIndex();

    ExecutionEnvironmentRefCountRestore restorer(executionEnvironment);
    releaseDeviceResources();

    cardBusPath = getPciCardBusDirectoryPath(realRootPath);    // e.g cardBusPath=/sys/devices/pci0000:89/0000:89:02.0/
    std::string rootAddress = getAddressFromPath(cardBusPath); // e.g rootAddress = 0000:8a:00.0

    std::vector<std::string> dir;
    result = pFsAccess->listDirectory(slotPath, dir); // get list of slot directories from  /sys/bus/pci/slots/
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    for (auto &slot : dir) {
        std::string slotAddress;
        result = pFsAccess->read((slotPath + slot + "/address"), slotAddress); // extract slot address from the slot directory /sys/bus/pci/slots/<slot num>/address
        if (ZE_RESULT_SUCCESS != result) {
            return result;
        }
        if (slotAddress.compare(rootAddress) == 0) {                      // compare slot address to root port address
            result = pFsAccess->write((slotPath + slot + "/power"), "0"); // turn off power
            if (ZE_RESULT_SUCCESS != result) {
                return result;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Sleep for 100 milliseconds just to make sure, 1 ms is defined as part of spec
            result = pFsAccess->write((slotPath + slot + "/power"), "1"); // turn on power
            if (ZE_RESULT_SUCCESS != result) {
                return result;
            }
            return initDevice();
        }
    }
    return ZE_RESULT_ERROR_DEVICE_LOST; // incase the reset fails inform upper layers.
}

OsSysman *OsSysman::create(SysmanDeviceImp *pParentSysmanDeviceImp) {
    LinuxSysmanImp *pLinuxSysmanImp = new LinuxSysmanImp(pParentSysmanDeviceImp);
    return static_cast<OsSysman *>(pLinuxSysmanImp);
}

} // namespace L0
