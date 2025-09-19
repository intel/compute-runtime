/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/execution_environment/root_device_environment.h"
#include "shared/source/helpers/sleep.h"
#include "shared/source/memory_manager/memory_manager.h"
#include "shared/source/os_interface/device_factory.h"
#include "shared/source/os_interface/linux/file_descriptor.h"
#include "shared/source/os_interface/linux/system_info.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/core/source/driver/driver_handle_imp.h"
#include "level_zero/tools/source/sysman/firmware_util/firmware_util.h"
#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/pci/linux/os_pci_imp.h"
#include "level_zero/tools/source/sysman/pci/pci_utils.h"

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

    std::string prelimVersion{};
    pDrm->getPrelimVersion(prelimVersion);
    if (prelimVersion != "") {
        isUsingPrelimEnabledKmd = true;
    }

    if (pSysfsAccess == nullptr) {
        pSysfsAccess = SysfsAccess::create(myDeviceName);
    }
    DEBUG_BREAK_IF(nullptr == pSysfsAccess);

    pPmuInterface = PmuInterface::create(this);

    DEBUG_BREAK_IF(nullptr == pPmuInterface);

    getMemoryType();
    return createPmtHandles();
}

void LinuxSysmanImp::createFwUtilInterface() {
    ze_pci_ext_properties_t pPciProperties;
    if (ZE_RESULT_SUCCESS != pDevice->getPciProperties(&pPciProperties)) {
        return;
    }
    uint16_t domain = static_cast<uint16_t>(pPciProperties.address.domain);
    uint8_t bus = static_cast<uint8_t>(pPciProperties.address.bus);
    uint8_t device = static_cast<uint8_t>(pPciProperties.address.device);
    uint8_t function = static_cast<uint8_t>(pPciProperties.address.function);
    pFwUtilInterface = FirmwareUtil::create(domain, bus, device, function);
}

ze_result_t LinuxSysmanImp::createPmtHandles() {
    std::string gtDevicePCIPath;
    auto result = pSysfsAccess->getRealPath("device", gtDevicePCIPath);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    auto gpuUpstreamPortPath = getPciCardBusDirectoryPath(gtDevicePCIPath);
    PlatformMonitoringTech::create(pParentSysmanDeviceImp->deviceHandles, pFsAccess, gpuUpstreamPortPath, mapOfSubDeviceIdToPmtObject);
    return result;
}

PmuInterface *LinuxSysmanImp::getPmuInterface() {
    return pPmuInterface;
}

