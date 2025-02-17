/*
 * Copyright (C) 2023-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/os_interface/product_helper.h"

#include "level_zero/sysman/source/api/global_operations/sysman_os_global_operations.h"
#include "level_zero/sysman/source/shared/windows/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {
class KmdSysManager;
class WddmGlobalOperationsImp : public OsGlobalOperations, NEO::NonCopyableAndNonMovableClass {
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
    ze_result_t getSubDeviceProperties(uint32_t *pCount, zes_subdevice_exp_properties_t *pSubdeviceProps) override;
    WddmGlobalOperationsImp(OsSysman *pOsSysman);
    WddmGlobalOperationsImp() = default;
    ~WddmGlobalOperationsImp() override = default;
    struct {
        bool isValid = false;
        std::array<uint8_t, NEO::ProductHelper::uuidSize> id;
    } uuid;

  protected:
    WddmSysmanImp *pWddmSysmanImp = nullptr;
    KmdSysManager *pKmdSysManager = nullptr;
};

} // namespace Sysman
} // namespace L0
