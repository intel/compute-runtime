/*
 * Copyright (C) 2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/execution_environment/execution_environment.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/linux/hw_device_id_linux.h"
#include "level_zero/sysman/source/os_sysman.h"
#include "level_zero/sysman/source/sysman_device_imp.h"

#include <map>

namespace NEO {
class Drm;
} // namespace NEO

namespace L0 {
namespace Sysman {
class PlatformMonitoringTech;
class PmuInterface;
class FsAccess;
class ProcfsAccess;
class SysfsAccess;
class FirmwareUtil;

class LinuxSysmanImp : public OsSysman, NEO::NonCopyableOrMovableClass {
  public:
    LinuxSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp);
    ~LinuxSysmanImp() override;

    ze_result_t init() override;

    FirmwareUtil *getFwUtilInterface();
    PmuInterface *getPmuInterface() { return pPmuInterface; }
    FsAccess &getFsAccess();
    ProcfsAccess &getProcfsAccess();
    SysfsAccess &getSysfsAccess();
    SysmanDeviceImp *getSysmanDeviceImp();
    uint32_t getSubDeviceCount() override;
    std::string getPciCardBusDirectoryPath(std::string realPciPath);
    PlatformMonitoringTech *getPlatformMonitoringTechAccess(uint32_t subDeviceId);
    PRODUCT_FAMILY getProductFamily() const { return pParentSysmanDeviceImp->getProductFamily(); }
    SysmanHwDeviceIdDrm *getSysmanHwDeviceId();
    NEO::Drm *getDrm();
    void releasePmtObject();
    ze_result_t createPmtHandles();

    std::string devicePciBdf = "";
    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    uint32_t rootDeviceIndex;

  protected:
    FsAccess *pFsAccess = nullptr;
    ProcfsAccess *pProcfsAccess = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;
    std::map<uint32_t, L0::Sysman::PlatformMonitoringTech *> mapOfSubDeviceIdToPmtObject;
    uint32_t subDeviceCount = 0;
    FirmwareUtil *pFwUtilInterface = nullptr;
    PmuInterface *pPmuInterface = nullptr;
    void releaseFwUtilInterface();

  private:
    LinuxSysmanImp() = delete;
    SysmanDeviceImp *pParentSysmanDeviceImp = nullptr;
    static const std::string deviceDir;
    void createFwUtilInterface();
};

} // namespace Sysman
} // namespace L0
