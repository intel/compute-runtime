/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_cmds.h"
#include "core/helpers/options.h"
#include "runtime/helpers/array_count.h"

#include <cstddef>

namespace NEO {
// AUB file folder location
const char *folderAUB = "aub_out";

// Initial value for HW tag
uint32_t initialHardwareTag = (uint32_t)-1;

// Number of devices in the platform
static const HardwareInfo *DefaultPlatformDevices[] =
    {
        &DEFAULT_PLATFORM::hwInfo,
};

size_t numPlatformDevices = arrayCount(DefaultPlatformDevices);
const HardwareInfo **platformDevices = DefaultPlatformDevices;
} // namespace NEO