FirmwareUtil *LinuxSysmanImp::getFwUtilInterface() {
    const std::lock_guard<std::mutex> lock(this->fwLock);
    if (pFwUtilInterface == nullptr) {
        createFwUtilInterface();
    }
    return pFwUtilInterface;
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
    NEO::OSInterface &osInterface = *pDevice->getOsInterface();
    if (osInterface.getDriverModel()->getDriverModelType() != NEO::DriverModelType::drm) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    pDrm = osInterface.getDriverModel()->as<NEO::Drm>();
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

ze_bool_t LinuxSysmanImp::isDriverModelSupported() {
    auto drmVersion = getDrm().getDrmVersion(getDrm().getFileDescriptor());
    if ("i915" == drmVersion) {
        return true;
    } else {
        return false;
    }
}

static std::string modifyPathOnLevel(std::string realPciPath, uint8_t nLevel) {
    size_t loc;
    // we need to change the absolute path to 'nLevel' levels up
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
std::string LinuxSysmanImp::getPciRootPortDirectoryPath(std::string realPciPath) {
    // the rootport is always the first pci folder after the pcie slot.
    //    +-[0000:89]-+-00.0
    // |           +-00.1
    // |           +-00.2
    // |           +-00.4
    // |           \-02.0-[8a-8e]----00.0-[8b-8e]--+-01.0-[8c-8d]----00.0
    // |                                           \-02.0-[8e]--+-00.0
    // |                                                        +-00.1
    // |                                                        \-00.2
    // /sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0
    // '/sys/devices/pci0000:89/0000:89:02.0/' will always be the same distance.
    // from 0000:8c:00.0 i.e the 3rd PCI address from the gt tile
    return modifyPathOnLevel(realPciPath, 3);
}

std::string LinuxSysmanImp::getPciCardBusDirectoryPath(std::string realPciPath) {
    // the cardbus is always the second pci folder after the pcie slot.
    //    +-[0000:89]-+-00.0
    // |           +-00.1
    // |           +-00.2
    // |           +-00.4
    // |           \-02.0-[8a-8e]----00.0-[8b-8e]--+-01.0-[8c-8d]----00.0
    // |                                           \-02.0-[8e]--+-00.0
    // |                                                        +-00.1
    // |                                                        \-00.2
    // /sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0
    // '/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/' will always be the same distance.
    // from 0000:8c:00.0 i.e the 2nd PCI address from the gt tile.
    return modifyPathOnLevel(realPciPath, 2);
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

ze_result_t LinuxSysmanImp::gpuProcessCleanup(ze_bool_t force) {
    ::pid_t myPid = pProcfsAccess->myProcessId();
    std::vector<::pid_t> processes;
    std::vector<int> myPidFds;
    ze_result_t result = pProcfsAccess->listProcesses(processes);
    if (ZE_RESULT_SUCCESS != result) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                              "gpuProcessCleanup: listProcesses() failed with error code: %ld\n", result);
        return result;
    }

    for (auto &&pid : processes) {
        std::vector<int> fds;
        getPidFdsForOpenDevice(pProcfsAccess, pSysfsAccess, pid, fds);
        if (pid == myPid) {
            // L0 is expected to have this file open.
            // Keep list of fds. Close before unbind.
            myPidFds = fds;
            continue;
        }
        if (!fds.empty()) {
            if (force) {
                pProcfsAccess->kill(pid);
            } else {
                NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Device in use by another process, returning error:0x%x \n", __FUNCTION__, ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE);
                return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
            }
        }
    }

    for (auto &&fd : myPidFds) {
        // Close open filedescriptors to the device
        // before unbinding device.
        // From this point forward, there is no
        // graceful way to fail the reset call.
        // All future ze calls by this process for this
        // device will fail.
        NEO::SysCalls::close(fd);
    }
    return ZE_RESULT_SUCCESS;
}

void LinuxSysmanImp::releaseSysmanDeviceResources() {
    getSysmanDeviceImp()->pEngineHandleContext->releaseEngines();
    getSysmanDeviceImp()->pRasHandleContext->releaseRasHandles();
    getSysmanDeviceImp()->pMemoryHandleContext->releaseMemoryHandles();
    getSysmanDeviceImp()->pTempHandleContext->releaseTemperatureHandles();
    getSysmanDeviceImp()->pPowerHandleContext->releasePowerHandles();
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
    auto devicePtr = static_cast<DeviceImp *>(pDevice);
    executionEnvironment = devicePtr->getNEODevice()->getExecutionEnvironment();
    devicePciBdf = devicePtr->getNEODevice()->getRootDeviceEnvironment().osInterface->getDriverModel()->as<NEO::Drm>()->getPciPath();
    rootDeviceIndex = devicePtr->getNEODevice()->getRootDeviceIndex();
    pSysfsAccess->getRealPath(deviceDir, gtDevicePath);
    releaseSysmanDeviceResources();
    auto device = static_cast<DeviceImp *>(getDeviceHandle());
    executionEnvironment = device->getNEODevice()->getExecutionEnvironment();
    device->releaseResources();
    executionEnvironment->memoryManager->releaseDeviceSpecificMemResources(rootDeviceIndex);
    executionEnvironment->releaseRootDeviceEnvironmentResources(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get());
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].reset();
    executionEnvironment->memoryManager->releaseDeviceSpecificGfxPartition(rootDeviceIndex);
}

