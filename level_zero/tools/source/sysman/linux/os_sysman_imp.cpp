/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/linux/os_sysman_imp.h"

#include "sysman/linux/sysfs_access.h"

namespace L0 {

ze_result_t LinuxSysmanImp::init() {
    Device *pDevice = Device::fromHandle(pParentSysmanImp->hCoreDevice);
    NEO::OSInterface &OsInterface = pDevice->getOsInterface();
    NEO::Drm *pDrm = OsInterface.get()->getDrm();
    int fd = pDrm->getFileDescriptor();

    pSysfsAccess = SysfsAccess::create(fd);
    UNRECOVERABLE_IF(nullptr == pSysfsAccess);
    return ZE_RESULT_SUCCESS;
}

SysfsAccess &LinuxSysmanImp::getSysfsAccess() {
    UNRECOVERABLE_IF(nullptr == pSysfsAccess);
    return *pSysfsAccess;
}

ze_result_t LinuxSysmanImp::systemCmd(const std::string cmd, std::string &output) {
    std::array<char, ZET_STRING_PROPERTY_SIZE> temp;
    FILE *fd = popen(cmd.c_str(), "r");
    if (!fd) {
        return ZE_RESULT_ERROR_UNKNOWN;
    }
    while (fgets(temp.data(), ZET_STRING_PROPERTY_SIZE, fd) != NULL) {
        output += temp.data();
    }
    pclose(fd);
    return ZE_RESULT_SUCCESS;
}

LinuxSysmanImp::~LinuxSysmanImp() {
    if (nullptr != pSysfsAccess) {
        delete pSysfsAccess;
    }
}

OsSysman *OsSysman::create(SysmanImp *pParentSysmanImp) {
    LinuxSysmanImp *pLinuxSysmanImp = new LinuxSysmanImp(pParentSysmanImp);
    return static_cast<OsSysman *>(pLinuxSysmanImp);
}

} // namespace L0
