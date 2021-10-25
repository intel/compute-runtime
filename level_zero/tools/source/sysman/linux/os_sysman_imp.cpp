/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

#include "level_zero/tools/source/sysman/linux/fs_access.h"

namespace L0 {

ze_result_t LinuxSysmanImp::init() {
    pFsAccess = FsAccess::create();
    DEBUG_BREAK_IF(nullptr == pFsAccess);

    if (pProcfsAccess == nullptr) {
        pProcfsAccess = ProcfsAccess::create();
    }
    DEBUG_BREAK_IF(nullptr == pProcfsAccess);

    pDevice = Device::fromHandle(pParentSysmanDeviceImp->hCoreDevice);
    DEBUG_BREAK_IF(nullptr == pDevice);
    NEO::OSInterface &OsInterface = pDevice->getOsInterface();
    if (OsInterface.getDriverModel()->getDriverModelType() != NEO::DriverModelType::DRM) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    pDrm = OsInterface.getDriverModel()->as<NEO::Drm>();
    int myDeviceFd = pDrm->getFileDescriptor();
    std::string myDeviceName;
    ze_result_t result = pProcfsAccess->getFileName(pProcfsAccess->myProcessId(), myDeviceFd, myDeviceName);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    if (pSysfsAccess == nullptr) {
        pSysfsAccess = SysfsAccess::create(myDeviceName);
    }
    DEBUG_BREAK_IF(nullptr == pSysfsAccess);

    std::string realRootPath;
    result = pSysfsAccess->getRealPath("device", realRootPath);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    auto rootPciPathOfGpuDevice = getPciRootPortDirectoryPath(realRootPath);
    PlatformMonitoringTech::create(pParentSysmanDeviceImp->deviceHandles, pFsAccess, rootPciPathOfGpuDevice, mapOfSubDeviceIdToPmtObject);

    pPmuInterface = PmuInterface::create(this);

    DEBUG_BREAK_IF(nullptr == pPmuInterface);
    auto loc = realRootPath.find_last_of('/');
    std::string pciBDF = realRootPath.substr(loc + 1, std::string::npos);
    pFwUtilInterface = FirmwareUtil::create(pciBDF);

    return ZE_RESULT_SUCCESS;
}

PmuInterface *LinuxSysmanImp::getPmuInterface() {
    return pPmuInterface;
}

FirmwareUtil *LinuxSysmanImp::getFwUtilInterface() {
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

NEO::Drm &LinuxSysmanImp::getDrm() {
    UNRECOVERABLE_IF(nullptr == pDrm);
    return *pDrm;
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
    if (nullptr != pFwUtilInterface) {
        delete pFwUtilInterface;
        pFwUtilInterface = nullptr;
    }
    if (nullptr != pPmuInterface) {
        delete pPmuInterface;
        pPmuInterface = nullptr;
    }
    releasePmtObject();
}

OsSysman *OsSysman::create(SysmanDeviceImp *pParentSysmanDeviceImp) {
    LinuxSysmanImp *pLinuxSysmanImp = new LinuxSysmanImp(pParentSysmanDeviceImp);
    return static_cast<OsSysman *>(pLinuxSysmanImp);
}

} // namespace L0
