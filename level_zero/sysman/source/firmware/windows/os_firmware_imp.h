/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/firmware/os_firmware.h"

namespace L0 {
namespace Sysman {
class FirmwareUtil;
class WddmFirmwareImp : public OsFirmware {
  public:
    void osGetFwProperties(zes_firmware_properties_t *pProperties) override {}
    ze_result_t osFirmwareFlash(void *pImage, uint32_t size) override;
    WddmFirmwareImp() = default;
    WddmFirmwareImp(OsSysman *pOsSysman, const std::string &fwType) {}
    ~WddmFirmwareImp() override = default;
};

} // namespace Sysman
} // namespace L0
