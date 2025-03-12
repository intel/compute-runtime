/*
 * Copyright (C) 2023-2025 Intel Corporation
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

void WddmSysmanImp::getDeviceUuids(std::vector<std::string> &deviceUuids) {
    deviceUuids.clear();
    std::array<uint8_t, NEO::ProductHelper::uuidSize> deviceUuid{};
    bool uuidValid = this->getUuid(deviceUuid);
    if (uuidValid) {
        uint8_t uuid[ZE_MAX_DEVICE_UUID_SIZE] = {};
        std::copy_n(std::begin(deviceUuid), ZE_MAX_DEVICE_UUID_SIZE, std::begin(uuid));
        std::string uuidString(reinterpret_cast<char const *>(uuid));
        deviceUuids.push_back(uuidString);
    }
}

bool WddmSysmanImp::getUuid(std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid) {
    if (getSysmanDeviceImp()->getRootDeviceEnvironment().osInterface != nullptr) {
        auto driverModel = getSysmanDeviceImp()->getRootDeviceEnvironment().osInterface->getDriverModel();
        if (!this->uuid.isValid) {
            NEO::PhysicalDevicePciBusInfo pciBusInfo = driverModel->getPciBusInfo();
            this->uuid.isValid = generateUuidFromPciBusInfo(pciBusInfo, this->uuid.id);
        }

        if (this->uuid.isValid) {
            uuid = this->uuid.id;
        }
    }

    return this->uuid.isValid;
}

bool WddmSysmanImp::generateUuidFromPciBusInfo(const NEO::PhysicalDevicePciBusInfo &pciBusInfo, std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid) {
    if (pciBusInfo.pciDomain != NEO::PhysicalDevicePciBusInfo::invalidValue) {
        uuid.fill(0);

        // Device UUID uniquely identifies a device within a system.
        // We generate it based on device information along with PCI information
        // This guarantees uniqueness of UUIDs on a system even when multiple
        // identical Intel GPUs are present.
        //

        // We want to have UUID matching between different GPU APIs (including outside
        // of compute_runtime project - i.e. other than L0 or OCL). This structure definition
        // has been agreed upon by various Intel driver teams.
        //
        // Consult other driver teams before changing this.
        //

        struct DeviceUUID {
            uint16_t vendorID;
            uint16_t deviceID;
            uint16_t revisionID;
            uint16_t pciDomain;
            uint8_t pciBus;
            uint8_t pciDev;
            uint8_t pciFunc;
            uint8_t reserved[4];
            uint8_t subDeviceID;
        };
        auto &hwInfo = getSysmanDeviceImp()->getHardwareInfo();
        DeviceUUID deviceUUID = {};
        deviceUUID.vendorID = 0x8086; // Intel
        deviceUUID.deviceID = hwInfo.platform.usDeviceID;
        deviceUUID.revisionID = hwInfo.platform.usRevId;
        deviceUUID.pciDomain = static_cast<uint16_t>(pciBusInfo.pciDomain);
        deviceUUID.pciBus = static_cast<uint8_t>(pciBusInfo.pciBus);
        deviceUUID.pciDev = static_cast<uint8_t>(pciBusInfo.pciDevice);
        deviceUUID.pciFunc = static_cast<uint8_t>(pciBusInfo.pciFunction);
        deviceUUID.subDeviceID = 0;

        static_assert(sizeof(DeviceUUID) == NEO::ProductHelper::uuidSize);

        memcpy_s(uuid.data(), NEO::ProductHelper::uuidSize, &deviceUUID, sizeof(DeviceUUID));

        return true;
    }

    return false;
}

OsSysman *OsSysman::create(SysmanDeviceImp *pParentSysmanDeviceImp) {
    WddmSysmanImp *pWddmSysmanImp = new WddmSysmanImp(pParentSysmanDeviceImp);
    return static_cast<OsSysman *>(pWddmSysmanImp);
}

} // namespace Sysman
} // namespace L0
