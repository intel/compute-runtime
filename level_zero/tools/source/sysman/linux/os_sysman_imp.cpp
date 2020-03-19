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
    pFsAccess = FsAccess::create();
    UNRECOVERABLE_IF(nullptr == pFsAccess);

    pProcfsAccess = ProcfsAccess::create();
    UNRECOVERABLE_IF(nullptr == pProcfsAccess);

    Device *pDevice = Device::fromHandle(pParentSysmanImp->hCoreDevice);
    NEO::OSInterface &OsInterface = pDevice->getOsInterface();
    NEO::Drm *pDrm = OsInterface.get()->getDrm();
    int myDeviceFd = pDrm->getFileDescriptor();
    std::string myDeviceName;
    ze_result_t result = pProcfsAccess->getFileName(pProcfsAccess->myProcessId(), myDeviceFd, myDeviceName);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    pSysfsAccess = SysfsAccess::create(myDeviceName);
    UNRECOVERABLE_IF(nullptr == pSysfsAccess);

    return ZE_RESULT_SUCCESS;
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

LinuxSysmanImp::LinuxSysmanImp(SysmanImp *pParentSysmanImp) {
    this->pParentSysmanImp = pParentSysmanImp;
}

LinuxSysmanImp::~LinuxSysmanImp() {
    if (nullptr != pSysfsAccess) {
        delete pSysfsAccess;
    }
    if (nullptr != pProcfsAccess) {
        delete pProcfsAccess;
    }
    if (nullptr != pFsAccess) {
        delete pFsAccess;
    }
}

OsSysman *OsSysman::create(SysmanImp *pParentSysmanImp) {
    LinuxSysmanImp *pLinuxSysmanImp = new LinuxSysmanImp(pParentSysmanImp);
    return static_cast<OsSysman *>(pLinuxSysmanImp);
}

} // namespace L0
