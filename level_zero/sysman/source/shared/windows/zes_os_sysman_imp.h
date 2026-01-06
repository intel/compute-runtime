/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/product_helper.h"

#include "level_zero/sysman/source/device/os_sysman.h"

#include "neo_igfxfmid.h"

#include <array>
#include <cstdint>

namespace NEO {
class Wddm;
struct HardwareInfo;
} // namespace NEO
namespace L0 {
namespace Sysman {
class FirmwareUtil;
class KmdSysManager;
class SysmanProductHelper;
class PlatformMonitoringTech;
struct SysmanDevice;
struct SysmanDeviceImp;

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
    const NEO::HardwareInfo &getHardwareInfo() const override;
    PRODUCT_FAMILY getProductFamily() const;
    SysmanProductHelper *getSysmanProductHelper();
    PlatformMonitoringTech *getSysmanPmt();
    bool getUuid(std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid);
    bool generateUuidFromPciBusInfo(const NEO::PhysicalDevicePciBusInfo &pciBusInfo, std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid);
    ze_result_t initSurvivabilityMode(std::unique_ptr<NEO::HwDeviceId> hwDeviceId) override;
    bool isDeviceInSurvivabilityMode() override;
    std::unique_ptr<NEO::PhysicalDevicePciBusInfo> getPciBdfInfo() const override { return nullptr; }

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
    NEO::PhysicalDevicePciBusInfo pciBusInfo;
    SysmanDeviceImp *pParentSysmanDeviceImp = nullptr;
    NEO::Wddm *pWddm = nullptr;
    uint32_t subDeviceCount = 0;
    void createFwUtilInterface();
};

} // namespace Sysman
} // namespace L0
