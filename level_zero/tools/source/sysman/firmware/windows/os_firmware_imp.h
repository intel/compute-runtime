/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once
#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "sysman/firmware/os_firmware.h"
#include "sysman/windows/os_sysman_imp.h"

namespace L0 {
class WddmFirmwareImp : public OsFirmware {
  public:
    bool isFirmwareSupported(void) override;
};

} // namespace L0
