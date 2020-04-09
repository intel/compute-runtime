/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "sysman/linux/os_sysman_imp.h"
#include "sysman/scheduler/scheduler_imp.h"

namespace L0 {
class LinuxSchedulerImp : public OsScheduler {
  public:
    LinuxSchedulerImp(OsSysman *pOsSysman);
    ~LinuxSchedulerImp() override = default;

    // Don't allow copies of the LinuxSchedulerImp object
    LinuxSchedulerImp(const LinuxSchedulerImp &obj) = delete;
    LinuxSchedulerImp &operator=(const LinuxSchedulerImp &obj) = delete;

  private:
    SysfsAccess *pSysfsAccess;
};

LinuxSchedulerImp::LinuxSchedulerImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);

    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

OsScheduler *OsScheduler::create(OsSysman *pOsSysman) {
    LinuxSchedulerImp *pLinuxSchedulerImp = new LinuxSchedulerImp(pOsSysman);
    return static_cast<OsScheduler *>(pLinuxSchedulerImp);
}

} // namespace L0