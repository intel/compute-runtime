/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "helpers/array_count.h"
#include "helpers/hw_cmds.h"

namespace NEO {
static const HardwareInfo *DefaultPlatformDevices[] = {
    &DEFAULT_TEST_PLATFORM::hwInfo,
};

const HardwareInfo **platformDevices = DefaultPlatformDevices;
} // namespace NEO
