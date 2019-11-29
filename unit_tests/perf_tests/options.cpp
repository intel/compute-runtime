/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/hw_cmds.h"
#include "runtime/helpers/array_count.h"

namespace NEO {
// IP address for TBX server
const char *tbxServerIp = "127.0.0.1";

// AUB file folder location
const char *folderAUB = "aub_out";

// Initial value for HW tag
// Set to 0 if using HW or simulator, otherwise 0xFFFFFF00, needs to be lower then Event::EventNotReady.
uint32_t initialHardwareTag = static_cast<uint32_t>(0);

// Number of devices in the platform
static const HardwareInfo *DefaultPlatformDevices[] = {
    &DEFAULT_PLATFORM::hwInfo,
};

size_t numPlatformDevices = ARRAY_COUNT(DefaultPlatformDevices);
const HardwareInfo **platformDevices = DefaultPlatformDevices;
} // namespace NEO

bool printMemoryOpCallStack = true;
