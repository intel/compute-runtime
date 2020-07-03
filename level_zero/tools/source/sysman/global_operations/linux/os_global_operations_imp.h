/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/tools/source/sysman/global_operations/os_global_operations.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

namespace L0 {
class SysfsAccess;

class LinuxGlobalOperationsImp : public OsGlobalOperations, NEO::NonCopyableOrMovableClass {
  public:
    void getSerialNumber(int8_t (&serialNumber)[ZET_STRING_PROPERTY_SIZE]) override;
    void getBoardNumber(int8_t (&boardNumber)[ZET_STRING_PROPERTY_SIZE]) override;
    void getBrandName(int8_t (&brandName)[ZET_STRING_PROPERTY_SIZE]) override;
    void getModelName(int8_t (&modelName)[ZET_STRING_PROPERTY_SIZE]) override;
    void getVendorName(int8_t (&vendorName)[ZET_STRING_PROPERTY_SIZE]) override;
    void getDriverVersion(int8_t (&driverVersion)[ZET_STRING_PROPERTY_SIZE]) override;
    ze_result_t reset() override;
    ze_result_t scanProcessesState(std::vector<zet_process_state_t> &pProcessList) override;
    LinuxGlobalOperationsImp() = default;
    LinuxGlobalOperationsImp(OsSysman *pOsSysman);
    ~LinuxGlobalOperationsImp() override = default;

  protected:
    SysfsAccess *pSysfsAccess = nullptr;
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;

  private:
    static const std::string deviceDir;
    static const std::string vendorFile;
    static const std::string deviceFile;
    static const std::string subsystemVendorFile;
    static const std::string driverFile;
    static const std::string functionLevelReset;
    static const std::string clientsDir;
};

} // namespace L0
