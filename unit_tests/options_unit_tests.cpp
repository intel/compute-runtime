/*
 * Copyright (C) 2017-2020 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#include "core/helpers/array_count.h"
#include "core/helpers/hw_cmds.h"
#include "core/helpers/hw_info.h"
#include "core/helpers/options.h"

namespace NEO {
const char *folderAUB = "aub_out";

uint32_t initialHardwareTag = static_cast<uint32_t>(0xFFFFFF00);

static const HardwareInfo *DefaultPlatformDevices[] = {
    &DEFAULT_TEST_PLATFORM::hwInfo,
};

size_t numPlatformDevices = arrayCount(DefaultPlatformDevices);
const HardwareInfo **platformDevices = DefaultPlatformDevices;
} // namespace NEO
