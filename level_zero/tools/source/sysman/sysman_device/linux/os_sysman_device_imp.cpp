/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device.h"

#include "drm_neo.h"
#include "os_interface.h"
#include "sysman/linux/os_sysman_imp.h"
#include "sysman/linux/sysfs_access.h"
#include "sysman/sysman_device/os_sysman_device.h"
#include "sysman/sysman_device/sysman_device_imp.h"

#include <unistd.h>

namespace L0 {

class LinuxSysmanDeviceImp : public OsSysmanDevice {
  public:
    void getSerialNumber(int8_t (&serialNumber)[ZET_STRING_PROPERTY_SIZE]) override;
    void getBoardNumber(int8_t (&boardNumber)[ZET_STRING_PROPERTY_SIZE]) override;
    void getBrandName(int8_t (&brandName)[ZET_STRING_PROPERTY_SIZE]) override;
    void getModelName(int8_t (&modelName)[ZET_STRING_PROPERTY_SIZE]) override;
    void getVendorName(int8_t (&vendorName)[ZET_STRING_PROPERTY_SIZE]) override;
    void getDriverVersion(int8_t (&driverVersion)[ZET_STRING_PROPERTY_SIZE]) override;
    LinuxSysmanDeviceImp(OsSysman *pOsSysman);
    ~LinuxSysmanDeviceImp() override = default;

    // Don't allow copies of the LinuxSysmanDeviceImp object
    LinuxSysmanDeviceImp(const LinuxSysmanDeviceImp &obj) = delete;
    LinuxSysmanDeviceImp &operator=(const LinuxSysmanDeviceImp &obj) = delete;

  private:
    SysfsAccess *pSysfsAccess;
    LinuxSysmanImp *pLinuxSysmanImp;
    static const std::string deviceDir;
    static const std::string vendorFile;
    static const std::string deviceFile;
    static const std::string subsystemVendorFile;
    static const std::string driverFile;
};

const std::string vendorIntel("Intel(R) Corporation");
const std::string unknown("Unknown");
const std::string intelPciId("0x8086");
const std::string LinuxSysmanDeviceImp::vendorFile("device/vendor");
const std::string LinuxSysmanDeviceImp::deviceFile("device/device");
const std::string LinuxSysmanDeviceImp::subsystemVendorFile("device/subsystem_vendor");
const std::string LinuxSysmanDeviceImp::driverFile("device/driver");

void LinuxSysmanDeviceImp::getSerialNumber(int8_t (&serialNumber)[ZET_STRING_PROPERTY_SIZE]) {
    std::copy(unknown.begin(), unknown.end(), serialNumber);
    serialNumber[unknown.size()] = '\0';
}

void LinuxSysmanDeviceImp::getBoardNumber(int8_t (&boardNumber)[ZET_STRING_PROPERTY_SIZE]) {
    std::copy(unknown.begin(), unknown.end(), boardNumber);
    boardNumber[unknown.size()] = '\0';
}

void LinuxSysmanDeviceImp::getBrandName(int8_t (&brandName)[ZET_STRING_PROPERTY_SIZE]) {
    std::string strVal;
    ze_result_t result = pSysfsAccess->read(subsystemVendorFile, strVal);
    if (ZE_RESULT_SUCCESS != result) {
        std::copy(unknown.begin(), unknown.end(), brandName);
        brandName[unknown.size()] = '\0';
        return;
    }
    if (strVal.compare(intelPciId) == 0) {
        std::copy(vendorIntel.begin(), vendorIntel.end(), brandName);
        brandName[vendorIntel.size()] = '\0';
        return;
    }
    std::copy(unknown.begin(), unknown.end(), brandName);
    brandName[unknown.size()] = '\0';
}

void LinuxSysmanDeviceImp::getModelName(int8_t (&modelName)[ZET_STRING_PROPERTY_SIZE]) {
    std::string strVal;
    ze_result_t result = pSysfsAccess->read(deviceFile, strVal);
    if (ZE_RESULT_SUCCESS != result) {
        std::copy(unknown.begin(), unknown.end(), modelName);
        modelName[unknown.size()] = '\0';
        return;
    }

    std::copy(strVal.begin(), strVal.end(), modelName);
    modelName[strVal.size()] = '\0';
}

void LinuxSysmanDeviceImp::getVendorName(int8_t (&vendorName)[ZET_STRING_PROPERTY_SIZE]) {
    std::string strVal;
    ze_result_t result = pSysfsAccess->read(vendorFile, strVal);
    if (ZE_RESULT_SUCCESS != result) {
        std::copy(unknown.begin(), unknown.end(), vendorName);
        vendorName[unknown.size()] = '\0';
        return;
    }
    if (strVal.compare(intelPciId) == 0) {
        std::copy(vendorIntel.begin(), vendorIntel.end(), vendorName);
        vendorName[vendorIntel.size()] = '\0';
        return;
    }
    std::copy(unknown.begin(), unknown.end(), vendorName);
    vendorName[unknown.size()] = '\0';
}

void LinuxSysmanDeviceImp::getDriverVersion(int8_t (&driverVersion)[ZET_STRING_PROPERTY_SIZE]) {
    std::string output, command, strVal, driver;
    //First get the driver name
    ze_result_t result = pSysfsAccess->readSymLink(driverFile, strVal);
    if (ZE_RESULT_SUCCESS != result) {
        std::copy(unknown.begin(), unknown.end(), driverVersion);
        driverVersion[unknown.size()] = '\0';
        return;
    }
    const auto loc = strVal.find_last_of('/');
    driver = strVal.substr(loc + 1);
    //create the command like: modinfo -F vermagic i915
    command = "modinfo -F vermagic " + driver;
    result = pLinuxSysmanImp->systemCmd(command, output);
    if (ZE_RESULT_SUCCESS != result) {
        std::copy(unknown.begin(), unknown.end(), driverVersion);
        driverVersion[unknown.size()] = '\0';
        return;
    }

    //After below copy Not appending null in driverVersion, as output is null terminated
    std::copy(output.begin(), output.end(), driverVersion);
}

LinuxSysmanDeviceImp::LinuxSysmanDeviceImp(OsSysman *pOsSysman) {
    pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);

    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

OsSysmanDevice *OsSysmanDevice::create(OsSysman *pOsSysman) {
    LinuxSysmanDeviceImp *pLinuxSysmanDeviceImp = new LinuxSysmanDeviceImp(pOsSysman);
    return static_cast<OsSysmanDevice *>(pLinuxSysmanDeviceImp);
}

} // namespace L0
