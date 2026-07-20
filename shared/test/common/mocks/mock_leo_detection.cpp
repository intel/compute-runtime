/*
 * Copyright (C) 2026 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/device_factory.h"
#include "shared/test/common/helpers/ult_hw_config.h"

namespace NEO {
bool isLeoDetectionEnabled() {
    return ultHwConfig.leoDetectionEnabled;
}
} // namespace NEO
