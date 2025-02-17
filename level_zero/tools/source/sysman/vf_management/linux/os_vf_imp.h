/*
 * Copyright (C) 2024-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/linux/drm_neo.h"
#include "shared/source/os_interface/product_helper.h"

#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"
#include "level_zero/tools/source/sysman/vf_management/vf_imp.h"

#include <set>
#include <string>

namespace L0 {
class SysfsAccess;

class LinuxVfImp : public OsVf, NEO::NonCopyableAndNonMovableClass {
  public:
    struct EngineUtilsData {
        zes_engine_group_t engineType;
        int64_t busyTicksFd;
        int64_t totalTicksFd;
    };

    LinuxVfImp() = default;
    LinuxVfImp(OsSysman *pOsSysman, uint32_t vfId);
    ~LinuxVfImp() override;

    ze_result_t vfOsGetCapabilities(zes_vf_exp2_capabilities_t *pCapability) override;
    ze_result_t vfOsGetMemoryUtilization(uint32_t *pCount, zes_vf_util_mem_exp2_t *pMemUtil) override;
    ze_result_t vfOsGetEngineUtilization(uint32_t *pCount, zes_vf_util_engine_exp2_t *pEngineUtil) override;
    bool vfOsGetLocalMemoryQuota(uint64_t &lMemQuota) override;
    bool vfOsGetLocalMemoryUsed(uint64_t &lMemUsed) override;

  protected:
    ze_result_t vfEngineDataInit();
    ze_result_t getEngineUtilization(uint32_t *pCount, zes_vf_util_engine_exp2_t *pEngineUtil);
    ze_result_t getVfBDFAddress(uint32_t vfIdMinusOne, zes_pci_address_t *address);
    void vfGetInstancesFromEngineInfo(NEO::EngineInfo *engineInfo, std::set<std::pair<zes_engine_group_t, uint32_t>> &engineGroupAndInstance);
    void cleanup();
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;
    uint32_t vfId = 0;
    static const uint32_t maxMemoryTypes = 1; // Since only the Device Memory Utilization is Supported and not for the Host Memory, this value is 1

  private:
    std::set<std::pair<zes_engine_group_t, uint32_t>> engineGroupAndInstance = {};
    std::vector<EngineUtilsData> pEngineUtils = {};
    std::once_flag initEngineDataOnce;
};

} // namespace L0
