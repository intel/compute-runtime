/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/os_interface.h"
#include "shared/source/os_interface/windows/wddm/wddm.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/sysman_imp.h"
#include "level_zero/tools/source/sysman/windows/kmd_sys.h"
#include "level_zero/tools/source/sysman/windows/kmd_sys_manager.h"

namespace L0 {
class FirmwareUtil;

class WddmSysmanImp : public OsSysman, NEO::NonCopyableOrMovableClass {
  public:
    WddmSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp);
    ~WddmSysmanImp() override;

    ze_result_t init() override;

    KmdSysManager &getKmdSysManager();
    FirmwareUtil *getFwUtilInterface();
    NEO::Wddm &getWddm();
    Device *getDeviceHandle();
    void releaseFwUtilInterface();
    std::vector<ze_device_handle_t> &getDeviceHandles() override;
    ze_device_handle_t getCoreDeviceHandle() override;

  protected:
    FirmwareUtil *pFwUtilInterface = nullptr;
    KmdSysManager *pKmdSysManager = nullptr;
    Device *pDevice = nullptr;

  private:
    SysmanDeviceImp *pParentSysmanDeviceImp = nullptr;
    NEO::Wddm *pWddm = nullptr;
    void createFwUtilInterface();
};

} // namespace L0
