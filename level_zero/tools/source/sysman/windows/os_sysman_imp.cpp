/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"

#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "level_zero/core/source/driver/driver_handle.h"
#include "level_zero/tools/source/sysman/firmware_util/firmware_util.h"
#include "level_zero/tools/source/sysman/windows/kmd_sys_manager.h"

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

void WddmSysmanImp::createFwUtilInterface() {
    ze_pci_ext_properties_t pPciProperties;
    if (ZE_RESULT_SUCCESS != pDevice->getPciProperties(&pPciProperties)) {
        return;
    }
    uint16_t domain = static_cast<uint16_t>(pPciProperties.address.domain);
    uint8_t bus = static_cast<uint8_t>(pPciProperties.address.bus);
    uint8_t device = static_cast<uint8_t>(pPciProperties.address.device);
    uint8_t function = static_cast<uint8_t>(pPciProperties.address.function);
    pFwUtilInterface = FirmwareUtil::create(domain, bus, device, function);
}

FirmwareUtil *WddmSysmanImp::getFwUtilInterface() {
    if (pFwUtilInterface == nullptr) {
        createFwUtilInterface();
    }
    return pFwUtilInterface;
}

Device *WddmSysmanImp::getDeviceHandle() {
    return pDevice;
}
std::vector<ze_device_handle_t> &WddmSysmanImp::getDeviceHandles() {
    return pParentSysmanDeviceImp->deviceHandles;
}
ze_device_handle_t WddmSysmanImp::getCoreDeviceHandle() {
    return pParentSysmanDeviceImp->hCoreDevice;
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

void WddmSysmanImp::releaseFwUtilInterface() {
    if (nullptr != pFwUtilInterface) {
        delete pFwUtilInterface;
        pFwUtilInterface = nullptr;
    }
}

WddmSysmanImp::~WddmSysmanImp() {
    if (nullptr != pKmdSysManager) {
        delete pKmdSysManager;
        pKmdSysManager = nullptr;
    }
    releaseFwUtilInterface();
}

OsSysman *OsSysman::create(SysmanDeviceImp *pParentSysmanDeviceImp) {
    WddmSysmanImp *pWddmSysmanImp = new WddmSysmanImp(pParentSysmanDeviceImp);
    return static_cast<OsSysman *>(pWddmSysmanImp);
}

} // namespace L0
