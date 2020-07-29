/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"

namespace L0 {

ze_result_t WddmSysmanImp::init() {
    Device *pDevice = nullptr;
    if (pParentSysmanDeviceImp != nullptr) {
        pDevice = Device::fromHandle(pParentSysmanDeviceImp->hCoreDevice);
    } else {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    UNRECOVERABLE_IF(nullptr == pDevice);

    NEO::OSInterface &OsInterface = pDevice->getOsInterface();
    pWddm = OsInterface.get()->getWddm();
    pKmdSysManager = KmdSysManager::create(pWddm);
    UNRECOVERABLE_IF(nullptr == pKmdSysManager);

    return ZE_RESULT_SUCCESS;
}

NEO::Wddm &WddmSysmanImp::getWddm() {
    UNRECOVERABLE_IF(nullptr == pWddm);
    return *pWddm;
}

KmdSysManager &WddmSysmanImp::getKmdSysManager() {
    UNRECOVERABLE_IF(nullptr == pKmdSysManager);
    return *pKmdSysManager;
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
