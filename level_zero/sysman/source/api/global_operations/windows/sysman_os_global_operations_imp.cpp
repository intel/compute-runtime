/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/api/global_operations/windows/sysman_os_global_operations_imp.h"

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/os_time.h"

namespace L0 {
namespace Sysman {

bool WddmGlobalOperationsImp::getSerialNumber(char (&serialNumber)[ZES_STRING_PROPERTY_SIZE]) {
    return false;
}

bool WddmGlobalOperationsImp::getBoardNumber(char (&boardNumber)[ZES_STRING_PROPERTY_SIZE]) {
    return false;
}

void WddmGlobalOperationsImp::getBrandName(char (&brandName)[ZES_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getModelName(char (&modelName)[ZES_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getVendorName(char (&vendorName)[ZES_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) {
}

void WddmGlobalOperationsImp::getRepairStatus(zes_device_state_t *pState) {
}

void WddmGlobalOperationsImp::getTimerResolution(double *pTimerResolution) {
    *pTimerResolution = pWddmSysmanImp->getSysmanDeviceImp()->getTimerResolution();
}

bool WddmGlobalOperationsImp::getUuid(std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid) {
    if (pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironment().osInterface != nullptr) {
        auto driverModel = pWddmSysmanImp->getSysmanDeviceImp()->getRootDeviceEnvironment().osInterface->getDriverModel();
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

bool WddmGlobalOperationsImp::generateUuidFromPciBusInfo(const NEO::PhysicalDevicePciBusInfo &pciBusInfo, std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid) {
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

        auto &hwInfo = pWddmSysmanImp->getSysmanDeviceImp()->getHardwareInfo();
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

ze_bool_t WddmGlobalOperationsImp::getDeviceInfoByUuid(zes_uuid_t uuid, ze_bool_t *onSubdevice, uint32_t *subdeviceId) {
    std::array<uint8_t, NEO::ProductHelper::uuidSize> deviceUuid;
    bool uuidValid = getUuid(deviceUuid);
    if (uuidValid) {
        if (0 == std::memcmp(uuid.id, deviceUuid.data(), ZE_MAX_DEVICE_UUID_SIZE)) {
            *onSubdevice = false;
            *subdeviceId = 0;
            return true;
        }
    }
    return false;
}

ze_result_t WddmGlobalOperationsImp::getSubDeviceProperties(uint32_t *pCount, zes_subdevice_exp_properties_t *pSubdeviceProps) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmGlobalOperationsImp::reset(ze_bool_t force) {
    uint32_t value = 0;
    KmdSysman::RequestProperty request;
    KmdSysman::ResponseProperty response;
    request.commandId = KmdSysman::Command::Set;
    request.componentId = KmdSysman::Component::GlobalOperationsComponent;
    request.requestId = KmdSysman::Requests::GlobalOperation::TriggerDeviceLevelReset;
    request.dataSize = sizeof(uint32_t);
    value = static_cast<uint32_t>(force);
    memcpy_s(request.dataBuffer, sizeof(uint32_t), &value, sizeof(uint32_t));
    return pKmdSysManager->requestSingle(request, response);
}

ze_result_t WddmGlobalOperationsImp::resetExt(zes_reset_properties_t *pProperties) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmGlobalOperationsImp::scanProcessesState(std::vector<zes_process_state_t> &pProcessList) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

ze_result_t WddmGlobalOperationsImp::deviceGetState(zes_device_state_t *pState) {
    return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
}

WddmGlobalOperationsImp::WddmGlobalOperationsImp(OsSysman *pOsSysman) {
    pWddmSysmanImp = static_cast<WddmSysmanImp *>(pOsSysman);
    pKmdSysManager = &pWddmSysmanImp->getKmdSysManager();
}

OsGlobalOperations *OsGlobalOperations::create(OsSysman *pOsSysman) {
    WddmGlobalOperationsImp *pWddmGlobalOperationsImp = new WddmGlobalOperationsImp(pOsSysman);
    return static_cast<OsGlobalOperations *>(pWddmGlobalOperationsImp);
}

} // namespace Sysman
} // namespace L0
