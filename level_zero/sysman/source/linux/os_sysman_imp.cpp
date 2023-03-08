/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/sysman/source/linux/os_sysman_imp.h"

#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/os_interface.h"

#include "level_zero/sysman/source/firmware_util/firmware_util.h"
#include "level_zero/sysman/source/linux/fs_access.h"
#include "level_zero/sysman/source/linux/pmt/pmt.h"
#include "level_zero/sysman/source/linux/pmu/pmu.h"

namespace L0 {
namespace Sysman {

const std::string LinuxSysmanImp::deviceDir("device");

ze_result_t LinuxSysmanImp::init() {
    pFsAccess = FsAccess::create();
    DEBUG_BREAK_IF(nullptr == pFsAccess);

    if (pProcfsAccess == nullptr) {
        pProcfsAccess = ProcfsAccess::create();
    }
    DEBUG_BREAK_IF(nullptr == pProcfsAccess);

    ze_result_t result;
    NEO::OSInterface &osInterface = *pParentSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    if (osInterface.getDriverModel()->getDriverModelType() != NEO::DriverModelType::DRM) {
        return ZE_RESULT_ERROR_UNSUPPORTED_FEATURE;
    }
    auto sysmanHwDeviceId = getSysmanHwDeviceId();
    sysmanHwDeviceId->openFileDescriptor();
    int myDeviceFd = sysmanHwDeviceId->getFileDescriptor();
    std::string myDeviceName;
    result = pProcfsAccess->getFileName(pProcfsAccess->myProcessId(), myDeviceFd, myDeviceName);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }

    if (pSysfsAccess == nullptr) {
        pSysfsAccess = SysfsAccess::create(myDeviceName);
    }
    DEBUG_BREAK_IF(nullptr == pSysfsAccess);

    subDeviceCount = NEO::GfxCoreHelper::getSubDevicesCount(&pParentSysmanDeviceImp->getHardwareInfo());
    if (subDeviceCount == 1) {
        subDeviceCount = 0;
    }

    osInterface.getDriverModel()->as<NEO::Drm>()->cleanup();
    // Close Drm handles
    sysmanHwDeviceId->closeFileDescriptor();
    pPmuInterface = PmuInterface::create(this);
    return createPmtHandles();
}

SysmanHwDeviceIdDrm *LinuxSysmanImp::getSysmanHwDeviceId() {
    return static_cast<SysmanHwDeviceIdDrm *>(getDrm()->getHwDeviceId().get());
}

NEO::Drm *LinuxSysmanImp::getDrm() {
    const auto &osInterface = *pParentSysmanDeviceImp->getRootDeviceEnvironment().osInterface;
    return osInterface.getDriverModel()->as<NEO::Drm>();
}

ze_result_t LinuxSysmanImp::createPmtHandles() {
    std::string gtDevicePCIPath;
    auto result = pSysfsAccess->getRealPath("device", gtDevicePCIPath);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    auto gpuUpstreamPortPath = getPciCardBusDirectoryPath(gtDevicePCIPath);
    PlatformMonitoringTech::create(this, gpuUpstreamPortPath, mapOfSubDeviceIdToPmtObject);
    return result;
}

static std::string modifyPathOnLevel(std::string realPciPath, uint8_t nLevel) {
    size_t loc;
    // we need to change the absolute path to 'nLevel' levels up
    while (nLevel > 0) {
        loc = realPciPath.find_last_of('/');
        if (loc == std::string::npos) {
            break;
        }
        realPciPath = realPciPath.substr(0, loc);
        nLevel--;
    }
    return realPciPath;
}

std::string LinuxSysmanImp::getPciCardBusDirectoryPath(std::string realPciPath) {
    // the cardbus is always the second pci folder after the pcie slot.
    //    +-[0000:89]-+-00.0
    // |           +-00.1
    // |           +-00.2
    // |           +-00.4
    // |           \-02.0-[8a-8e]----00.0-[8b-8e]--+-01.0-[8c-8d]----00.0
    // |                                           \-02.0-[8e]--+-00.0
    // |                                                        +-00.1
    // |                                                        \-00.2
    // /sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/0000:8b:01.0/0000:8c:00.0
    // '/sys/devices/pci0000:89/0000:89:02.0/0000:8a:00.0/' will always be the same distance.
    // from 0000:8c:00.0 i.e the 2nd PCI address from the gt tile.
    return modifyPathOnLevel(realPciPath, 2);
}

PlatformMonitoringTech *LinuxSysmanImp::getPlatformMonitoringTechAccess(uint32_t subDeviceId) {
    auto subDeviceIdToPmtEntry = mapOfSubDeviceIdToPmtObject.find(subDeviceId);
    if (subDeviceIdToPmtEntry == mapOfSubDeviceIdToPmtObject.end()) {
        return nullptr;
    }
    return subDeviceIdToPmtEntry->second;
}

void LinuxSysmanImp::releasePmtObject() {
    for (auto &subDeviceIdToPmtEntry : mapOfSubDeviceIdToPmtObject) {
        if (subDeviceIdToPmtEntry.second) {
            delete subDeviceIdToPmtEntry.second;
            subDeviceIdToPmtEntry.second = nullptr;
        }
    }
    mapOfSubDeviceIdToPmtObject.clear();
}

FsAccess &LinuxSysmanImp::getFsAccess() {
    UNRECOVERABLE_IF(nullptr == pFsAccess);
    return *pFsAccess;
}

ProcfsAccess &LinuxSysmanImp::getProcfsAccess() {
    UNRECOVERABLE_IF(nullptr == pProcfsAccess);
    return *pProcfsAccess;
}

SysfsAccess &LinuxSysmanImp::getSysfsAccess() {
    UNRECOVERABLE_IF(nullptr == pSysfsAccess);
    return *pSysfsAccess;
}

SysmanDeviceImp *LinuxSysmanImp::getSysmanDeviceImp() {
    return pParentSysmanDeviceImp;
}

uint32_t LinuxSysmanImp::getSubDeviceCount() {
    return subDeviceCount;
}

LinuxSysmanImp::LinuxSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp) {
    this->pParentSysmanDeviceImp = pParentSysmanDeviceImp;
    executionEnvironment = pParentSysmanDeviceImp->getExecutionEnvironment();
    rootDeviceIndex = pParentSysmanDeviceImp->getRootDeviceIndex();
}

