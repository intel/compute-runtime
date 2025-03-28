/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "level_zero/api/sysman/zes_handles_struct.h"
#include <level_zero/zes_api.h>

#include <mutex>
#include <string>
#include <vector>

namespace L0 {
namespace Sysman {
struct OsSysman;

class Firmware : _zes_firmware_handle_t {
  public:
    virtual ~Firmware() = default;
    virtual ze_result_t firmwareGetProperties(zes_firmware_properties_t *pProperties) = 0;
    virtual ze_result_t firmwareFlash(void *pImage, uint32_t size) = 0;
    virtual ze_result_t firmwareGetFlashProgress(uint32_t *pCompletionPercent) = 0;
    virtual ze_result_t firmwareGetSecurityVersion(char *pVersion) = 0;
    virtual ze_result_t firmwareSetSecurityVersion() = 0;
    virtual ze_result_t firmwareGetConsoleLogs(size_t *pSize, char *pFirmwareLog) = 0;
    inline zes_firmware_handle_t toHandle() { return this; }

    static Firmware *fromHandle(zes_firmware_handle_t handle) {
        return static_cast<Firmware *>(handle);
    }
};

struct FirmwareHandleContext {
    FirmwareHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    MOCKABLE_VIRTUAL ~FirmwareHandleContext();
    void releaseFwHandles();

    MOCKABLE_VIRTUAL void init();

    ze_result_t firmwareGet(uint32_t *pCount, zes_firmware_handle_t *phFirmware);

    OsSysman *pOsSysman = nullptr;
    std::vector<Firmware *> handleList = {};
    bool isFirmwareInitDone() {
        return firmwareInitDone;
    }

  private:
    void createHandle(const std::string &fwType);
    std::once_flag initFirmwareOnce;
    bool firmwareInitDone = false;
};

} // namespace Sysman
} // namespace L0
