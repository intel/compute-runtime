/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/firmware_util/igsc_wrapper.h"
#include <level_zero/zes_api.h>

#include <string>
#include <vector>

namespace L0 {
class FirmwareUtil {
  public:
    static FirmwareUtil *create(uint16_t domain, uint8_t bus, uint8_t device, uint8_t function);
    virtual ze_result_t fwDeviceInit() = 0;
    virtual ze_result_t getFirstDevice(igsc_device_info *) = 0;
    virtual ze_result_t getFwVersion(std::string fwType, std::string &firmwareVersion) = 0;
    virtual ze_result_t flashFirmware(std::string fwType, void *pImage, uint32_t size) = 0;
    virtual ze_result_t fwIfrApplied(bool &ifrStatus) = 0;
    virtual ze_result_t fwSupportedDiagTests(std::vector<std::string> &supportedDiagTests) = 0;
    virtual ze_result_t fwRunDiagTests(std::string &osDiagType, zes_diag_result_t *pDiagResult) = 0;
    virtual ze_result_t fwGetMemoryErrorCount(zes_ras_error_type_t type, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count) = 0;
    virtual ze_result_t fwGetEccConfig(uint8_t *currentState, uint8_t *pendingState) = 0;
    virtual ze_result_t fwSetEccConfig(uint8_t newState, uint8_t *currentState, uint8_t *pendingState) = 0;
    virtual void getDeviceSupportedFwTypes(std::vector<std::string> &fwTypes) = 0;
    virtual ~FirmwareUtil() = default;
};
} // namespace L0
