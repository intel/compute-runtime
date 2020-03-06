/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device.h"

#include "drm_neo.h"
#include "os_interface.h"
#include "sysman/linux/sysfs_access.h"
#include "sysman/sysman_imp.h"

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
    ze_result_t systemCmd(const std::string cmd, std::string &output);

  private:
    SysmanImp *pParentSysmanImp;
    SysfsAccess *pSysfsAccess;
};

} // namespace L0
