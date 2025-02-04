/*
 * Copyright (C) 2025 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/os_interface/linux/xe/eudebug/eudebug_interface_prelim.h"

namespace NEO {
bool EuDebugInterfacePrelim::isExecQueuePageFaultEnableSupported() {
    return false;
}

uint32_t EuDebugInterfacePrelim::getAdditionalParamValue(EuDebugParam param) const {
    return 0;
}

} // namespace NEO