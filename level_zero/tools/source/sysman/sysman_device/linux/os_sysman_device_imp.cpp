/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/sysman_device/os_sysman_device.h"
#include "level_zero/tools/source/sysman/sysman_device/sysman_device_imp.h"

#include <csignal>

namespace L0 {

class LinuxSysmanDeviceImp : public OsSysmanDevice {
  public:
    void getSerialNumber(int8_t (&serialNumber)[ZET_STRING_PROPERTY_SIZE]) override;
    void getBoardNumber(int8_t (&boardNumber)[ZET_STRING_PROPERTY_SIZE]) override;
    void getBrandName(int8_t (&brandName)[ZET_STRING_PROPERTY_SIZE]) override;
    void getModelName(int8_t (&modelName)[ZET_STRING_PROPERTY_SIZE]) override;
    void getVendorName(int8_t (&vendorName)[ZET_STRING_PROPERTY_SIZE]) override;
    void getDriverVersion(int8_t (&driverVersion)[ZET_STRING_PROPERTY_SIZE]) override;
    ze_result_t reset() override;
    LinuxSysmanDeviceImp(OsSysman *pOsSysman);
    ~LinuxSysmanDeviceImp() override = default;

    // Don't allow copies of the LinuxSysmanDeviceImp object
    LinuxSysmanDeviceImp(const LinuxSysmanDeviceImp &obj) = delete;
    LinuxSysmanDeviceImp &operator=(const LinuxSysmanDeviceImp &obj) = delete;

