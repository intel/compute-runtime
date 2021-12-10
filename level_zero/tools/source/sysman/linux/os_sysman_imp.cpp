/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

#include "level_zero/tools/source/sysman/linux/fs_access.h"

#include "sysman/linux/firmware_util/firmware_util.h"

namespace L0 {

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

OsSysman *OsSysman::create(SysmanDeviceImp *pParentSysmanDeviceImp) {
    LinuxSysmanImp *pLinuxSysmanImp = new LinuxSysmanImp(pParentSysmanDeviceImp);
    return static_cast<OsSysman *>(pLinuxSysmanImp);
}

} // namespace L0
