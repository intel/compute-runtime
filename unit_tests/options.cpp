/*
 * Copyright (C) 2017-2019 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "runtime/helpers/options.h"

#include "core/helpers/hw_cmds.h"
#include "core/helpers/hw_info.h"
#include "runtime/helpers/array_count.h"

namespace NEO {
const char *folderAUB = "aub_out";

uint32_t initialHardwareTag = static_cast<uint32_t>(0xFFFFFF00);

static const HardwareInfo *DefaultPlatformDevices[] = {
    &DEFAULT_TEST_PLATFORM::hwInfo,
};

size_t numPlatformDevices = arrayCount(DefaultPlatformDevices);
const HardwareInfo **platformDevices = DefaultPlatformDevices;
} // namespace NEO
