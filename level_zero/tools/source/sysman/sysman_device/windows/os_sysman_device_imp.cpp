/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/tools/source/sysman/sysman_device/os_sysman_device.h"
#include "level_zero/tools/source/sysman/windows/os_sysman_imp.h"

namespace L0 {

class WddmSysmanDeviceImp : public OsSysmanDevice {
  public:
    void getSerialNumber(int8_t (&serialNumber)[ZET_STRING_PROPERTY_SIZE]) override;
    void getBoardNumber(int8_t (&boardNumber)[ZET_STRING_PROPERTY_SIZE]) override;
    void getBrandName(int8_t (&brandName)[ZET_STRING_PROPERTY_SIZE]) override;
    void getModelName(int8_t (&modelName)[ZET_STRING_PROPERTY_SIZE]) override;
    void getVendorName(int8_t (&vendorName)[ZET_STRING_PROPERTY_SIZE]) override;
    void getDriverVersion(int8_t (&driverVersion)[ZET_STRING_PROPERTY_SIZE]) override;
};

void WddmSysmanDeviceImp::getSerialNumber(int8_t (&serialNumber)[ZET_STRING_PROPERTY_SIZE]) {
}

void WddmSysmanDeviceImp::getBoardNumber(int8_t (&boardNumber)[ZET_STRING_PROPERTY_SIZE]) {
}

void WddmSysmanDeviceImp::getBrandName(int8_t (&brandName)[ZET_STRING_PROPERTY_SIZE]) {
}

void WddmSysmanDeviceImp::getModelName(int8_t (&modelName)[ZET_STRING_PROPERTY_SIZE]) {
}

void WddmSysmanDeviceImp::getVendorName(int8_t (&vendorName)[ZET_STRING_PROPERTY_SIZE]) {
}

void WddmSysmanDeviceImp::getDriverVersion(int8_t (&driverVersion)[ZET_STRING_PROPERTY_SIZE]) {
}

OsSysmanDevice *OsSysmanDevice::create(OsSysman *pOsSysman) {
    WddmSysmanDeviceImp *pWddmSysmanDeviceImp = new WddmSysmanDeviceImp();
    return static_cast<OsSysmanDevice *>(pWddmSysmanDeviceImp);
}

} // namespace L0
