/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/linux/os_interface.h"

#include "level_zero/core/source/device.h"
#include "level_zero/tools/source/sysman/linux/sysfs_access.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"

namespace L0 {

class LinuxSysmanImp : public OsSysman {
  public:
    LinuxSysmanImp(SysmanImp *pParentSysmanImp) : pParentSysmanImp(pParentSysmanImp), pSysfsAccess(nullptr){};
    ~LinuxSysmanImp() override;

    // Don't allow copies of the LinuxSysmanImp object
    LinuxSysmanImp(const LinuxSysmanImp &obj) = delete;
    LinuxSysmanImp &operator=(const LinuxSysmanImp &obj) = delete;

    ze_result_t init() override;

    SysfsAccess &getSysfsAccess();

  private:
    SysmanImp *pParentSysmanImp;
    SysfsAccess *pSysfsAccess;
};

} // namespace L0
