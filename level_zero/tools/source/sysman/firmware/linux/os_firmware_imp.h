/*
 * Copyright (C) 2020 Intel Corporation
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

    LinuxFirmwareImp() = default;
    LinuxFirmwareImp(OsSysman *pOsSysman);
    ~LinuxFirmwareImp() override = default;
};

} // namespace L0
