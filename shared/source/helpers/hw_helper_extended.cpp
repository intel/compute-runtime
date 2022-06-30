/*
 * Copyright (C) 2020-2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"
#include "shared/source/helpers/hw_info.h"

namespace NEO {

uint32_t HwHelper::getCopyEnginesCount(const HardwareInfo &hwInfo) {
    return hwInfo.capabilityTable.blitterOperationsSupported ? 1 : 0;
}

} // namespace NEO
