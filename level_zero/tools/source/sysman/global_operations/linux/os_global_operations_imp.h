/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/os_interface/linux/pmt_util.h"

#include "level_zero/tools/source/sysman/global_operations/os_global_operations.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

namespace L0 {
class SysfsAccess;
struct Device;

class LinuxGlobalOperationsImp : public OsGlobalOperations, NEO::NonCopyableOrMovableClass {
  public:
    bool getSerialNumber(char (&serialNumber)[ZES_STRING_PROPERTY_SIZE]) override;
    void getBoardNumber(char (&boardNumber)[ZES_STRING_PROPERTY_SIZE]) override;
    void getBrandName(char (&brandName)[ZES_STRING_PROPERTY_SIZE]) override;
    void getModelName(char (&modelName)[ZES_STRING_PROPERTY_SIZE]) override;
    void getVendorName(char (&vendorName)[ZES_STRING_PROPERTY_SIZE]) override;
    void getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) override;
    void getWedgedStatus(zes_device_state_t *pState) override;
    void getRepairStatus(zes_device_state_t *pState) override;
    Device *getDevice() override;
    ze_result_t reset(ze_bool_t force) override;
    ze_result_t scanProcessesState(std::vector<zes_process_state_t> &pProcessList) override;
    ze_result_t deviceGetState(zes_device_state_t *pState) override;
    LinuxGlobalOperationsImp() = default;
    LinuxGlobalOperationsImp(OsSysman *pOsSysman);
    ~LinuxGlobalOperationsImp() override = default;

  protected:
    FsAccess *pFsAccess = nullptr;
    ProcfsAccess *pProcfsAccess = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    Device *pDevice = nullptr;
    int resetTimeout = 10000; // in milliseconds
    void releaseSysmanDeviceResources();
    void releaseDeviceResources();
    ze_result_t initDevice();
    void reInitSysmanDeviceResources();

  private:
    static const std::string deviceDir;
    static const std::string subsystemVendorFile;
    static const std::string driverFile;
    static const std::string functionLevelReset;
    static const std::string clientsDir;
    static const std::string srcVersionFile;
    static const std::string agamaVersionFile;
    static const std::string ueventWedgedFile;
    static bool readSerialNumber(std::string_view telemDir, std::array<char, NEO::PmtUtil::guidStringSize> &guidString, const uint64_t offset, char (&serialNumber)[ZES_STRING_PROPERTY_SIZE]);
    std::string devicePciBdf = "";
    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    uint32_t rootDeviceIndex = 0u;
};

} // namespace L0
