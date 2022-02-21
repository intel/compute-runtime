/*
 * Copyright (C) 2021-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/string.h"
#include "shared/source/os_interface/device_factory.h"

#include "level_zero/core/source/device/device_imp.h"
#include "level_zero/tools/source/sysman/diagnostics/linux/os_diagnostics_imp.h"

#include <linux/pci_regs.h>
namespace L0 {
//All memory mappings where LMEMBAR is being referenced are invalidated.
//Also prevents new ones from being created.
//It will invalidate LMEM memory mappings only when sysfs entry quiesce_gpu is set.

//the sysfs node will be at /sys/class/drm/card<n>/invalidate_lmem_mmaps
const std::string LinuxDiagnosticsImp::invalidateLmemFile("invalidate_lmem_mmaps");
// the sysfs node will be at /sys/class/drm/card<n>/quiesce_gpu
const std::string LinuxDiagnosticsImp::quiescentGpuFile("quiesce_gpu");
void OsDiagnostics::getSupportedDiagTestsFromFW(void *pOsSysman, std::vector<std::string> &supportedDiagTests) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);
    if (IGFX_PVC == pLinuxSysmanImp->getProductFamily()) {
        FirmwareUtil *pFwInterface = pLinuxSysmanImp->getFwUtilInterface();
        if (pFwInterface != nullptr) {
            if (ZE_RESULT_SUCCESS == static_cast<FirmwareUtil *>(pFwInterface)->fwDeviceInit()) {
                static_cast<FirmwareUtil *>(pFwInterface)->fwSupportedDiagTests(supportedDiagTests);
            }
        }
    }
}

void LinuxDiagnosticsImp::releaseSysmanDeviceResources() {
    pLinuxSysmanImp->getSysmanDeviceImp()->pEngineHandleContext->releaseEngines();
    pLinuxSysmanImp->getSysmanDeviceImp()->pRasHandleContext->releaseRasHandles();
    pLinuxSysmanImp->releasePmtObject();
    pLinuxSysmanImp->releaseLocalDrmHandle();
}

void LinuxDiagnosticsImp::releaseDeviceResources() {
    releaseSysmanDeviceResources();
    auto device = static_cast<DeviceImp *>(pLinuxSysmanImp->getDeviceHandle());
    device->releaseResources();
    executionEnvironment->memoryManager->releaseDeviceSpecificMemResources(rootDeviceIndex);
    executionEnvironment->releaseRootDeviceEnvironmentResources(executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].get());
    executionEnvironment->rootDeviceEnvironments[rootDeviceIndex].reset();
}

void LinuxDiagnosticsImp::reInitSysmanDeviceResources() {
    pLinuxSysmanImp->getSysmanDeviceImp()->updateSubDeviceHandlesLocally();
    pLinuxSysmanImp->createPmtHandles();
    pLinuxSysmanImp->getSysmanDeviceImp()->pRasHandleContext->init(pLinuxSysmanImp->getSysmanDeviceImp()->deviceHandles);
    pLinuxSysmanImp->getSysmanDeviceImp()->pEngineHandleContext->init();
}

ze_result_t LinuxDiagnosticsImp::initDevice() {
    ze_result_t result = ZE_RESULT_SUCCESS;
    auto device = static_cast<DeviceImp *>(pLinuxSysmanImp->getDeviceHandle());

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

static void getPidFdsForOpenDevice(ProcfsAccess *pProcfsAccess, SysfsAccess *pSysfsAccess, const ::pid_t pid, std::vector<int> &deviceFds) {
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
// A 'warm reset' is a conventional reset that is triggered across a PCI express link.
// A warm reset is triggered either when a link is forced into electrical idle or
// by sending TS1 and TS2 ordered sets with the hot reset bit set.
// Software can initiate a warm reset by setting and then clearing the secondary bus reset bit
// in the bridge control register in the PCI configuration space of the bridge port upstream of the device.
ze_result_t LinuxDiagnosticsImp::osWarmReset() {
    std::string rootPortPath;
    std::string realRootPath;
    ze_result_t result = pSysfsAccess->getRealPath(deviceDir, realRootPath);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    auto device = static_cast<DeviceImp *>(pDevice);
    executionEnvironment = device->getNEODevice()->getExecutionEnvironment();

    ExecutionEnvironmentRefCountRestore restorer(executionEnvironment);
    releaseDeviceResources();
    // write 1 to remove
    result = pFsAccess->write(realRootPath + '/' + "remove", "1");
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    size_t loc;

    loc = realRootPath.find_last_of('/');
    realRootPath = realRootPath.substr(0, loc);

    int fd, ret = 0;
    unsigned int offset = PCI_BRIDGE_CONTROL; // Bridge control offset in Header of PCI config space
    unsigned int value = 0x00;
    unsigned int resetValue = 0x00;
    std::string configFilePath = realRootPath + '/' + "config";
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

    result = pFsAccess->write(realRootPath + '/' + "rescan", "1");
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    return initDevice();
}

std::string getRootPortaddress(std::string &rootPortPath) {
    size_t loc;
    loc = rootPortPath.find_last_of('/'); // we get the pci address of the root port  from rootPortPath
    return rootPortPath.substr(loc + 1, std::string::npos);
}

ze_result_t LinuxDiagnosticsImp::osColdReset() {
    const std::string slotPath("/sys/bus/pci/slots/");                       // holds the directories matching to the number of slots in the PC
    std::string rootPortPath;                                                // will hold the PCIe Root port directory path (the address of the PCIe slot).
    std::string realRootPath;                                                // will hold the absolute real path (not symlink) to the selected Device
    ze_result_t result = pSysfsAccess->getRealPath(deviceDir, realRootPath); // e.g realRootPath=/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    auto device = static_cast<DeviceImp *>(pDevice);
    executionEnvironment = device->getNEODevice()->getExecutionEnvironment();

    ExecutionEnvironmentRefCountRestore restorer(executionEnvironment);
    releaseDeviceResources();

    rootPortPath = pLinuxSysmanImp->getPciRootPortDirectoryPath(realRootPath); // e.g rootPortPath=/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0
    std::string rootAddress = getRootPortaddress(rootPortPath);                // e.g rootAddress = 0000:8a:00.0

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

ze_result_t LinuxDiagnosticsImp::osRunDiagTestsinFW(zes_diag_result_t *pResult) {
    const int intVal = 1;
    // before running diagnostics need to close all active workloads
    // writing 1 to /sys/class/drm/card<n>/quiesce_gpu will signal KMD
    //GPU (every gt in the card) will be wedged.
    // GPU will only be unwedged after warm/cold reset
    ::pid_t myPid = pProcfsAccess->myProcessId();
    std::vector<::pid_t> processes;
    ze_result_t result = pProcfsAccess->listProcesses(processes);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    for (auto &&pid : processes) {
        std::vector<int> fds;
        getPidFdsForOpenDevice(pProcfsAccess, pSysfsAccess, pid, fds);
        if (pid == myPid) {
            // L0 is expected to have this file open.
            // Keep list of fds. Close before unbind.
            continue;
        }
        if (!fds.empty()) {
            pProcfsAccess->kill(pid);
        }
    }
    result = pSysfsAccess->write(quiescentGpuFile, intVal);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    result = pSysfsAccess->write(invalidateLmemFile, intVal);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    pFwInterface->fwRunDiagTests(osDiagType, pResult);
    if (*pResult == ZES_DIAG_RESULT_REBOOT_FOR_REPAIR) {
        return osColdReset();
    }
    return osWarmReset(); // we need to at least do a Warm reset to bring the machine out of wedged state
}

} // namespace L0
