/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#pragma once

#include "shared/source/helpers/non_copyable_or_moveable.h"

#include "level_zero/core/source/device/device.h"
#include "level_zero/tools/source/sysman/linux/firmware_util/firmware_util_base.h"
#include "level_zero/tools/source/sysman/linux/firmware_util/firmware_util_extended.h"

#include <string>
#include <vector>

namespace L0 {
class FirmwareUtil : public virtual FirmwareUtilBase, public virtual FirmwareUtilExtended {
  public:
    static FirmwareUtil *create();
    virtual ~FirmwareUtil() = default;
};
} // namespace L0
