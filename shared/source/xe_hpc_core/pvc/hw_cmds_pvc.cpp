/*
 * Copyright (C) 2022 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/xe_hpc_core/hw_cmds_pvc.h"

namespace NEO {
bool PVC::isXlA0(const HardwareInfo &hwInfo) {
    auto revId = hwInfo.platform.usRevId & pvcSteppingBits;
    return (revId < 0x3);
}

bool PVC::isAtMostXtA0(const HardwareInfo &hwInfo) {
    auto revId = hwInfo.platform.usRevId & pvcSteppingBits;
    return (revId <= 0x3);
}

} // namespace NEO
