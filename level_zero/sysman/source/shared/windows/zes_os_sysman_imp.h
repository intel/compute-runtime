/*
 * Copyright (C) 2023-2024 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/device/os_sysman.h"
#include "level_zero/sysman/source/device/sysman_device.h"
#include "level_zero/sysman/source/device/sysman_device_imp.h"
#include "level_zero/sysman/source/shared/firmware_util/sysman_firmware_util.h"
#include "level_zero/sysman/source/shared/windows/sysman_kmd_sys_manager.h"

namespace NEO {
class Wddm;
}
namespace L0 {
namespace Sysman {
class SysmanProductHelper;
class PlatformMonitoringTech;

class WddmSysmanImp : public OsSysman, NEO::NonCopyableOrMovableClass {
  public:
    WddmSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp);
    ~WddmSysmanImp() override;

    ze_result_t init() override;

    KmdSysManager &getKmdSysManager();
    FirmwareUtil *getFwUtilInterface();
    NEO::Wddm &getWddm();
    void releaseFwUtilInterface();

    uint32_t getSubDeviceCount() override;
    SysmanDeviceImp *getSysmanDeviceImp();
    const NEO::HardwareInfo &getHardwareInfo() const override { return pParentSysmanDeviceImp->getHardwareInfo(); }
    PRODUCT_FAMILY getProductFamily() const { return pParentSysmanDeviceImp->getProductFamily(); }
    SysmanProductHelper *getSysmanProductHelper();
    PlatformMonitoringTech *getSysmanPmt();

  protected:
    FirmwareUtil *pFwUtilInterface = nullptr;
    KmdSysManager *pKmdSysManager = nullptr;
    SysmanDevice *pDevice = nullptr;
    std::unique_ptr<PlatformMonitoringTech> pPmt;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper;

  private:
    SysmanDeviceImp *pParentSysmanDeviceImp = nullptr;
    NEO::Wddm *pWddm = nullptr;
    uint32_t subDeviceCount = 0;
    void createFwUtilInterface();
};

} // namespace Sysman
} // namespace L0
