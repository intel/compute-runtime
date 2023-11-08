/*
 * Copyright (C) 2020-2023 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/sysman/source/api/firmware/sysman_firmware_imp.h"
#include "level_zero/sysman/source/api/firmware/sysman_os_firmware.h"
#include "level_zero/sysman/source/shared/linux/zes_os_sysman_imp.h"

namespace L0 {
namespace Sysman {

class FirmwareUtil;

class LinuxFirmwareImp : public OsFirmware, NEO::NonCopyableOrMovableClass {
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

} // namespace Sysman
} // namespace L0
