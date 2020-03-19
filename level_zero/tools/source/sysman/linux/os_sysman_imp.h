/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/linux/fs_access.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

namespace L0 {

class LinuxSysmanImp : public OsSysman {
  public:
    LinuxSysmanImp(SysmanImp *pParentSysmanImp);
    ~LinuxSysmanImp() override;

    // Don't allow copies of the LinuxSysmanImp object
    LinuxSysmanImp(const LinuxSysmanImp &obj) = delete;
    LinuxSysmanImp &operator=(const LinuxSysmanImp &obj) = delete;

    ze_result_t init() override;

    FsAccess &getFsAccess();
    ProcfsAccess &getProcfsAccess();
    SysfsAccess &getSysfsAccess();

  private:
    LinuxSysmanImp() = delete;
    SysmanImp *pParentSysmanImp = nullptr;
    FsAccess *pFsAccess = nullptr;
    ProcfsAccess *pProcfsAccess = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;
};

} // namespace L0
