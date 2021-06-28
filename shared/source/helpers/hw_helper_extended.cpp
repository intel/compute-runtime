/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/hw_helper.h"

namespace NEO {

uint32_t HwHelper::getCopyEnginesCount(const HardwareInfo &hwInfo) {
    return hwInfo.capabilityTable.blitterOperationsSupported ? 1 : 0;
}

} // namespace NEO
