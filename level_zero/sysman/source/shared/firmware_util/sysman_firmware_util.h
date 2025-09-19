/*
 * Copyright (C) 2022-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/shared/firmware_util/sysman_igsc_wrapper.h"
#include <level_zero/zes_api.h>

#include <string>
#include <vector>

namespace L0 {
namespace Sysman {
constexpr uint8_t maxGfspHeciOutBuffer = UINT8_MAX;
constexpr uint8_t maxGfspHeciInBuffer = 4;
static std::vector<std ::string> lateBindingFirmwareTypes = {"FanTable", "VRConfig"};

namespace GfspHeciConstants {
enum Cmd {
    setConfigurationCmd8 = 0x8,
    getConfigurationCmd9 = 0x9,
    setConfigurationCmd15 = 0xf,
    getConfigurationCmd16 = 0x10
};

enum SetCmd15BytePostition {
    request = 0,
    response = 0
};

enum SetEccCmd8BytePostition {
    setRequest = 0,
    responseCurrentState = 0,
    responsePendingState = 1
};

enum GetCmd16BytePostition {
    availableBytePosition = 0,
    currentStateBytePosition = 4,
    configurableBytePosition = 8,
    pendingStateBytePosition = 12,
    defaultStateBytePosition = 16
};

enum GetEccCmd9BytePostition {
    currentState = 0,
    pendingState = 1
};
} // namespace GfspHeciConstants

class FirmwareUtil {
  public:
    static FirmwareUtil *create(uint16_t domain, uint8_t bus, uint8_t device, uint8_t function);
    virtual ze_result_t fwDeviceInit() = 0;
    virtual ze_result_t getFwVersion(std::string fwType, std::string &firmwareVersion) = 0;
    virtual ze_result_t flashFirmware(std::string fwType, void *pImage, uint32_t size) = 0;
    virtual ze_result_t getFlashFirmwareProgress(uint32_t *pCompletionPercent) = 0;
    virtual ze_result_t fwIfrApplied(bool &ifrStatus) = 0;
    virtual ze_result_t fwSupportedDiagTests(std::vector<std::string> &supportedDiagTests) = 0;
    virtual ze_result_t fwRunDiagTests(std::string &osDiagType, zes_diag_result_t *pDiagResult) = 0;
    virtual ze_result_t fwGetMemoryErrorCount(zes_ras_error_type_t type, uint32_t subDeviceCount, uint32_t subDeviceId, uint64_t &count) = 0;
    virtual ze_result_t fwGetEccAvailable(ze_bool_t *pAvailable) = 0;
    virtual ze_result_t fwGetEccConfigurable(ze_bool_t *pConfigurable) = 0;
    virtual ze_result_t fwGetEccConfig(uint8_t *currentState, uint8_t *pendingState, uint8_t *defaultState) = 0;
    virtual ze_result_t fwSetEccConfig(uint8_t newState, uint8_t *currentState, uint8_t *pendingState) = 0;
    virtual ze_result_t fwSetGfspConfig(uint32_t gfspHeciCmdCode, std::vector<uint8_t> inBuf, std::vector<uint8_t> &outBuf) = 0;
    virtual ze_result_t fwGetGfspConfig(uint32_t gfspHeciCmdCode, std::vector<uint8_t> &outBuf) = 0;
    virtual void getDeviceSupportedFwTypes(std::vector<std::string> &fwTypes) = 0;
    virtual void fwGetMemoryHealthIndicator(zes_mem_health_t *health) = 0;
    virtual void getLateBindingSupportedFwTypes(std::vector<std::string> &fwTypes) = 0;
    virtual ~FirmwareUtil() = default;
};
} // namespace Sysman
} // namespace L0
