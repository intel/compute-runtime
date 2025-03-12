/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/hw_info.h"
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/product_helper.h"

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

class WddmSysmanImp : public OsSysman, NEO::NonCopyableAndNonMovableClass {
  public:
    WddmSysmanImp(SysmanDeviceImp *pParentSysmanDeviceImp);
    ~WddmSysmanImp() override;

    ze_result_t init() override;

    KmdSysManager &getKmdSysManager();
    FirmwareUtil *getFwUtilInterface();
    NEO::Wddm &getWddm();
    void releaseFwUtilInterface();

    uint32_t getSubDeviceCount() override;
    void getDeviceUuids(std::vector<std::string> &deviceUuids) override;
    SysmanDeviceImp *getSysmanDeviceImp();
    const NEO::HardwareInfo &getHardwareInfo() const override { return pParentSysmanDeviceImp->getHardwareInfo(); }
    PRODUCT_FAMILY getProductFamily() const { return pParentSysmanDeviceImp->getProductFamily(); }
    SysmanProductHelper *getSysmanProductHelper();
    PlatformMonitoringTech *getSysmanPmt();
    bool getUuid(std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid);
    bool generateUuidFromPciBusInfo(const NEO::PhysicalDevicePciBusInfo &pciBusInfo, std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid);

  protected:
    FirmwareUtil *pFwUtilInterface = nullptr;
    KmdSysManager *pKmdSysManager = nullptr;
    SysmanDevice *pDevice = nullptr;
    std::unique_ptr<PlatformMonitoringTech> pPmt;
    std::unique_ptr<SysmanProductHelper> pSysmanProductHelper;
    struct {
        bool isValid = false;
        std::array<uint8_t, NEO::ProductHelper::uuidSize> id{};
    } uuid;

  private:
    SysmanDeviceImp *pParentSysmanDeviceImp = nullptr;
    NEO::Wddm *pWddm = nullptr;
    uint32_t subDeviceCount = 0;
    void createFwUtilInterface();
};

} // namespace Sysman
} // namespace L0
