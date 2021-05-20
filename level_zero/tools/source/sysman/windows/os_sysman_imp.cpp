/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"

#include "shared/source/os_interface/windows/wddm/wddm.h"

namespace L0 {

ze_result_t WddmSysmanImp::init() {
    pDevice = Device::fromHandle(pParentSysmanDeviceImp->hCoreDevice);
    UNRECOVERABLE_IF(nullptr == pDevice);

    NEO::OSInterface &OsInterface = pDevice->getOsInterface();
    auto driverModel = OsInterface.getDriverModel();
    if (driverModel) {
        pWddm = driverModel->as<NEO::Wddm>();
    }
    pKmdSysManager = KmdSysManager::create(pWddm);
    UNRECOVERABLE_IF(nullptr == pKmdSysManager);

    return ZE_RESULT_SUCCESS;
}

Device *WddmSysmanImp::getDeviceHandle() {
    return pDevice;
}

NEO::Wddm &WddmSysmanImp::getWddm() {
    UNRECOVERABLE_IF(nullptr == pWddm);
    return *pWddm;
}

KmdSysManager &WddmSysmanImp::getKmdSysManager() {
    UNRECOVERABLE_IF(nullptr == pKmdSysManager);
    return *pKmdSysManager;
}

WddmSysmanImp::WddmSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp) {
    this->pParentSysmanDeviceImp = pParentSysmanDeviceImp;
}

WddmSysmanImp::~WddmSysmanImp() {
    if (nullptr != pKmdSysManager) {
        delete pKmdSysManager;
        pKmdSysManager = nullptr;
    }
}

OsSysman *OsSysman::create(SysmanDeviceImp *pParentSysmanDeviceImp) {
    WddmSysmanImp *pWddmSysmanImp = new WddmSysmanImp(pParentSysmanDeviceImp);
    return static_cast<OsSysman *>(pWddmSysmanImp);
}

} // namespace L0
