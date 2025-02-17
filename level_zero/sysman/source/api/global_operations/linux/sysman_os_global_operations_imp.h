/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/debug_settings/debug_settings_manager.h"
#include "shared/source/helpers/gfx_core_helper.h"
#include "shared/source/os_interface/driver_info.h"
#include "shared/source/os_interface/linux/pmt_util.h"
#include "shared/source/os_interface/product_helper.h"

#include "level_zero/sysman/source/api/global_operations/sysman_os_global_operations.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {
class SysFsAccessInterface;
class FsAccessInterface;
class ProcFsAccessInterface;
class SysmanKmdInterface;
constexpr uint32_t maxUuidsPerDevice = 3;

class LinuxGlobalOperationsImp : public OsGlobalOperations, NEO::NonCopyableAndNonMovableClass {
  public:
    bool getSerialNumber(char (&serialNumber)[ZES_STRING_PROPERTY_SIZE]) override;
    bool getBoardNumber(char (&boardNumber)[ZES_STRING_PROPERTY_SIZE]) override;
    void getBrandName(char (&brandName)[ZES_STRING_PROPERTY_SIZE]) override;
    void getModelName(char (&modelName)[ZES_STRING_PROPERTY_SIZE]) override;
    void getVendorName(char (&vendorName)[ZES_STRING_PROPERTY_SIZE]) override;
    void getDriverVersion(char (&driverVersion)[ZES_STRING_PROPERTY_SIZE]) override;
    void getRepairStatus(zes_device_state_t *pState) override;
    void getTimerResolution(double *pTimerResolution) override;
    ze_result_t reset(ze_bool_t force) override;
    ze_result_t scanProcessesState(std::vector<zes_process_state_t> &pProcessList) override;
    ze_result_t deviceGetState(zes_device_state_t *pState) override;
    ze_result_t resetExt(zes_reset_properties_t *pProperties) override;
    bool getUuid(std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid) override;
    bool generateUuidFromPciBusInfo(const NEO::PhysicalDevicePciBusInfo &pciBusInfo, std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid) override;
    ze_bool_t getDeviceInfoByUuid(zes_uuid_t uuid, ze_bool_t *onSubdevice, uint32_t *subdeviceId) override;
    bool generateUuidFromPciAndSubDeviceInfo(uint32_t subDeviceID, const NEO::PhysicalDevicePciBusInfo &pciBusInfo, std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid);
    ze_result_t getSubDeviceProperties(uint32_t *pCount, zes_subdevice_exp_properties_t *pSubdeviceProps) override;
    LinuxGlobalOperationsImp() = default;
    LinuxGlobalOperationsImp(OsSysman *pOsSysman);
    ~LinuxGlobalOperationsImp() override = default;

    struct {
        bool isValid = false;
        std::array<uint8_t, NEO::ProductHelper::uuidSize> id;
    } uuid[maxUuidsPerDevice];

  protected:
    struct DeviceMemStruct {
        uint64_t deviceMemorySize;
        uint64_t deviceSharedMemorySize;
    };
    struct EngineMemoryPairType {
        int64_t engineTypeField;
        DeviceMemStruct deviceMemStructField;
    };

    FsAccessInterface *pFsAccess = nullptr;
    ProcFsAccessInterface *pProcfsAccess = nullptr;
    SysFsAccessInterface *pSysfsAccess = nullptr;
    LinuxSysmanImp *pLinuxSysmanImp = nullptr;
    int resetTimeout = 10000; // in milliseconds
    void releaseSysmanDeviceResources();
    void releaseDeviceResources();
    ze_result_t initDevice();
    void reInitSysmanDeviceResources();
    ze_result_t readClientInfoFromSysfs(std::map<uint64_t, EngineMemoryPairType> &pidClientMap);
    ze_result_t readClientInfoFromFdInfo(std::map<uint64_t, EngineMemoryPairType> &pidClientMap);

  private:
    static const std::string deviceDir;
    static const std::string vendorFile;
    static const std::string subsystemVendorFile;
    static const std::string driverFile;
    static const std::string functionLevelReset;
    static const std::string clientsDir;
    static const std::string ueventWedgedFile;
    std::string devicePciBdf = "";
    NEO::ExecutionEnvironment *executionEnvironment = nullptr;
    uint32_t rootDeviceIndex = 0u;
    ze_result_t getListOfEnginesUsedByProcess(std::vector<std::string> &fdFileContents, uint32_t &activeEngines);
    ze_result_t getMemoryStatsUsedByProcess(std::vector<std::string> &fdFileContents, uint64_t &memSize, uint64_t &sharedSize);
    ze_result_t resetImpl(ze_bool_t force, zes_reset_type_t resetType);
    bool getUuidFromSubDeviceInfo(uint32_t subDeviceID, std::array<uint8_t, NEO::ProductHelper::uuidSize> &uuid);
};

} // namespace Sysman
} // namespace L0
