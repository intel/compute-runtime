/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include <level_zero/zes_api.h>

#include <vector>

struct _zes_firmware_handle_t {
    virtual ~_zes_firmware_handle_t() = default;
};

namespace L0 {

struct OsSysman;

class Firmware : _zes_firmware_handle_t {
  public:
    virtual ~Firmware() {}
    virtual ze_result_t firmwareGetProperties(zes_firmware_properties_t *pProperties) = 0;

    inline zes_firmware_handle_t toHandle() { return this; }

    static Firmware *fromHandle(zes_firmware_handle_t handle) {
        return static_cast<Firmware *>(handle);
    }
    bool isFirmwareEnabled = false;
};

struct FirmwareHandleContext {
    FirmwareHandleContext(OsSysman *pOsSysman) : pOsSysman(pOsSysman){};
    ~FirmwareHandleContext();

    void init();

    ze_result_t firmwareGet(uint32_t *pCount, zes_firmware_handle_t *phFirmware);

    OsSysman *pOsSysman = nullptr;
    std::vector<Firmware *> handleList = {};
};

} // namespace L0