void LinuxSysmanImp::reInitSysmanDeviceResources() {
    getSysmanDeviceImp()->updateSubDeviceHandlesLocally();
    createPmtHandles();
    if (!diagnosticsReset) {
        createFwUtilInterface();
    }
    if (getSysmanDeviceImp()->pRasHandleContext->isRasInitDone()) {
        getSysmanDeviceImp()->pRasHandleContext->init(getSysmanDeviceImp()->deviceHandles);
    }
    if (getSysmanDeviceImp()->pEngineHandleContext->isEngineInitDone()) {
        getSysmanDeviceImp()->pEngineHandleContext->init(getSysmanDeviceImp()->deviceHandles);
    }
    if (!diagnosticsReset) {
        if (getSysmanDeviceImp()->pDiagnosticsHandleContext->isDiagnosticsInitDone()) {
            getSysmanDeviceImp()->pDiagnosticsHandleContext->init();
        }
    }
    this->diagnosticsReset = false;
    isMemoryDiagnostics = false;
    if (getSysmanDeviceImp()->pFirmwareHandleContext->isFirmwareInitDone()) {
        getSysmanDeviceImp()->pFirmwareHandleContext->init();
    }
    if (getSysmanDeviceImp()->pMemoryHandleContext->isMemoryInitDone()) {
        getSysmanDeviceImp()->pMemoryHandleContext->init(getSysmanDeviceImp()->deviceHandles);
    }
    if (getSysmanDeviceImp()->pTempHandleContext->isTempInitDone()) {
        getSysmanDeviceImp()->pTempHandleContext->init(getSysmanDeviceImp()->deviceHandles);
    }
    if (getSysmanDeviceImp()->pPowerHandleContext->isPowerInitDone()) {
        getSysmanDeviceImp()->pPowerHandleContext->init(getSysmanDeviceImp()->deviceHandles, getCoreDeviceHandle());
    }
}

ze_result_t LinuxSysmanImp::initDevice() {
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto device = static_cast<DeviceImp *>(getDeviceHandle());

    auto neoDevice = NEO::DeviceFactory::createDevice(*executionEnvironment, devicePciBdf, rootDeviceIndex);
    if (neoDevice == nullptr) {
        return ZE_RESULT_ERROR_DEVICE_LOST;
    }
    static_cast<L0::DriverHandleImp *>(device->getDriverHandle())->updateRootDeviceBitFields(neoDevice);
    Device::deviceReinit(device->getDriverHandle(), device, neoDevice, &result);
    reInitSysmanDeviceResources();

    return ZE_RESULT_SUCCESS;
}

// function to clear Hot-Plug interrupt enable bit in the slot control register
// this is required to prevent interrupts from being raised in the warm reset path.
void LinuxSysmanImp::clearHPIE(int fd) {
    uint8_t value = 0x00;
    uint8_t resetValue = 0x00;
    uint8_t offset = 0x0;
    this->preadFunction(fd, &offset, 0x01, PCI_CAPABILITY_LIST);
    // Bottom two bits of capability pointer register are reserved and
    // software should mask these bits to get pointer to capability list.
    // PCI_EXP_SLTCTL - offset for slot control register.
    offset = (offset & 0xfc) + PCI_EXP_SLTCTL;
    this->preadFunction(fd, &value, 0x01, offset);
    resetValue = value & (~PCI_EXP_SLTCTL_HPIE);
    this->pwriteFunction(fd, &resetValue, 0x01, offset);
    NEO::sleep(std::chrono::seconds(10)); // Sleep for 10seconds just to make sure the change is propagated.
}

// Function to adjust VF BAR size i.e Modify VF BAR Control register.
// size param is an encoded value described as follows:
// 0  - 1 MB (2^20 bytes)
// 1  - 2 MB (2^21 bytes)
// 2  - 4 MB (2^22 bytes)
// 3  - 8 MB (2^23 bytes)
// .
// .
// .
// b  - 2 GB (2^31 bytes)
// 43 - 8 EB (2^63 bytes)
ze_result_t LinuxSysmanImp::resizeVfBar(uint8_t size) {
    std::string pciConfigNode;
    pciConfigNode = gtDevicePath + "/config";

    auto fdConfig = NEO::FileDescriptor(pciConfigNode.c_str(), O_RDWR);
    if (fdConfig < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stdout,
                              "Config node open failed\n");
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    std::unique_ptr<uint8_t[]> configMemory = std::make_unique<uint8_t[]>(PCI_CFG_SPACE_EXP_SIZE);
    memset(configMemory.get(), 0, PCI_CFG_SPACE_EXP_SIZE);
    if (this->preadFunction(fdConfig, configMemory.get(), PCI_CFG_SPACE_EXP_SIZE, 0) < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stdout,
                              "Read to get config space failed\n");
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    auto reBarCapPos = L0::LinuxPciImp::getRebarCapabilityPos(configMemory.get(), true);
    if (!reBarCapPos) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stdout,
                              "VF BAR capability not found\n");
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    auto barSizePos = reBarCapPos + PCI_REBAR_CTRL + 1; // position of VF(0) BAR SIZE.
    if (this->pwriteFunction(fdConfig, &size, 0x01, barSizePos) < 0) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stdout,
                              "Write to change VF bar size failed\n");
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    return ZE_RESULT_SUCCESS;
}

