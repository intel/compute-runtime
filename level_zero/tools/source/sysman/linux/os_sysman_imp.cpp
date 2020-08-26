/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

#include "level_zero/tools/source/sysman/linux/fs_access.h"

namespace L0 {

ze_result_t LinuxSysmanImp::init() {
    pXmlParser = XmlParser::create();

    pFsAccess = FsAccess::create();
    UNRECOVERABLE_IF(nullptr == pFsAccess);

    pProcfsAccess = ProcfsAccess::create();
    UNRECOVERABLE_IF(nullptr == pProcfsAccess);

    pDevice = Device::fromHandle(pParentSysmanDeviceImp->hCoreDevice);
    UNRECOVERABLE_IF(nullptr == pDevice);
    NEO::OSInterface &OsInterface = pDevice->getOsInterface();
    pDrm = OsInterface.get()->getDrm();
    int myDeviceFd = pDrm->getFileDescriptor();
    std::string myDeviceName;
    ze_result_t result = pProcfsAccess->getFileName(pProcfsAccess->myProcessId(), myDeviceFd, myDeviceName);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    pSysfsAccess = SysfsAccess::create(myDeviceName);
    UNRECOVERABLE_IF(nullptr == pSysfsAccess);

    pPmt = new PlatformMonitoringTech();
    UNRECOVERABLE_IF(nullptr == pPmt);
    pPmt->init(myDeviceName, pFsAccess);
    pPmuInterface = PmuInterface::create(this);
    UNRECOVERABLE_IF(nullptr == pPmuInterface);

    return ZE_RESULT_SUCCESS;
}

PmuInterface *LinuxSysmanImp::getPmuInterface() {
    return pPmuInterface;
}

XmlParser *LinuxSysmanImp::getXmlParser() {
    return pXmlParser;
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

PlatformMonitoringTech &LinuxSysmanImp::getPlatformMonitoringTechAccess() {
    UNRECOVERABLE_IF(nullptr == pPmt);
    return *pPmt;
}

LinuxSysmanImp::LinuxSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp) {
    this->pParentSysmanDeviceImp = pParentSysmanDeviceImp;
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
    if (nullptr != pXmlParser) {
        delete pXmlParser;
        pXmlParser = nullptr;
    }
    if (nullptr != pPmt) {
        delete pPmt;
        pPmt = nullptr;
    }
    if (nullptr != pPmuInterface) {
        delete pPmuInterface;
        pPmuInterface = nullptr;
    }
}

OsSysman *OsSysman::create(SysmanDeviceImp *pParentSysmanDeviceImp) {
    LinuxSysmanImp *pLinuxSysmanImp = new LinuxSysmanImp(pParentSysmanDeviceImp);
    return static_cast<OsSysman *>(pLinuxSysmanImp);
}

} // namespace L0