  private:
    SysfsAccess *pSysfsAccess;
    LinuxSysmanImp *pLinuxSysmanImp;
    static const std::string deviceDir;
    static const std::string vendorFile;
    static const std::string deviceFile;
    static const std::string subsystemVendorFile;
    static const std::string driverFile;
    static const std::string functionLevelReset;
};

const std::string vendorIntel("Intel(R) Corporation");
const std::string unknown("Unknown");
const std::string intelPciId("0x8086");
const std::string LinuxSysmanDeviceImp::deviceDir("device");
const std::string LinuxSysmanDeviceImp::vendorFile("device/vendor");
const std::string LinuxSysmanDeviceImp::deviceFile("device/device");
const std::string LinuxSysmanDeviceImp::subsystemVendorFile("device/subsystem_vendor");
const std::string LinuxSysmanDeviceImp::driverFile("device/driver");
const std::string LinuxSysmanDeviceImp::functionLevelReset("device/reset");

void LinuxSysmanDeviceImp::getSerialNumber(int8_t (&serialNumber)[ZET_STRING_PROPERTY_SIZE]) {
    std::copy(unknown.begin(), unknown.end(), serialNumber);
    serialNumber[unknown.size()] = '\0';
}

void LinuxSysmanDeviceImp::getBoardNumber(int8_t (&boardNumber)[ZET_STRING_PROPERTY_SIZE]) {
    std::copy(unknown.begin(), unknown.end(), boardNumber);
    boardNumber[unknown.size()] = '\0';
}

void LinuxSysmanDeviceImp::getBrandName(int8_t (&brandName)[ZET_STRING_PROPERTY_SIZE]) {
    std::string strVal;
    ze_result_t result = pSysfsAccess->read(subsystemVendorFile, strVal);
    if (ZE_RESULT_SUCCESS != result) {
        std::copy(unknown.begin(), unknown.end(), brandName);
        brandName[unknown.size()] = '\0';
        return;
    }
    if (strVal.compare(intelPciId) == 0) {
        std::copy(vendorIntel.begin(), vendorIntel.end(), brandName);
        brandName[vendorIntel.size()] = '\0';
        return;
    }
    std::copy(unknown.begin(), unknown.end(), brandName);
    brandName[unknown.size()] = '\0';
}

void LinuxSysmanDeviceImp::getModelName(int8_t (&modelName)[ZET_STRING_PROPERTY_SIZE]) {
    std::string strVal;
    ze_result_t result = pSysfsAccess->read(deviceFile, strVal);
    if (ZE_RESULT_SUCCESS != result) {
        std::copy(unknown.begin(), unknown.end(), modelName);
        modelName[unknown.size()] = '\0';
        return;
    }

    std::copy(strVal.begin(), strVal.end(), modelName);
    modelName[strVal.size()] = '\0';
}

void LinuxSysmanDeviceImp::getVendorName(int8_t (&vendorName)[ZET_STRING_PROPERTY_SIZE]) {
    std::string strVal;
    ze_result_t result = pSysfsAccess->read(vendorFile, strVal);
    if (ZE_RESULT_SUCCESS != result) {
        std::copy(unknown.begin(), unknown.end(), vendorName);
        vendorName[unknown.size()] = '\0';
        return;
    }
    if (strVal.compare(intelPciId) == 0) {
        std::copy(vendorIntel.begin(), vendorIntel.end(), vendorName);
        vendorName[vendorIntel.size()] = '\0';
        return;
    }
    std::copy(unknown.begin(), unknown.end(), vendorName);
    vendorName[unknown.size()] = '\0';
}

void LinuxSysmanDeviceImp::getDriverVersion(int8_t (&driverVersion)[ZET_STRING_PROPERTY_SIZE]) {
    std::copy(unknown.begin(), unknown.end(), driverVersion);
    driverVersion[unknown.size()] = '\0';
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

ze_result_t LinuxSysmanDeviceImp::reset() {
    FsAccess *pFsAccess = &pLinuxSysmanImp->getFsAccess();
    ProcfsAccess *pProcfsAccess = &pLinuxSysmanImp->getProcfsAccess();
    SysfsAccess *pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();

    std::string resetPath;
    std::string resetName;
    ze_result_t result = ZE_RESULT_SUCCESS;

    pSysfsAccess->getRealPath(functionLevelReset, resetPath);
    // Must run as root. Verify permission to perform reset.
    result = pFsAccess->canWrite(resetPath);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    pSysfsAccess->getRealPath(deviceDir, resetName);
    resetName = pFsAccess->getBaseName(resetName);

    ::pid_t myPid = pProcfsAccess->myProcessId();
    std::vector<int> myPidFds;
    std::vector<::pid_t> processes;

    // For all processes in the system, see if the process
    // has this device open.
    result = pProcfsAccess->listProcesses(processes);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    for (auto &&pid : processes) {
        std::vector<int> fds;
        getPidFdsForOpenDevice(pProcfsAccess, pSysfsAccess, pid, fds);
        if (pid == myPid) {
            // L0 is expected to have this file open.
            // Keep list of fds. Close before unbind.
            myPidFds = fds;
        } else if (!fds.empty()) {
            // Device is in use by another process.
            // Don't reset while in use.
            return ZE_RESULT_ERROR_HANDLE_OBJECT_IN_USE;
        }
    }

    for (auto &&fd : myPidFds) {
        // Close open filedescriptors to the device
        // before unbinding device.
        // From this point forward, there is
        // graceful way to fail the reset call.
        // All future ze calls by this process for this
        // device will fail.
        ::close(fd);
    }

    // Unbind the device from the kernel driver.
    result = pSysfsAccess->unbindDevice(resetName);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    // Verify no other process has the device open.
    // This addresses the window between checking
    // file handles above, and unbinding the device.
    result = pProcfsAccess->listProcesses(processes);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    for (auto &&pid : processes) {
        std::vector<int> fds;
        getPidFdsForOpenDevice(pProcfsAccess, pSysfsAccess, pid, fds);
        if (!fds.empty()) {
            // Process is using this device after we unbound it.
            // Send a sigkill to the process to force it to close
            // the device. Otherwise, the device cannot be rebound.
            ::kill(pid, SIGKILL);
        }
    }

    // Reset the device.
    result = pFsAccess->write(resetPath, "1");
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    // Rebind the device to the kernel driver.
    result = pSysfsAccess->bindDevice(resetName);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    return ZE_RESULT_SUCCESS;
}

LinuxSysmanDeviceImp::LinuxSysmanDeviceImp(OsSysman *pOsSysman) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);

    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

OsSysmanDevice *OsSysmanDevice::create(OsSysman *pOsSysman) {
    LinuxSysmanDeviceImp *pLinuxSysmanDeviceImp = new LinuxSysmanDeviceImp(pOsSysman);
    return static_cast<OsSysmanDevice *>(pLinuxSysmanDeviceImp);
}

} // namespace L0