// A 'warm reset' is a conventional reset that is triggered across a PCI express link.
// A warm reset is triggered either when a link is forced into electrical idle or
// by sending TS1 and TS2 ordered sets with the hot reset bit set.
// Software can initiate a warm reset by setting and then clearing the secondary bus reset bit
// in the bridge control register in the PCI configuration space of the bridge port upstream of the device.
ze_result_t LinuxSysmanImp::osWarmReset() {
    std::string rootPortPath;
    rootPortPath = getPciRootPortDirectoryPath(gtDevicePath);

    std::string configFilePath = rootPortPath + '/' + "config";
    auto fd = NEO::FileDescriptor(configFilePath.c_str(), O_RDWR);
    if (fd < 0) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }

    std::string cardBusPath = getPciCardBusDirectoryPath(gtDevicePath);
    ze_result_t result = pFsAccess->write(cardBusPath + '/' + "remove", "1");
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    NEO::sleep(std::chrono::seconds(10)); // Sleep for 10seconds to make sure that the config spaces of all devices are saved correctly.

    clearHPIE(fd);

    uint8_t offset = PCI_BRIDGE_CONTROL; // Bridge control offset in Header of PCI config space
    uint8_t value = 0x00;
    uint8_t resetValue = 0x00;

    this->preadFunction(fd, &value, 0x01, offset);
    resetValue = value | PCI_BRIDGE_CTL_BUS_RESET;
    this->pwriteFunction(fd, &resetValue, 0x01, offset);
    NEO::sleep(std::chrono::seconds(10)); // Sleep for 10seconds just to make sure the change is propagated.
    this->pwriteFunction(fd, &value, 0x01, offset);

    if (isMemoryDiagnostics) {
        int32_t delayDurationForPPR = 6; // Sleep for 6 minutes to allow PPR to complete.
        if (NEO::debugManager.flags.DebugSetMemoryDiagnosticsDelay.get() != -1) {
            delayDurationForPPR = NEO::debugManager.flags.DebugSetMemoryDiagnosticsDelay.get();
        }
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stdout,
                              "Delay of %d mins introduced to allow HBM IFR to complete\n", delayDurationForPPR);
        NEO::sleep(std::chrono::seconds(delayDurationForPPR * 60));
    } else {
        NEO::sleep(std::chrono::seconds(10)); // Sleep for 10 seconds to make sure writing to bridge control offset is propagated.
    }

    result = pFsAccess->write(rootPortPath + '/' + "rescan", "1");
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    NEO::sleep(std::chrono::seconds(10)); // Sleep for 10seconds, allows the rescan to complete on all devices attached to the root port.

    // PCIe port driver uses the BIOS allocated VF bars on bootup. A known bug exists in pcie port driver
    // and is causing VF bar allocation failure in PCIe port driver after an SBR - https://bugzilla.kernel.org/show_bug.cgi?id=216795

    // WA to adjust VF bar size to 2GB. The default VF bar size is 8GB and for 63VFs, 504GB need to be allocated which is failing on SBR.
    // When configured VF bar size to 2GB, an allocation of 126GB is successful. This WA resizes VF0 bar to 2GB. Once pcie port driver
    // issue is resolved, this WA may not be necessary. Description for 0xb is explained at function definition - resizeVfVar.
    if (NEO::debugManager.flags.VfBarResourceAllocationWa.get()) {
        if (ZE_RESULT_SUCCESS != (result = resizeVfBar(0xb))) {
            return result;
        }

        result = pFsAccess->write(cardBusPath + '/' + "remove", "1");
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stdout,
                                  "Card Bus remove after resizing VF bar failed\n");
            return result;
        }
        NEO::sleep(std::chrono::seconds(10)); // Sleep for 10seconds to make sure that the config spaces of all devices are saved correctly.

        result = pFsAccess->write(rootPortPath + '/' + "rescan", "1");
        if (ZE_RESULT_SUCCESS != result) {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stdout,
                                  "Rescanning root port failed after resizing VF bar failed\n");
            return result;
        }
        NEO::sleep(std::chrono::seconds(10)); // Sleep for 10seconds, allows the rescan to complete on all devices attached to the root port.
    }
    return result;
}

