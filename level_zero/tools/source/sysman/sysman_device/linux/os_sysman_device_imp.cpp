/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/sysman_device/linux/os_sysman_device_imp.h"

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "level_zero/core/source/device/device.h"
#include <level_zero/zet_api.h>

#include <csignal>

namespace L0 {

const std::string vendorIntel("Intel(R) Corporation");
const std::string unknown("Unknown");
const std::string intelPciId("0x8086");
const std::string LinuxSysmanDeviceImp::deviceDir("device");
const std::string LinuxSysmanDeviceImp::vendorFile("device/vendor");
const std::string LinuxSysmanDeviceImp::deviceFile("device/device");
const std::string LinuxSysmanDeviceImp::subsystemVendorFile("device/subsystem_vendor");
const std::string LinuxSysmanDeviceImp::driverFile("device/driver");
const std::string LinuxSysmanDeviceImp::functionLevelReset("device/reset");
const std::string LinuxSysmanDeviceImp::clientsDir("clients");

// Map engine entries(numeric values) present in /sys/class/drm/card<n>/clients/<client_n>/busy,
// with engine enum defined in leve-zero spec
// Note that entries with int 2 and 3(represented by i915 as CLASS_VIDEO and CLASS_VIDEO_ENHANCE)
// are both mapped to MEDIA, as CLASS_VIDEO represents any media fixed-function hardware.
const std::map<int, zet_engine_type_t> engineMap = {
    {0, ZET_ENGINE_TYPE_3D},
    {1, ZET_ENGINE_TYPE_DMA},
    {2, ZET_ENGINE_TYPE_MEDIA},
    {3, ZET_ENGINE_TYPE_MEDIA},
    {4, ZET_ENGINE_TYPE_COMPUTE}};

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

// Processes in the form of clients are present in sysfs like this:
// # /sys/class/drm/card0/clients$ ls
// 4  5
// # /sys/class/drm/card0/clients/4$ ls
// busy  name  pid
// # /sys/class/drm/card0/clients/4/busy$ ls
// 0  1  2  3
//
// Number of processes(If one process opened drm device multiple times, then multiple entries will be
// present for same process in clients directory) will be the number of clients
// (For example from above example, processes dirs are 4,5)
// Thus total number of times drm connection opened with this device will be 2.
// process.pid = pid (from above example)
// process.engines -> For each client's busy dir, numbers 0,1,2,3 represent engines and they contain
// accumulated nanoseconds each client spent on engines.
// Thus we traverse each file in busy dir for non-zero time and if we find that file say 0,then we could say that
// this engine 0 is used by process.
ze_result_t LinuxSysmanDeviceImp::scanProcessesState(std::vector<zet_process_state_t> &pProcessList) {
    std::vector<std::string> clientIds;
    ze_result_t result = pSysfsAccess->scanDirEntries(clientsDir, clientIds);
    if (ZE_RESULT_SUCCESS != result) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    // Create a map with unique pid as key and engineType as value
    std::map<uint64_t, int64_t> pidClientMap;
    for (auto clientId : clientIds) {
        // realClientPidPath will be something like: clients/<clientId>/pid
        std::string realClientPidPath = clientsDir + "/" + clientId + "/" + "pid";
        uint64_t pid;
        result = pSysfsAccess->read(realClientPidPath, pid);
        if (ZE_RESULT_SUCCESS != result) {
            if (ZE_RESULT_ERROR_NOT_AVAILABLE == result) {
                continue;
            } else {
                return result;
            }
        }

        // Traverse the clients/<clientId>/busy directory to get accelerator engines used by process
        std::vector<std::string> engineNums;
        std::string busyDirForEngines = clientsDir + "/" + clientId + "/" + "busy";
        result = pSysfsAccess->scanDirEntries(busyDirForEngines, engineNums);
        if (ZE_RESULT_SUCCESS != result) {
            if (ZE_RESULT_ERROR_NOT_AVAILABLE == result) {
                continue;
            } else {
                return result;
            }
        }
        int64_t engineType = 0;
        // Scan all engine files present in /sys/class/drm/card0/clients/<ClientId>/busy and check
        // whether that engine is used by process
        for (auto engineNum : engineNums) {
            uint64_t timeSpent = 0;
            std::string engine = busyDirForEngines + "/" + engineNum;
            result = pSysfsAccess->read(engine, timeSpent);
            if (ZE_RESULT_SUCCESS != result) {
                if (ZE_RESULT_ERROR_NOT_AVAILABLE == result) {
                    continue;
                } else {
                    return result;
                }
            }
            if (timeSpent > 0) {
                int i915EnginNumber = stoi(engineNum);
                auto i915MapToL0EngineType = engineMap.find(i915EnginNumber);
                zet_engine_type_t val = ZET_ENGINE_TYPE_OTHER;
                if (i915MapToL0EngineType != engineMap.end()) {
                    // Found a valid map
                    val = i915MapToL0EngineType->second;
                }
                // In this for loop we want to retrieve the overall engines used by process
                engineType = engineType | (1 << val);
            }
        }

        auto ret = pidClientMap.insert(std::make_pair(pid, engineType));
        if (ret.second == false) {
            // insertion failed as entry with same pid already exists in map
            // Now update the engineType field of the existing pid entry
            auto pidEntryFromMap = pidClientMap.find(pid);
            auto existingEngineType = pidEntryFromMap->second;
            engineType = existingEngineType | engineType;
            pidClientMap[pid] = engineType;
        }
    }

    // iterate through all elements of pidClientMap
    for (auto itr = pidClientMap.begin(); itr != pidClientMap.end(); ++itr) {
        zet_process_state_t process;
        process.processId = static_cast<uint32_t>(itr->first);
        process.memSize = 0; // Unsupported
        process.engines = itr->second;
        pProcessList.push_back(process);
    }

    return result;
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