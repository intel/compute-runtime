/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "shared/source/helpers/array_count.h"
#include "shared/source/helpers/hw_cmds.h"

namespace NEO {
static const HardwareInfo *DefaultPlatformDevices[] = {
    &DEFAULT_TEST_PLATFORM::hwInfo,
};

const HardwareInfo **platformDevices = DefaultPlatformDevices;
} // namespace NEO