std::string LinuxSysmanImp::getAddressFromPath(std::string &cardBusPath) {
    size_t loc;
    loc = cardBusPath.find_last_of('/'); // we get the pci address of the upstream port from card bus Path
    auto uspAddress = cardBusPath.substr(loc + 1, std::string::npos);
    loc = uspAddress.find_last_of('.'); // we remove the function number from the pci address
    return uspAddress.substr(0, loc);
}

ze_result_t LinuxSysmanImp::osColdReset() {
    const std::string slotPath("/sys/bus/pci/slots/"); // holds the directories matching to the number of slots in the PC
    std::string cardBusPath;                           // will hold the PCIe Root port directory path (the address of the PCIe slot).                                           // will hold the absolute real path (not symlink) to the selected Device

    cardBusPath = getPciCardBusDirectoryPath(gtDevicePath);   // e.g cardBusPath=/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0
    std::string uspAddress = getAddressFromPath(cardBusPath); // e.g upstreamPortAddress = 0000:8a:00

    std::vector<std::string> dir;
    ze_result_t result = pFsAccess->listDirectory(slotPath, dir); // get list of slot directories from  /sys/bus/pci/slots/
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    for (auto &slot : dir) {
        std::string slotAddress;
        result = pFsAccess->read((slotPath + slot + "/address"), slotAddress); // extract slot address from the slot directory /sys/bus/pci/slots/<slot num>/address
        if (ZE_RESULT_SUCCESS != result) {
            return result;
        }
        if (slotAddress.compare(uspAddress) == 0) {                       // compare slot address to upstream port address
            result = pFsAccess->write((slotPath + slot + "/power"), "0"); // turn off power
            if (ZE_RESULT_SUCCESS != result) {
                return result;
            }
            NEO::sleep(std::chrono::milliseconds(100));                   // Sleep for 100 milliseconds just to make sure, 1 ms is defined as part of spec
            result = pFsAccess->write((slotPath + slot + "/power"), "1"); // turn on power
            if (ZE_RESULT_SUCCESS != result) {
                return result;
            }
            return ZE_RESULT_SUCCESS;
        }
    }
    return ZE_RESULT_ERROR_DEVICE_LOST; // in case the reset fails inform upper layers.
}

uint32_t LinuxSysmanImp::getMemoryType() {
    if (isMemTypeRetrieved == false) {
        auto pDrm = &getDrm();
        if (pDrm->querySystemInfo()) {
            auto memSystemInfo = pDrm->getSystemInfo();
            if (memSystemInfo != nullptr) {
                memType = memSystemInfo->getMemoryType();
                isMemTypeRetrieved = true;
            }
        }
    }
    return memType;
}

OsSysman *OsSysman::create(SysmanDeviceImp *pParentSysmanDeviceImp) {
    LinuxSysmanImp *pLinuxSysmanImp = new LinuxSysmanImp(pParentSysmanDeviceImp);
    return static_cast<OsSysman *>(pLinuxSysmanImp);
}
std::vector<ze_device_handle_t> &LinuxSysmanImp::getDeviceHandles() {
    return pParentSysmanDeviceImp->deviceHandles;
}
ze_device_handle_t LinuxSysmanImp::getCoreDeviceHandle() {
    return pParentSysmanDeviceImp->hCoreDevice;
}

} // namespace L0
