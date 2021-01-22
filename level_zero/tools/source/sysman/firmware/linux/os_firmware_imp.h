/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/firmware/firmware_imp.h"
#include "sysman/firmware/os_firmware.h"
#include "sysman/linux/os_sysman_imp.h"

namespace L0 {

class LinuxFirmwareImp : public OsFirmware, NEO::NonCopyableOrMovableClass {
  public:
    bool isFirmwareSupported(void) override;
    void osGetFwProperties(zes_firmware_properties_t *pProperties) override;
    ze_result_t osFirmwareFlash(void *pImage, uint32_t size) override;
    LinuxFirmwareImp() = default;
    LinuxFirmwareImp(OsSysman *pOsSysman);
    ~LinuxFirmwareImp() override = default;

  protected:
    FirmwareUtil *pFwInterface = nullptr;
    bool isFWInitalized = false;

  private:
    void getFirmwareVersion(char *);
};

} // namespace L0
