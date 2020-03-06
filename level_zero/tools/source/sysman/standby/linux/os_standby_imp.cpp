/*
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "level_zero/core/source/device.h"

#include "drm_neo.h"
#include "os_interface.h"
#include "sysman/linux/os_sysman_imp.h"
#include "sysman/linux/sysfs_access.h"
#include "sysman/standby/os_standby.h"
#include "sysman/standby/standby_imp.h"

namespace L0 {

class LinuxStandbyImp : public OsStandby {
  public:
    ze_result_t getMode(zet_standby_promo_mode_t &mode) override;
    ze_result_t setMode(zet_standby_promo_mode_t mode) override;

    LinuxStandbyImp(OsSysman *pOsSysman);
    ~LinuxStandbyImp() override = default;

    // Don't allow copies of the LinuxStandbyImp object
    LinuxStandbyImp(const LinuxStandbyImp &obj) = delete;
    LinuxStandbyImp &operator=(const LinuxStandbyImp &obj) = delete;

  private:
    SysfsAccess *pSysfsAccess;

    static const std::string standbyModeFile;
    static const int standbyModeDefault = 1;
    static const int standbyModeNever = 0;
};

const std::string LinuxStandbyImp::standbyModeFile("power/rc6_enable");

ze_result_t LinuxStandbyImp::getMode(zet_standby_promo_mode_t &mode) {
    int currentMode;
    ze_result_t result = pSysfsAccess->read(standbyModeFile, currentMode);
    if (ZE_RESULT_SUCCESS != result) {
        return result;
    }
    if (standbyModeDefault == currentMode) {
        mode = ZET_STANDBY_PROMO_MODE_DEFAULT;
        return ZE_RESULT_SUCCESS;
    }
    if (standbyModeNever == currentMode) {
        mode = ZET_STANDBY_PROMO_MODE_NEVER;
        return ZE_RESULT_SUCCESS;
    }
    return ZE_RESULT_ERROR_UNKNOWN;
}

ze_result_t LinuxStandbyImp::setMode(zet_standby_promo_mode_t mode) {
    if (ZET_STANDBY_PROMO_MODE_DEFAULT == mode) {
        return pSysfsAccess->write(standbyModeFile, standbyModeDefault);
    }
    return pSysfsAccess->write(standbyModeFile, standbyModeNever);
}

LinuxStandbyImp::LinuxStandbyImp(OsSysman *pOsSysman) {
    LinuxSysmanImp *pLinuxSysmanImp = static_cast<LinuxSysmanImp *>(pOsSysman);

    pSysfsAccess = &pLinuxSysmanImp->getSysfsAccess();
}

OsStandby *OsStandby::create(OsSysman *pOsSysman) {
    LinuxStandbyImp *pLinuxStandbyImp = new LinuxStandbyImp(pOsSysman);
    return static_cast<OsStandby *>(pLinuxStandbyImp);
}

} // namespace L0
