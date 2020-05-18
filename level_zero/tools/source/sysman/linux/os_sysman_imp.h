/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/linux/pmt.h"
#include "level_zero/tools/source/sysman/linux/xml_parser/xml_parser.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

namespace L0 {

class LinuxSysmanImp : public OsSysman, NEO::NonCopyableOrMovableClass {
  public:
    LinuxSysmanImp(SysmanImp *pParentSysmanImp);
    LinuxSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp);
    ~LinuxSysmanImp() override;

    ze_result_t init() override;

    XmlParser *getXmlParser();
    FsAccess &getFsAccess();
    ProcfsAccess &getProcfsAccess();
    SysfsAccess &getSysfsAccess();
    NEO::Drm &getDrm();
    PlatformMonitoringTech &getPlatformMonitoringTechAccess();

  protected:
    XmlParser *pXmlParser = nullptr;
    FsAccess *pFsAccess = nullptr;
    ProcfsAccess *pProcfsAccess = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;
    PlatformMonitoringTech *pPmt = nullptr;

  private:
    LinuxSysmanImp() = delete;
    SysmanImp *pParentSysmanImp = nullptr;
    SysmanDeviceImp *pParentSysmanDeviceImp = nullptr;
    NEO::Drm *pDrm = nullptr;
};

} // namespace L0
