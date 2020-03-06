/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman_device_imp.h"

#include "shared/source/helpers/basic_math.h"
#include "shared/source/helpers/debug_helpers.h"

#include "level_zero/core/source/device.h"

namespace L0 {

ze_result_t SysmanDeviceImp::deviceGetProperties(zet_sysman_properties_t *pProperties) {
    Device *device = L0::Device::fromHandle(hCoreDevice);
    ze_device_properties_t deviceProperties;
    device->getProperties(&deviceProperties);
    sysmanProperties.core = deviceProperties;
    uint32_t count = 0;
    device->getSubDevices(&count, nullptr);
    sysmanProperties.numSubdevices = count;

    *pProperties = sysmanProperties;
    return ZE_RESULT_SUCCESS;
}

void SysmanDeviceImp::init() {
    if (pOsSysmanDevice == nullptr) {
        pOsSysmanDevice = OsSysmanDevice::create(pOsSysman);
    }
    UNRECOVERABLE_IF(nullptr == pOsSysmanDevice);
    pOsSysmanDevice->getVendorName(sysmanProperties.vendorName);
    pOsSysmanDevice->getDriverVersion(sysmanProperties.driverVersion);
    pOsSysmanDevice->getModelName(sysmanProperties.modelName);
    pOsSysmanDevice->getBrandName(sysmanProperties.brandName);
    pOsSysmanDevice->getBoardNumber(sysmanProperties.boardNumber);
    pOsSysmanDevice->getSerialNumber(sysmanProperties.serialNumber);
}

SysmanDeviceImp::~SysmanDeviceImp() {
    if (nullptr != pOsSysmanDevice) {
        delete pOsSysmanDevice;
    }
}

} // namespace L0
