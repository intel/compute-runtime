/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include <level_zero/zes_api.h>

#include "firmware.h"

namespace L0 {

class OsFirmware;

class FirmwareImp : public Firmware, NEO::NonCopyableOrMovableClass {
  public:
    ze_result_t firmwareGetProperties(zes_firmware_properties_t *pProperties) override;
    FirmwareImp() = default;
    FirmwareImp(OsSysman *pOsSysman);
    ~FirmwareImp() override;
    OsFirmware *pOsFirmware = nullptr;

    void init();
};

} // namespace L0
