/*
 * Copyright (C) 2023-2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/driver/os_sysman_driver.h"
#include "level_zero/sysman/source/shared/linux/udev/udev_lib.h"

namespace NEO {
class HwDeviceId;
}

namespace L0 {
namespace Sysman {

class DrmNlApi;
class LinuxEventsUtil;
struct InfoLogHandleContext;
struct SysmanDevice;
struct SysmanDeviceImp;
struct SysmanDriverHandleImp;
class UdevLib;

class LinuxSysmanDriverImp : public OsSysmanDriver, NEO::NonCopyableAndNonMovableClass {
  public:
    LinuxSysmanDriverImp();
    ~LinuxSysmanDriverImp() override;

    ze_result_t eventsListen(uint64_t timeout, uint32_t count, zes_device_handle_t *phDevices, uint32_t *pNumDeviceEvents, zes_event_type_flags_t *pEvents) override;
    ze_result_t enumInfoLogs(uint32_t *pCount, zes_intel_info_log_handle_t *phInfoLogs) override;
    ze_result_t rescanDevices(SysmanDriverHandleImp *driverHandle, uint32_t *pCount, zes_device_handle_t *phDevices) override;
    MOCKABLE_VIRTUAL ze_result_t getPciBdfAndUuidForHwDevice(NEO::HwDeviceId *hwDeviceId, std::string &pciBdf, std::string &pciUuid);
    MOCKABLE_VIRTUAL ze_result_t updateHwDeviceId(SysmanDevice *sysmanDevice, const std::string &newBdf);
    void eventRegister(zes_event_type_flags_t events, SysmanDeviceImp *pSysmanDevice);
    L0::Sysman::UdevLib *getUdevLibHandle();
    DrmNlApi *getDrmNlApiHandle();
    static DrmNlApi *createDrmNlApi();
    static void destroyDrmNlApi(DrmNlApi *pDrmNl);

  protected:
    L0::Sysman::UdevLib *pUdevLib = nullptr;
    DrmNlApi *pDrmNl = nullptr;
    L0::Sysman::LinuxEventsUtil *pLinuxEventsUtil = nullptr;
    InfoLogHandleContext *pInfoLogHandleContext = nullptr;

  private:
    int32_t findDeviceIndexByPciUuid(SysmanDriverHandleImp *driverHandle, const std::string &pciUuid);
    void netlinkCleanup();
};

} // namespace Sysman
} // namespace L0
