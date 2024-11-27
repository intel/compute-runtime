/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/shared/windows/zes_os_sysman_imp.h"

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "level_zero/core/source/driver/driver.h"
#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/source/shared/windows/pmt/sysman_pmt.h"
#include "level_zero/sysman/source/shared/windows/product_helper/sysman_product_helper.h"
#include "level_zero/sysman/source/shared/windows/sysman_kmd_sys_manager.h"

namespace L0 {
namespace Sysman {

ze_result_t WddmSysmanImp::init() {

    NEO::OSInterface &osInterface = *(pParentSysmanDeviceImp->getRootDeviceEnvironment()).osInterface;
    auto driverModel = osInterface.getDriverModel();

    if (driverModel && (driverModel->getDriverModelType() == NEO::DriverModelType::wddm)) {
        pWddm = driverModel->as<NEO::Wddm>();
    } else {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }

    pKmdSysManager = KmdSysManager::create(pWddm);
    UNRECOVERABLE_IF(nullptr == pKmdSysManager);

    subDeviceCount = NEO::GfxCoreHelper::getSubDevicesCount(&pParentSysmanDeviceImp->getHardwareInfo());
    if (subDeviceCount == 1) {
        subDeviceCount = 0;
    }

    pSysmanProductHelper = SysmanProductHelper::create(getProductFamily());
    DEBUG_BREAK_IF(nullptr == pSysmanProductHelper);

    if (sysmanInitFromCore) {
        if (pSysmanProductHelper->isZesInitSupported()) {
            sysmanInitFromCore = false;
        } else {
            NEO::printDebugString(NEO::debugManager.flags.PrintDebugMessages.get(), stderr,
                                  "%s", "Sysman Initialization already happened via zeInit\n");
            return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
        }
    }

    pPmt = PlatformMonitoringTech::create(pSysmanProductHelper.get());

    return ZE_RESULT_SUCCESS;
}

uint32_t WddmSysmanImp::getSubDeviceCount() {
    return subDeviceCount;
}

SysmanDeviceImp *WddmSysmanImp::getSysmanDeviceImp() {
    return pParentSysmanDeviceImp;
}

SysmanProductHelper *WddmSysmanImp::getSysmanProductHelper() {
    UNRECOVERABLE_IF(nullptr == pSysmanProductHelper);
    return pSysmanProductHelper.get();
}

PlatformMonitoringTech *WddmSysmanImp::getSysmanPmt() {
    return pPmt.get();
}

void WddmSysmanImp::createFwUtilInterface() {
    const auto pciBusInfo = pParentSysmanDeviceImp->getRootDeviceEnvironment().osInterface->getDriverModel()->getPciBusInfo();
    const uint16_t domain = static_cast<uint16_t>(pciBusInfo.pciDomain);
    const uint8_t bus = static_cast<uint8_t>(pciBusInfo.pciBus);
    const uint8_t device = static_cast<uint8_t>(pciBusInfo.pciDevice);
    const uint8_t function = static_cast<uint8_t>(pciBusInfo.pciFunction);
    pFwUtilInterface = FirmwareUtil::create(domain, bus, device, function);
}

FirmwareUtil *WddmSysmanImp::getFwUtilInterface() {
    if (pFwUtilInterface == nullptr) {
        createFwUtilInterface();
    }
    return pFwUtilInterface;
}

void WddmSysmanImp::releaseFwUtilInterface() {
    if (nullptr != pFwUtilInterface) {
        delete pFwUtilInterface;
        pFwUtilInterface = nullptr;
    }
}

WddmSysmanImp::WddmSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp) {
    this->pParentSysmanDeviceImp = pParentSysmanDeviceImp;
}

WddmSysmanImp::~WddmSysmanImp() {
    if (nullptr != pKmdSysManager) {
        delete pKmdSysManager;
        pKmdSysManager = nullptr;
    }
    releaseFwUtilInterface();
}

KmdSysManager &WddmSysmanImp::getKmdSysManager() {
    UNRECOVERABLE_IF(nullptr == pKmdSysManager);
    return *pKmdSysManager;
}

OsSysman *OsSysman::create(SysmanDeviceImp *pParentSysmanDeviceImp) {
    WddmSysmanImp *pWddmSysmanImp = new WddmSysmanImp(pParentSysmanDeviceImp);
    return static_cast<OsSysman *>(pWddmSysmanImp);
}

} // namespace Sysman
} // namespace L0
