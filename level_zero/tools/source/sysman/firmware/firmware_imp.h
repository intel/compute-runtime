/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"
#include "shared/source/helpers/string.h"

#include "level_zero/tools/source/sysman/firmware/firmware.h"
#include "level_zero/tools/source/sysman/firmware/os_firmware.h"
#include <level_zero/zes_api.h>

namespace L0 {

class OsFirmware;

class FirmwareImp : public Firmware, NEO::NonCopyableAndNonMovableClass {
  public:
    ze_result_t firmwareGetProperties(zes_firmware_properties_t *pProperties) override;
    ze_result_t firmwareFlash(void *pImage, uint32_t size) override;
    ze_result_t firmwareGetFlashProgress(uint32_t *pCompletionPercent) override;
    FirmwareImp() = default;
    FirmwareImp(OsSysman *pOsSysman, const std::string &fwType);
    ~FirmwareImp() override;
    std::unique_ptr<OsFirmware> pOsFirmware = nullptr;
    std::string fwType = "Unknown";

    void init();
};

} // namespace L0