void LinuxSysmanImp::createFwUtilInterface() {
    const auto pciBusInfo = pParentSysmanDeviceImp->getRootDeviceEnvironment().osInterface->getDriverModel()->getPciBusInfo();
    const uint16_t domain = static_cast<uint16_t>(pciBusInfo.pciDomain);
    const uint8_t bus = static_cast<uint8_t>(pciBusInfo.pciBus);
    const uint8_t device = static_cast<uint8_t>(pciBusInfo.pciDevice);
    const uint8_t function = static_cast<uint8_t>(pciBusInfo.pciFunction);

    pFwUtilInterface = FirmwareUtil::create(domain, bus, device, function);
}

FirmwareUtil *LinuxSysmanImp::getFwUtilInterface() {
    if (pFwUtilInterface == nullptr) {
        createFwUtilInterface();
    }
    return pFwUtilInterface;
}

void LinuxSysmanImp::releaseFwUtilInterface() {
    if (nullptr != pFwUtilInterface) {
        delete pFwUtilInterface;
        pFwUtilInterface = nullptr;
    }
}

LinuxSysmanImp::~LinuxSysmanImp() {
    if (nullptr != pSysfsAccess) {
        delete pSysfsAccess;
        pSysfsAccess = nullptr;
    }
    if (nullptr != pProcfsAccess) {
        delete pProcfsAccess;
        pProcfsAccess = nullptr;
    }
    if (nullptr != pFsAccess) {
        delete pFsAccess;
        pFsAccess = nullptr;
    }
    if (nullptr != pPmuInterface) {
        delete pPmuInterface;
        pPmuInterface = nullptr;
    }
    releaseFwUtilInterface();
    releasePmtObject();
}

OsSysman *OsSysman::create(SysmanDeviceImp *pParentSysmanDeviceImp) {
    LinuxSysmanImp *pLinuxSysmanImp = new LinuxSysmanImp(pParentSysmanDeviceImp);
    return static_cast<OsSysman *>(pLinuxSysmanImp);
}

} // namespace Sysman
} // namespace L0
