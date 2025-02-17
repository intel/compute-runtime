/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/os_sysman.h"

namespace NEO {
class Wddm;
}

namespace L0 {

class FirmwareUtil;
class KmdSysManager;

struct Device;

class WddmSysmanImp : public OsSysman, NEO::NonCopyableAndNonMovableClass {
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
    ze_bool_t isDriverModelSupported() override;

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
