/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/helpers/sleep.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/file_descriptor.h"
#include "shared/source/os_interface/linux/pci_path.h"
#include "shared/source/os_interface/linux/pmt_util.h"
#include "shared/source/os_interface/linux/system_info.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/utilities/directory.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/sysman/source/api/pci/linux/sysman_os_pci_imp.h"
#include "level_zero/sysman/source/api/pci/sysman_pci_utils.h"
#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/source/shared/linux/kmd_interface/sysman_kmd_interface.h"
#include "level_zero/sysman/source/shared/linux/pmu/sysman_pmu.h"
#include "level_zero/sysman/source/shared/linux/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/linux/sysman_fs_access_interface.h"

namespace L0 {
namespace Sysman {

const std::string LinuxSysmanImp::deviceDir("device");

ze_result_t LinuxSysmanImp::init() {
    subDeviceCount = NEO::GfxCoreHelper::getSubDevicesCount(&pParentSysmanDeviceImp->getHardwareInfo());
    if (subDeviceCount == 1) {
        subDeviceCount = 0;
    }

    NEO::OSInterface &osInterface = *pParentSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    if (osInterface.getDriverModel()->getDriverModelType() != NEO::DriverModelType::drm) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    osInterface.getDriverModel()->cleanup();

    pSysmanProductHelper = SysmanProductHelper::create(getProductFamily());
    DEBUG_BREAK_IF(nullptr == pSysmanProductHelper);

    if (sysmanInitFromCore) {
        if (pSysmanProductHelper->isZesInitSupported()) {
            sysmanInitFromCore = false;
        } else {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                  "%s", "Sysman Initialization already happened via zeInit\n");
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }

    pSysmanKmdInterface = SysmanKmdInterface::create(*getDrm(), pSysmanProductHelper.get());
    auto result = pSysmanKmdInterface->initFsAccessInterface(*getDrm());
    if (result != ZE_RESULT_SUCCESS) {
        return result;
    }
    pSysmanKmdInterface->setSysmanDeviceDirName(getSysmanDeviceImp()->getRootDeviceEnvironment().getHardwareInfo()->capabilityTable.isIntegratedDevice);
    pFsAccess = pSysmanKmdInterface->getFsAccess();
    pProcfsAccess = pSysmanKmdInterface->getProcFsAccess();
    pSysfsAccess = pSysmanKmdInterface->getSysFsAccess();

    auto sysmanHwDeviceId = getSysmanHwDeviceIdInstance();
    int myDeviceFd = sysmanHwDeviceId.getFileDescriptor();
    rootPath = NEO::getPciRootPath(myDeviceFd).value_or("");
    pSysfsAccess->getRealPath(deviceDir, gtDevicePath);

    pPmuInterface = PmuInterface::create(this);
    return result;
}

ze_result_t LinuxSysmanImp::getResult(int err) {
    if ((EPERM == err) || (EACCES == err)) {
        return ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS;
    } else if (ENOENT == err) {
        return ZE_RESULT_ERROR_NOT_AVAILABLE;
    } else if (EBUSY == err) {
        return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
    } else {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
}

std::string &LinuxSysmanImp::getDeviceName() {
    return deviceName;
}

SysmanHwDeviceIdDrm::SingleInstance LinuxSysmanImp::getSysmanHwDeviceIdInstance() {
    UNRECOVERABLE_IF(!getDrm() || !getDrm()->getHwDeviceId());
    return static_cast<SysmanHwDeviceIdDrm *>(getDrm()->getHwDeviceId().get())->getSingleInstance();
}

NEO::Drm *LinuxSysmanImp::getDrm() {
    const auto &osInterface = *pParentSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    return osInterface.getDriverModel()->as<NEO::Drm>();
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

FsAccessInterface &LinuxSysmanImp::getFsAccess() {
    UNRECOVERABLE_IF(nullptr == pFsAccess);
    return *pFsAccess;
}

ProcFsAccessInterface &LinuxSysmanImp::getProcfsAccess() {
    UNRECOVERABLE_IF(nullptr == pProcfsAccess);
    return *pProcfsAccess;
}

SysFsAccessInterface &LinuxSysmanImp::getSysfsAccess() {
    UNRECOVERABLE_IF(nullptr == pSysfsAccess);
    return *pSysfsAccess;
}

SysmanDeviceImp *LinuxSysmanImp::getSysmanDeviceImp() {
    return pParentSysmanDeviceImp;
}

uint32_t LinuxSysmanImp::getSubDeviceCount() {
    return subDeviceCount;
}

SysmanProductHelper *LinuxSysmanImp::getSysmanProductHelper() {
    UNRECOVERABLE_IF(nullptr == pSysmanProductHelper);
    return pSysmanProductHelper.get();
}

LinuxSysmanImp::LinuxSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp) {
    this->pParentSysmanDeviceImp = pParentSysmanDeviceImp;
    executionEnvironment = pParentSysmanDeviceImp->getExecutionEnvironment();
    rootDeviceIndex = pParentSysmanDeviceImp->getRootDeviceIndex();
}

void LinuxSysmanImp::createFwUtilInterface() {
    if (isDeviceInSurvivabilityMode()) {
        pFwUtilInterface = FirmwareUtil::create(pciBdfInfo.pciDomain, pciBdfInfo.pciBus, pciBdfInfo.pciDevice, pciBdfInfo.pciFunction);
    } else {
        const auto pciBusInfo = pParentSysmanDeviceImp->getRootDeviceEnvironment().osInterface->getDriverModel()->getPciBusInfo();
        const uint16_t domain = static_cast<uint16_t>(pciBusInfo.pciDomain);
        const uint8_t bus = static_cast<uint8_t>(pciBusInfo.pciBus);
        const uint8_t device = static_cast<uint8_t>(pciBusInfo.pciDevice);
        const uint8_t function = static_cast<uint8_t>(pciBusInfo.pciFunction);

        pFwUtilInterface = FirmwareUtil::create(domain, bus, device, function);
    }
}

FirmwareUtil *LinuxSysmanImp::getFwUtilInterface() {
    const std::lock_guard<std::mutex> lock(this->fwLock);
    if (pFwUtilInterface == nullptr) {
        createFwUtilInterface();
    }
    return pFwUtilInterface;
}

void LinuxSysmanImp::releaseFwUtilInterface() {
    if (nullptr != pFwUtilInterface) {
        delete pFwUtilInterface;
        pFwUtilInterface = nullptr;
    }
}

LinuxSysmanImp::~LinuxSysmanImp() {
    if (nullptr != pPmuInterface) {
        delete pPmuInterface;
        pPmuInterface = nullptr;
    }
    releaseFwUtilInterface();
}

void LinuxSysmanImp::getPidFdsForOpenDevice(const ::pid_t pid, std::vector<int> &deviceFds) {
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
        getPidFdsForOpenDevice(pid, fds);
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
    if (!diagnosticsReset) {
        releaseFwUtilInterface();
    }
}

ze_result_t LinuxSysmanImp::reInitSysmanDeviceResources() {
    if (!diagnosticsReset) {
        if (pFwUtilInterface == nullptr) {
            createFwUtilInterface();
        }
    }
    if (getSysmanDeviceImp()->pRasHandleContext->isRasInitDone()) {
        getSysmanDeviceImp()->pRasHandleContext->init(getSubDeviceCount());
    }
    if (getSysmanDeviceImp()->pEngineHandleContext->isEngineInitDone()) {
        getSysmanDeviceImp()->pEngineHandleContext->init(getSubDeviceCount());
    }
    if (!diagnosticsReset) {
        if (getSysmanDeviceImp()->pDiagnosticsHandleContext->isDiagnosticsInitDone()) {
            getSysmanDeviceImp()->pDiagnosticsHandleContext->init();
        }
    }
    diagnosticsReset = false;
    isMemoryDiagnostics = false;
    if (getSysmanDeviceImp()->pFirmwareHandleContext->isFirmwareInitDone()) {
        getSysmanDeviceImp()->pFirmwareHandleContext->init();
    }
    if (getSysmanDeviceImp()->pMemoryHandleContext->isMemoryInitDone()) {
        getSysmanDeviceImp()->pMemoryHandleContext->init(getSubDeviceCount());
    }
    if (getSysmanDeviceImp()->pTempHandleContext->isTempInitDone()) {
        getSysmanDeviceImp()->pTempHandleContext->init(getSubDeviceCount());
    }
    if (getSysmanDeviceImp()->pPowerHandleContext->isPowerInitDone()) {
        getSysmanDeviceImp()->pPowerHandleContext->init(getSubDeviceCount());
    }
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

    std::string devicePath = {};
    if (pSysmanProductHelper->isUpstreamPortConnected()) {
        devicePath = getPciCardBusDirectoryPath(gtDevicePath);
    } else {
        devicePath = gtDevicePath;
    }

    ze_result_t result = pFsAccess->write(devicePath + '/' + "remove", "1");
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
    const std::string slotPath("/sys/bus/pci/slots/");        // holds the directories matching to the number of slots in the PC
    std::string cardBusPath;                                  // will hold the PCIe upstream port path (the address of the PCIe slot).
                                                              // will hold the absolute real path (not symlink) to the selected Device
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
    if (memType == unknownMemoryType) {
        NEO::Drm *pDrm = getDrm();
        auto hwDeviceIdInstance = getSysmanHwDeviceIdInstance();

        if (pDrm->querySystemInfo()) {
            auto memSystemInfo = getDrm()->getSystemInfo();
            if (memSystemInfo != nullptr) {
                memType = memSystemInfo->getMemoryType();
            }
        }
    }
    return memType;
}

bool LinuxSysmanImp::getTelemData(uint32_t subDeviceId, std::string &telemDir, std::string &guid, uint64_t &offset) {

    if (mapOfSubDeviceIdToTelemData.find(subDeviceId) != mapOfSubDeviceIdToTelemData.end()) {
        auto pTelemData = mapOfSubDeviceIdToTelemData[subDeviceId].get();
        telemDir = pTelemData->telemDir;
        guid = pTelemData->guid;
        offset = pTelemData->offset;
        return true;
    }

    if (telemNodesInPciPath.empty()) {
        NEO::PmtUtil::getTelemNodesInPciPath(std::string_view(rootPath), telemNodesInPciPath);
    }

    uint32_t deviceCount = getSubDeviceCount() + 1;
    if (telemNodesInPciPath.size() < deviceCount) {
        NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr, "Error@ %s(): Number of telemetry nodes:%d is less than device count: %d \n", __FUNCTION__, telemNodesInPciPath.size(), deviceCount);
        return false;
    }

    if (telemNodesInPciPath.size() == 1) {
        if (!PlatformMonitoringTech::getTelemData(telemNodesInPciPath, telemDir, guid, offset)) {
            return false;
        }
    } else {
        if (!PlatformMonitoringTech::getTelemDataForTileAggregator(telemNodesInPciPath, subDeviceId, telemDir, guid, offset)) {
            return false;
        }
    }

    pTelemData = std::make_unique<PlatformMonitoringTech::TelemData>();
    pTelemData->telemDir = telemDir;
    pTelemData->offset = offset;
    pTelemData->guid = guid;
    mapOfSubDeviceIdToTelemData[subDeviceId] = std::move(pTelemData);
    return true;
}

void LinuxSysmanImp::getDeviceUuids(std::vector<std::string> &deviceUuids) {
    constexpr uint32_t rootDeviceCount = 1;
    uint32_t totalUuidCountForDevice = this->getSubDeviceCount() + rootDeviceCount;
    deviceUuids.clear();
    for (uint32_t index = 0; index < totalUuidCountForDevice; index++) {
        std::array<uint8_t, NEO::ProductHelper::uuidSize> deviceUuid{};
        bool uuidValid = this->getUuidFromSubDeviceInfo(index, deviceUuid);
        if (uuidValid) {
            uint8_t uuid[ZE_MAX_DEVICE_UUID_SIZE] = {};
            std::copy_n(std::begin(deviceUuid), ZE_MAX_DEVICE_UUID_SIZE, std::begin(uuid));
            std::string uuidString(reinterpret_cast<char const *>(uuid));
            deviceUuids.push_back(uuidString);
        }
    }
}

bool LinuxSysmanImp::generateUuidFromPciAndSubDeviceInfo(uint32_t subDeviceID, const NEO::PhysicalDevicePciBusInfo &pciBusInfo, std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid) {
    if (pciBusInfo.pciDomain != NEO::PhysicalDevicePciBusInfo::invalidValue) {
        uuid.fill(0);

        // Device UUID uniquely identifies a device within a system.
        // We generate it based on device information along with PCI information
        // This guarantees uniqueness of UUIDs on a system even when multiple
        // identical Intel GPUs are present.

        // We want to have UUID matching between different GPU APIs (including outside
        // of compute_runtime project - i.e. other than L0 or OCL). This structure definition
        // has been agreed upon by various Intel driver teams.
        //
        // Consult other driver teams before changing this.
        //

        struct DeviceUUID {
            uint16_t vendorID;
            uint16_t deviceID;
            uint16_t revisionID;
            uint16_t pciDomain;
            uint8_t pciBus;
            uint8_t pciDev;
            uint8_t pciFunc;
            uint8_t reserved[4];
            uint8_t subDeviceID;
        };

        auto &hwInfo = getParentSysmanDeviceImp()->getHardwareInfo();
        DeviceUUID deviceUUID = {};
        deviceUUID.vendorID = 0x8086; // Intel
        deviceUUID.deviceID = hwInfo.platform.usDeviceID;
        deviceUUID.revisionID = hwInfo.platform.usRevId;
        deviceUUID.pciDomain = static_cast<uint16_t>(pciBusInfo.pciDomain);
        deviceUUID.pciBus = static_cast<uint8_t>(pciBusInfo.pciBus);
        deviceUUID.pciDev = static_cast<uint8_t>(pciBusInfo.pciDevice);
        deviceUUID.pciFunc = static_cast<uint8_t>(pciBusInfo.pciFunction);
        deviceUUID.subDeviceID = subDeviceID;

        static_assert(sizeof(DeviceUUID) == NEO::ProductHelper::uuidSize);

        memcpy_s(uuid.data(), NEO::ProductHelper::uuidSize, &deviceUUID, sizeof(DeviceUUID));

        return true;
    }
    return false;
}

bool LinuxSysmanImp::getUuidFromSubDeviceInfo(uint32_t subDeviceID, std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid) {
    auto subDeviceCount = getSubDeviceCount();
    if (uuidVec.size() == 0) {
        constexpr uint32_t rootDeviceCount = 1;
        uuidVec.resize(subDeviceCount + rootDeviceCount);
    }
    if (getParentSysmanDeviceImp()->getRootDeviceEnvironment().osInterface != nullptr) {
        auto driverModel = getParentSysmanDeviceImp()->getRootDeviceEnvironment().osInterface->getDriverModel();
        auto &gfxCoreHelper = getParentSysmanDeviceImp()->getRootDeviceEnvironment().getHelper<NEO::GfxCoreHelper>();
        auto &productHelper = getParentSysmanDeviceImp()->getRootDeviceEnvironment().getHelper<NEO::ProductHelper>();
        if (NEO::debugManager.flags.EnableChipsetUniqueUUID.get() != 0) {
            if (gfxCoreHelper.isChipsetUniqueUUIDSupported()) {
                auto hwDeviceId = getSysmanHwDeviceIdInstance();
                this->uuidVec[subDeviceID].isValid = productHelper.getUuid(driverModel, subDeviceCount, subDeviceID, this->uuidVec[subDeviceID].id);
            }
        }

        if (!this->uuidVec[subDeviceID].isValid) {
            NEO::PhysicalDevicePciBusInfo pciBusInfo = driverModel->getPciBusInfo();
            this->uuidVec[subDeviceID].isValid = generateUuidFromPciAndSubDeviceInfo(subDeviceID, pciBusInfo, this->uuidVec[subDeviceID].id);
        }

        if (this->uuidVec[subDeviceID].isValid) {
            uuid = this->uuidVec[subDeviceID].id;
        }
    }

    return this->uuidVec[subDeviceID].isValid;
}

static NEO::PhysicalDevicePciBusInfo getPciBufInfo(const char *bdfString) {
    constexpr int bdfTokensNum = 4;
    uint16_t domain = -1;
    uint8_t bus = -1, device = -1, function = -1;
    if (NEO::parseBdfString(bdfString, domain, bus, device, function) != bdfTokensNum) {
        return NEO::PhysicalDevicePciBusInfo{};
    }
    return NEO::PhysicalDevicePciBusInfo{domain, bus, device, function};
}

ze_result_t LinuxSysmanImp::initSurvivabilityMode(std::unique_ptr<NEO::HwDeviceId> hwDeviceId) {
    const auto hwDeviceIdDrm = static_cast<NEO::HwDeviceIdDrm *>(hwDeviceId.get());
    pciBdfInfo = getPciBufInfo(hwDeviceIdDrm->getPciPath());
    if (pciBdfInfo.pciDomain == pciBdfInfo.invalidValue) {
        return ZE_RESULT_ERROR_UNINITIALIZED;
    }
    return ZE_RESULT_SUCCESS;
}

bool LinuxSysmanImp::isDeviceInSurvivabilityMode() {
    return pParentSysmanDeviceImp->isDeviceInSurvivabilityMode;
}

OsSysman *OsSysman::create(SysmanDeviceImp *pParentSysmanDeviceImp) {
    LinuxSysmanImp *pLinuxSysmanImp = new LinuxSysmanImp(pParentSysmanDeviceImp);
    return static_cast<OsSysman *>(pLinuxSysmanImp);
}

} // namespace Sysman
} // namespace L0
