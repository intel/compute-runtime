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

#include "level_zero/sysman/source/api/vf_management/sysman_vf_imp.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

#include <set>
#include <string>

namespace L0 {
namespace Sysman {
class SysFsAccessInterface;

using EngineInstanceGtId = std::pair<uint32_t, uint32_t>; // Pair of engineInstance and gtId

class LinuxVfImp : public OsVf, NEO::NonCopyableAndNonMovableClass {
  public:
    struct EngineUtilsData {
        zes_engine_group_t engineType{};
        int64_t busyTicksFd = -1;
        int64_t totalTicksFd = -1;
    };

    LinuxVfImp(OsSysman *pOsSysman, uint32_t vfId);
    ~LinuxVfImp() override;

    ze_result_t vfOsGetCapabilities(zes_vf_exp2_capabilities_t *pCapability) override;
    ze_result_t vfOsGetMemoryUtilization(uint32_t *pCount, zes_vf_util_mem_exp2_t *pMemUtil) override;
    ze_result_t vfOsGetEngineUtilization(uint32_t *pCount, zes_vf_util_engine_exp2_t *pEngineUtil) override;
    bool vfOsGetLocalMemoryUsed(uint64_t &lMemUsed) override;

  protected:
    ze_result_t vfEngineDataInit();
    ze_result_t getVfBDFAddress(uint32_t vfIdMinusOne, zes_pci_address_t *address);
    void vfGetInstancesFromEngineInfo(NEO::Drm *pDrm);
    void cleanup();
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    SysFsAccessInterface *pSysfsAccess = nullptr;
    uint32_t vfId = 0;
    static const uint32_t maxMemoryTypes = 1; // Since only the Device Memory Utilization is Supported and not for the Host Memory, this value is 1

  private:
    std::set<std::pair<zes_engine_group_t, EngineInstanceGtId>> engineGroupInstance = {};
    std::vector<EngineUtilsData> pEngineUtils = {};
    std::once_flag initEngineDataOnce;
};

} // namespace Sysman
} // namespace L0
