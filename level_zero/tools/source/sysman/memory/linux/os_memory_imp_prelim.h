/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "sysman/memory/os_memory.h"

#include <map>

namespace L0 {

class SysfsAccess;
struct Device;
class PlatformMonitoringTech;
class LinuxMemoryImp : public OsMemory, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t getProperties(zes_mem_properties_t *pProperties) override;
    ze_result_t getBandwidth(zes_mem_bandwidth_t *pBandwidth) override;
    ze_result_t getState(zes_mem_state_t *pState) override;
    bool isMemoryModuleSupported() override;
    LinuxMemoryImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId);
    LinuxMemoryImp() = default;
    ~LinuxMemoryImp() override = default;

  protected:
    SysfsAccess *pSysfsAccess = nullptr;
    NEO::Drm *pDrm = nullptr;
    Device *pDevice = nullptr;
    PlatformMonitoringTech *pPmt = nullptr;
    void getHbmFrequency(PRODUCT_FAMILY productFamily, unsigned short stepping, uint64_t &hbmFrequency);

  private:
    ze_result_t getVFIDString(std::string &vfID);
    static const std::string deviceMemoryHealth;
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
    std::string physicalSizeFile;
    void init();
};

// Mapping of i915 memory health to L0 health params
// OK  status for No error, no HBM memory sparing, default value
// REBOOT_ALARM status for Hardware warning interrupt received and uevent has been sent, and system should be rebooted ASAP
// EC_FAILED sysfs status for Error correction failed: user did not reboot, and the uncorrectable errors happened
// DEGRADED  sysfs status for System has been rebooted and memory sparing is in action, detectable at boot time
// DEGRADED_FAILED  sysfs status for Upon receival of the final interrupt that uncorrectable errors happened when memory was already in sparing mode

const std::map<std::string, zes_mem_health_t> i915ToL0MemHealth{
    {"OK", ZES_MEM_HEALTH_OK},
    {"REBOOT_ALARM", ZES_MEM_HEALTH_DEGRADED},
    {"DEGRADED", ZES_MEM_HEALTH_CRITICAL},
    {"DEGRADED_FAILED", ZES_MEM_HEALTH_REPLACE},
    {"EC_FAILED", ZES_MEM_HEALTH_REPLACE}};

} // namespace L0
