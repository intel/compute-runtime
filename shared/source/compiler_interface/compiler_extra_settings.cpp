/*
 * Copyright (C) 2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/compiler_interface/compiler_interface.h"

namespace NEO {

bool CompilerInterface::isMidThreadPreemptionSupported(const HardwareInfo &hwInfo) {
    return hwInfo.featureTable.ftrGpGpuMidThreadLevelPreempt;
}

} // namespace NEO
