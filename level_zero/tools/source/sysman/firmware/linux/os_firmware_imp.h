/*
 * Copyright (C) 2020-2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/tools/source/sysman/firmware/firmware_imp.h"
#include "level_zero/tools/source/sysman/firmware/os_firmware.h"
#include "level_zero/tools/source/sysman/linux/os_sysman_imp.h"

namespace L0 {
class FirmwareUtil;

class LinuxFirmwareImp : public OsFirmware, NEO::NonCopyableAndNonMovableClass {
  public:
    void osGetFwProperties(zes_firmware_properties_t *pProperties) override;
    ze_result_t osFirmwareFlash(void *pImage, uint32_t size) override;
    ze_result_t getFirmwareVersion(std::string fwType, zes_firmware_properties_t *pProperties);
    LinuxFirmwareImp() = default;
    LinuxFirmwareImp(OsSysman *pOsSysman, const std::string &fwType);
    ~LinuxFirmwareImp() override = default;

  protected:
    FirmwareUtil *pFwInterface = nullptr;
    SysfsAccess *pSysfsAccess = nullptr;
    std::string osFwType;
};

} // namespace L0
