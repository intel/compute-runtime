/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

#include <mutex>
#include <string>
#include <vector>

struct _zes_firmware_handle_t {
    virtual ~_zes_firmware_handle_t() = default;
};

namespace L0 {

struct OsSysman;

class Firmware : _zes_firmware_handle_t {
  public:
    ~Firmware() override {}
    virtual ze_result_t firmwareGetProperties(zes_firmware_properties_t *pProperties) = 0;
    virtual ze_result_t firmwareFlash(void *pImage, uint32_t size) = 0;

    inline zes_firmware_handle_t toHandle() { return this; }

    static Firmware *fromHandle(zes_firmware_handle_t handle) {
        return static_cast<Firmware *>(handle);
    }
    bool isFirmwareEnabled = false;
};

struct FirmwareHandleContext {
    FirmwareHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    MOCKABLE_VIRTUAL ~FirmwareHandleContext();
    void releaseFwHandles();

    MOCKABLE_VIRTUAL void init();

    ze_result_t firmwareGet(uint32_t *pCount, zes_firmware_handle_t *phFirmware);

    OsSysman *pOsSysman = nullptr;
    std::vector<Firmware *> handleList = {};

  private:
    void createHandle(const std::string &fwType);
    std::once_flag initFirmwareOnce;
};

} // namespace L0
