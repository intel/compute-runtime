/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/linux/drm_neo.h"

#include "level_zero/tools/source/sysman/memory/os_memory.h"

#include <map>

namespace L0 {

class SysfsAccess;
struct Device;
class PlatformMonitoringTech;
class LinuxSysmanImp;
class LinuxMemoryImp : public OsMemory, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t getProperties(zes_mem_properties_t *pProperties) override;
    ze_result_t getBandwidth(zes_mem_bandwidth_t *pBandwidth) override;
    ze_result_t getState(zes_mem_state_t *pState) override;

    bool isMemoryModuleSupported() override;
    LinuxMemoryImp(OsSysman *pOsSysman, ze_bool_t onSubdevice, uint32_t subdeviceId);
    LinuxMemoryImp() = default;
    ~LinuxMemoryImp() override = default;

  protected:
    L0::LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;
    NEO::Drm *pDrm = nullptr;
    Device *pDevice = nullptr;
    PlatformMonitoringTech *pPmt = nullptr;
    void getHbmFrequency(PRODUCT_FAMILY productFamily, unsigned short stepping, uint64_t &hbmFrequency);

  private:
    ze_result_t readMcChannelCounters(uint64_t &readCounters, uint64_t &writeCounters);
    ze_result_t getVFIDString(std::string &vfID);
    ze_result_t getBandwidthForDg2(zes_mem_bandwidth_t *pBandwidth);
    ze_result_t getHbmBandwidth(uint32_t numHbmModules, zes_mem_bandwidth_t *pBandwidth);
    ze_result_t getHbmBandwidthPVC(uint32_t numHbmModules, zes_mem_bandwidth_t *pBandwidth);
    ze_result_t getHbmBandwidthEx(uint32_t numHbmModules, uint32_t counterMaxValue, uint64_t *pReadCounters, uint64_t *pWriteCounters, uint64_t *pMaxBandwidth, uint64_t timeout);
    void init();
    static const std::string deviceMemoryHealth;
    bool isSubdevice = false;
    uint32_t subdeviceId = 0;
    std::string physicalSizeFile;
};

} // namespace L0
