/*
 * Copyright (C) 2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/array_count.h"
#include "core/helpers/hw_cmds.h"

namespace NEO {
static const HardwareInfo *DefaultPlatformDevices[] = {
    &DEFAULT_TEST_PLATFORM::hwInfo,
};

size_t numPlatformDevices = arrayCount(DefaultPlatformDevices);
const HardwareInfo **platformDevices = DefaultPlatformDevices;
} // namespace NEO
